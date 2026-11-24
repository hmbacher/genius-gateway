/**
 * @file GeniusDevicesService.h
 * @brief Gateway devices service for managing genius smoke detector devices
 *
 * @copyright Copyright (c) 2024-2025 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the Commons Clause restriction.
 *
 * "Commons Clause" License Condition v1.0
 * The Software is provided to you by the Licensor under the License,
 * as defined below, subject to the following condition:
 * Without limiting other conditions in the License, the grant of rights
 * under the License will not include, and the License does not grant to you,
 * the right to Sell the Software.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#ifndef GeniusDevicesService_h
#define GeniusDevicesService_h

#include <atomic>
#include <map>
#include <vector>
#include <EventSocket.h>
#include <FSPersistence.h>
#include <HttpEndpoint.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <ESP32SvelteKit.h>
#include <Utils.hpp>
#include <PsramAllocator.h>
#include <AlarmPublishingSettingsService.h>
#include <GeniusDeviceStatus.h>
#include <HomeAssistant/HAService.h>
#include <HomeAssistant/HADevice.h>
#include <HomeAssistant/HABinarySensor.h>
#include <HomeAssistant/HAGroupedSensorPublisher.h>
#include <HomeAssistant/HASettingsService.h>
#include <PsychicMqttClient.h>

#define GATEWAY_DEVICES_FILE "/config/gateway-devices.json"  ///< Configuration file path for device data
#define GATEWAY_DEVICES_SERVICE_PATH "/rest/gateway-devices"  ///< REST API service endpoint path (bulk GET/POST)

// Per-device CRUD endpoints (keep request bodies small regardless of total device count)
#define GATEWAY_DEVICES_DEVICE_PATH         "/rest/gateway-devices/device"          ///< PUT: upsert single device
#define GATEWAY_DEVICES_DEVICE_DELETE_PATH  "/rest/gateway-devices/device/delete"   ///< POST: delete single device (body: { id })
#define GATEWAY_DEVICES_REORDER_PATH        "/rest/gateway-devices/reorder"         ///< POST: reorder (body: { order: [id...] })
#define GATEWAY_DEVICES_RESET_STATUS_PATH   "/rest/gateway-devices/device/reset-status" ///< POST: clear a device's status flags (body: { id })
#define GATEWAY_DEVICES_RUNTIME_PATH        "/rest/gateway-devices/runtime"         ///< GET: what was heard of the supervised radio modules since the gateway started

// Chunked import endpoints (let arbitrarily large device lists land without holding the whole payload in heap)
#define GATEWAY_DEVICES_IMPORT_BEGIN_PATH   "/rest/gateway-devices/import/begin"
#define GATEWAY_DEVICES_IMPORT_CHUNK_PATH   "/rest/gateway-devices/import/chunk"
#define GATEWAY_DEVICES_IMPORT_COMMIT_PATH  "/rest/gateway-devices/import/commit"
#define GATEWAY_DEVICES_IMPORT_ABORT_PATH   "/rest/gateway-devices/import/abort"

#define GATEWAY_DEVICES_IMPORT_SESSION_TTL_S 60  ///< Idle-expiry for a chunked-import session

#ifndef GATEWAY_MAX_DEVICES
#define GATEWAY_MAX_DEVICES 50   ///< Maximum number of devices supported (override via build flag for no-PSRAM boards)
#endif

#define ALARM_STATE_CHANGE "alarm-state-change"  ///< WebSocket event for alarm state changes
#define DEVICE_STATUS_CHANGE "device-status-change"  ///< Update origin for status flag changes (received over the air or reset manually)

#define GENIUS_DEVICE_ADDED_FROM_PACKET "genius-device-added-from-packet"  ///< Event for device discovery
#define RSSI_SURVEY_UPDATE "rssi-survey-update"  ///< Update origin for a ConfigCheckProbe RSSI write (persist only; no HA/sub-device sync)
#define HA_ENTITY_ID_PREFIX_UPDATE "ha-entity-id-prefix-update"  ///< Update origin for storing the entity-ID prefix announced to HA (persist only; no HA/sub-device sync)
#define GENIUS_DEVICE_DEFAULT_LOCATION "Unknown location"  ///< Default location for new devices

#define HA_ENTITY_RENAME_EVENT "ha-entity-rename"  ///< WebSocket event reporting the progress of a Home Assistant entity-ID rename
#define DEVICE_IMPORT_EVENT "device-import"        ///< WebSocket event reporting what the gateway does after an import was committed
#define DEVICE_RUNTIME_EVENT "device-runtime"      ///< WebSocket event: last seen / signal of radio modules changed (RAM-only values)
#define HA_ENTITY_RENAME_SETTLE_MS 3000            ///< Pause between removing and re-publishing entities, so HA finishes the removal first
#define HA_ENTITY_RENAME_EVENT_MAX_NAMES 3         ///< Detectors named by location in a HA_ENTITY_RENAME_EVENT; the count covers the rest
#define HA_RENAME_NOTIFY_ANNOUNCED 0x01            ///< Rename task notification: a cycle whose progress is shown in the web interface
#define HA_RENAME_NOTIFY_BACKGROUND 0x02           ///< Rename task notification: a cycle caused by an import, run without progress

#define GATEWAY_DEVICES_CONFIG_VERSION 3  ///< Current config file version (for JSON migration support)

typedef enum genius_alarm_ending
{
    GAE_MIN = -2,              ///< Minimum value (for enum range checks)
    GAE_ALARM_ACTIVE = -1,     ///< Alarm is currently active
    GAE_BY_SMOKE_DETECTOR = 0, ///< Alarm was ended by smoke detector
    GAE_BY_MANUAL,             ///< Alarm was ended manually via web interface
    GAE_BY_IMPORT,             ///< Alarm was cleared during a device config import
    GAE_MAX                    ///< Maximum value (for enum range checks)
} genius_alarm_ending_t;


typedef enum genius_smoke_detector
{
    GSD_UNKNOWN = -1,    ///< Unknown smoke detector type
    GSD_GENIUS_H = 0,    ///< Genius H smoke detector
    GSD_GENIUS_HX = 1,   ///< Genius Hx smoke detector
    GSD_GENIUS_PLUS = 2, ///< Genius Plus smoke detector
    GSD_GENIUS_PLUS_X = 3, ///< Genius Plus X smoke detector
} GeniusSmokeDetector;

typedef enum genius_radio_module
{
    GRM_UNKNOWN = -1,   ///< Unknown radio module type
    GRM_NONE = 0,       ///< No FM radio module
    GRM_FM_BASIS = 1,   ///< FM Basis radio module
    GRM_FM_PRO = 2,     ///< FM Pro radio module
    GRM_FM_MCP = 3,     ///< FM MCP radio module
    GRM_FM_BASIS_X = 4, ///< FM Basis X radio module
    GRM_FM_PRO_X = 5,   ///< FM Pro X radio module
} GeniusRadioModule;

/// True for the old-generation FM modules (FM.Basis / FM.Pro) that cannot expose
/// their alarm line via the device - the line must be entered manually, mirroring
/// the official app's SmartSonicValidationService.isOldFm().
static inline bool isOldFmModule(GeniusRadioModule model)
{
    return model == GRM_FM_BASIS || model == GRM_FM_PRO;
}

/// True iff the alarm line for this module must be entered by hand (old FM modules).
static inline bool isManualLineEntryRequired(GeniusRadioModule model)
{
    return isOldFmModule(model);
}

/// Maps acoustic protocol product type byte directly to GeniusSmokeDetector enum
/// Acoustic protocol: 0=Genius H, 1=Genius Hx, 2=Genius Plus, 3=Genius Plus X
static inline GeniusSmokeDetector acousticProdTypeToSmokeDetector(uint8_t prodType)
{
    if (prodType <= 3) return static_cast<GeniusSmokeDetector>(prodType);
    return GSD_UNKNOWN;
}

/// Maps acoustic protocol radio product byte directly to GeniusRadioModule enum
/// Acoustic protocol: 0=no FM, 1=FM.Basis, 2=FM.Pro, 3=FM.MCP, 4=FM.Basis X, 5=FM.Pro X
static inline GeniusRadioModule acousticRadioProdToModule(uint8_t radioProd)
{
    if (radioProd <= 5) return static_cast<GeniusRadioModule>(radioProd);
    return GRM_UNKNOWN;
}

/// Smoke detector identity and status data
struct GeniusSmokeDetectorInfo
{
    // --- Identity (available from all registration types) ---
    GeniusSmokeDetector model;  ///< Smoke detector model type
    uint32_t sn;                ///< Smoke detector serial number
    time_t productionDate;      ///< Production date (Unix timestamp, 0 if unknown)

    // --- Status (populated from SmartSonic readout, zero-initialized otherwise) ---
    time_t lastSelftest;           ///< Last successful self-test timestamp (0 if unknown)
    time_t lastAlarm;              ///< Last alarm timestamp (0 if no alarm)
    uint8_t deinstallationCount;   ///< Lifetime deinstallation count
    uint8_t alarmCountTotal;       ///< Lifetime alarm count
    uint8_t alarmCountLast3Months; ///< Alarm count in last 3 months
    uint16_t hoursInStorageMode;   ///< Hours spent in storage mode
    uint16_t warrantyFlags;        ///< Warranty condition flags bitmask
    bool batteryLowFault;          ///< Battery low fault flag - readout input only, see GeniusDevice::applyReadoutStatus()
    bool deviceFault;              ///< Device fault flag - readout input only
    uint8_t driftState;            ///< Drift state (0=Normal ... 7=Defekt)
    bool dirtForecastNegative;     ///< Negative dirt forecast flag

    void toJson(JsonObject &root) const
    {
        if (static_cast<int>(model) != -1)
            root["model"] = static_cast<int>(model);
        root["sn"] = sn;
        if (productionDate > 0)
            root["productionDate"] = Utils::time_t_to_iso8601(productionDate);
        if (lastSelftest > 0)
            root["lastSelftest"] = Utils::time_t_to_iso8601(lastSelftest);
        if (lastAlarm > 0)
            root["lastAlarm"] = Utils::time_t_to_iso8601(lastAlarm);
        if (deinstallationCount) root["deinstallationCount"] = deinstallationCount;
        if (alarmCountTotal)     root["alarmCountTotal"] = alarmCountTotal;
        if (alarmCountLast3Months) root["alarmCountLast3Months"] = alarmCountLast3Months;
        if (hoursInStorageMode)  root["hoursInStorageMode"] = hoursInStorageMode;
        if (warrantyFlags)       root["warrantyFlags"] = warrantyFlags;
        if (batteryLowFault)     root["batteryLowFault"] = batteryLowFault;
        if (deviceFault)         root["deviceFault"] = deviceFault;
        if (driftState)          root["driftState"] = driftState;
        if (dirtForecastNegative) root["dirtForecastNegative"] = dirtForecastNegative;
    }

    static GeniusSmokeDetectorInfo fromJson(JsonObject root)
    {
        GeniusSmokeDetectorInfo d = {};
        d.model = root["model"].is<int>() ? static_cast<GeniusSmokeDetector>(root["model"].as<int>()) : GSD_UNKNOWN;
        d.sn = root["sn"].as<uint32_t>();
        d.productionDate = root["productionDate"].is<String>() ? Utils::iso8601_to_time_t(root["productionDate"].as<String>()) : 0;
        d.lastSelftest = root["lastSelftest"].is<String>() ? Utils::iso8601_to_time_t(root["lastSelftest"].as<String>()) : 0;
        d.lastAlarm = root["lastAlarm"].is<String>() ? Utils::iso8601_to_time_t(root["lastAlarm"].as<String>()) : 0;
        d.deinstallationCount = root["deinstallationCount"].as<uint8_t>();
        d.alarmCountTotal = root["alarmCountTotal"].as<uint8_t>();
        d.alarmCountLast3Months = root["alarmCountLast3Months"].as<uint8_t>();
        d.hoursInStorageMode = root["hoursInStorageMode"].as<uint16_t>();
        d.warrantyFlags = root["warrantyFlags"].as<uint16_t>();
        d.batteryLowFault = root["batteryLowFault"].as<bool>();
        d.deviceFault = root["deviceFault"].as<bool>();
        d.driftState = root["driftState"].as<uint8_t>();
        d.dirtForecastNegative = root["dirtForecastNegative"].as<bool>();
        return d;
    }
};

/// Radio module identity and status data
struct GeniusRadioModuleInfo
{
    // --- Identity (available from all registration types) ---
    GeniusRadioModule model;  ///< Radio module model type
    uint32_t sn;              ///< Radio module serial number

    // --- Status (populated from SmartSonic readout, zero-initialized otherwise) ---
    uint32_t lineId;          ///< FM line identifier
    char lineCharacter;       ///< FM line character ('A'-'J', 0 if unknown)
    uint8_t lineNumber;       ///< FM line number
    uint8_t radioStateMask;   ///< Radio state flags bitmask
    uint8_t radioSwitchMask;  ///< Radio switch flags bitmask
    float radioInterference;  ///< Radio interference level (0.0-100.0%)
    bool radioNetworkFault;   ///< Radio network fault flag - readout input only, see GeniusDevice::applyReadoutStatus()
    bool lineManual;          ///< True if the alarm line was entered by hand (old FM modules)

    // --- Direct-link signal (populated by the ConfigCheckProbe range test) ---
    // lastRangeTest is stamped for EVERY known module each survey (reached or not); rssi carries the
    // measured dBm when the module answered, or 0 as the "tested but no response" sentinel. A real
    // 868 MHz direct-link RSSI is always negative (dBm ∈ [-138, -10]), so 0 never collides with a
    // genuine reading - that lets the UI tell "out of range" (rssi 0) from "never tested" (unset).
    int8_t rssi;              ///< Direct-link RSSI in dBm when reached; 0 = tested but no response
    time_t lastRangeTest;     ///< Timestamp of the last range test this module was included in (0 = never tested)

    void toJson(JsonObject &root) const
    {
        if (static_cast<int>(model) != -1)
            root["model"] = static_cast<int>(model);
        root["sn"] = sn;
        root["lineId"] = lineId;
        if (lineCharacter)    root["lineCharacter"] = String(lineCharacter);
        root["lineNumber"] = lineNumber;
        if (radioStateMask)   root["radioStateMask"] = radioStateMask;
        if (radioSwitchMask)  root["radioSwitchMask"] = radioSwitchMask;
        root["radioInterference"] = radioInterference;
        if (radioNetworkFault) root["radioNetworkFault"] = radioNetworkFault;
        if (lineManual)       root["lineManual"] = true;
        if (lastRangeTest > 0)
        {
            root["rssi"] = rssi;
            root["lastRangeTest"] = Utils::time_t_to_iso8601(lastRangeTest);
        }
    }

    static GeniusRadioModuleInfo fromJson(JsonObject root)
    {
        GeniusRadioModuleInfo d = {};
        d.model = root["model"].is<int>() ? static_cast<GeniusRadioModule>(root["model"].as<int>()) : GRM_UNKNOWN;
        d.sn = root["sn"].as<uint32_t>();
        d.lineId = root["lineId"].as<uint32_t>();
        String lineCharStr = root["lineCharacter"].is<String>() ? root["lineCharacter"].as<String>() : String();
        char lc = lineCharStr.length() > 0 ? lineCharStr[0] : 0;
        d.lineCharacter = (lc >= 'A' && lc <= 'J') ? lc : 0;  ///< Reject out-of-range values (e.g. ArduinoJson "null" artifact)
        d.lineNumber = root["lineNumber"].as<uint8_t>();
        d.radioStateMask = root["radioStateMask"].as<uint8_t>();
        d.radioSwitchMask = root["radioSwitchMask"].as<uint8_t>();
        d.radioInterference = max(0.0f, root["radioInterference"].as<float>());
        d.radioNetworkFault = root["radioNetworkFault"].as<bool>();
        d.lineManual = root["lineManual"].as<bool>();
        d.rssi = static_cast<int8_t>(root["rssi"].as<int>());
        d.lastRangeTest = root["lastRangeTest"].is<String>() ? Utils::iso8601_to_time_t(root["lastRangeTest"].as<String>()) : 0;
        return d;
    }
};

typedef enum genius_device_registration
{
    GDR_MIN = -1,      ///< Boundary check minimum value
    GDR_BUILT_IN = 0,  ///< Device is built-in
    GDR_GENIUS_PACKET, ///< Device was added via received genius packet
    GDR_MANUAL,        ///< Device registered manually (via web interface)
    GDR_ACOUSTIC,      ///< Device identified via acoustic (smartsonic) readout
    GDR_MAX            ///< Boundary check maximum value
} genius_device_registration_t;

/// Class for Genius devices
class GeniusDevice
{
public:
    GeniusDevice(const GeniusSmokeDetectorInfo &smokeDetector,
                 const GeniusRadioModuleInfo &radioModule,
                 const String &location,
                 uint32_t id = 0) : smokeDetector(smokeDetector),
                                   radioModule(radioModule),
                                   location(location),
                                   id(id),
                                   registration(GDR_MANUAL),
                                   isAlarming(false),
                                   published(false),
                                   readoutTime(0),
                                   readoutProtocolVersion(0)
    {
    }

    void toJson(JsonObject &root)
    {
        root["id"] = this->id;
        JsonObject sdJson = root["smokeDetector"].to<JsonObject>();
        this->smokeDetector.toJson(sdJson);
        JsonObject rmJson = root["radioModule"].to<JsonObject>();
        this->radioModule.toJson(rmJson);
        root["location"] = this->location;
        root["isAlarming"] = this->isAlarming;
        root["registration"] = this->registration;
        root["alarmCount"] = this->alarmCount;
        if (readoutTime > 0)
        {
            root["readoutTime"] = Utils::time_t_to_iso8601(readoutTime);
            root["readoutProtocolVersion"] = readoutProtocolVersion;
        }
        if (!haEntityIdPrefix.isEmpty())
            root["haEntityIdPrefix"] = haEntityIdPrefix;
        statusToJson(root);
    }

    uint32_t id;                          ///< Unique identifier for device (auto-generated, immutable)
    GeniusSmokeDetectorInfo smokeDetector;
    GeniusRadioModuleInfo radioModule;
    String location;
    /// Number of alarms of this smoke detector. Counted separately, as the alarms themselves are
    /// entries of the activity log (status.events), which only keeps the newest ones.
    uint32_t alarmCount = 0;
    genius_device_registration_t registration;
    bool isAlarming;
    bool published;                       ///< Whether current device config has been published via MQTT
    time_t readoutTime;                   ///< SmartSonic readout timestamp (0 = no readout performed)
    uint8_t readoutProtocolVersion;       ///< SmartSonic protocol version (valid when readoutTime > 0)

    /**
     * @brief Entity-ID prefix last announced to Home Assistant (empty = none announced)
     *
     * Backend-owned bookkeeping for the "rename entity IDs" option: a rename is
     * pending while this differs from the prefix the detector should have. It
     * is persisted so a rename that could not be sent survives a reboot, shows
     * up read-only in the REST output and is never taken from a client - only
     * from the config file.
     */
    String haEntityIdPrefix;

    /// Current status (state per flag) and activity log - see GeniusDeviceStatus.h. Backend-owned:
    /// fed by status changes received over the air, acoustic readouts and manual resets.
    GeniusDeviceStatus status;

    /// Write the "status" and "events" arrays (each omitted while empty).
    void statusToJson(JsonObject &root) const;

    /// Read the "status" and "events" arrays written by statusToJson(). Returns false (status
    /// untouched) if neither is present.
    bool statusFromJson(JsonVariant src);

    /// Take battery / detector fault / radio fault over from the acoustic readout and log the
    /// readout - called when a new readout arrives, or to derive the status of a device that
    /// has none stored yet. Returns true if a flag changed.
    bool applyReadoutStatus();

    /// Append an entry with the current time to the activity log.
    void logEvent(genius_device_event_t type, uint32_t value = 0);

    /// Take over the "alarms" list of a file written before v1.6.0: every alarm becomes an
    /// activity log entry and counts towards alarmCount.
    void importLegacyAlarms(JsonArray jsonAlarms);

    /// Another smoke detector took the place of this entry (its serial changed): status,
    /// activity log and alarm counter start over.
    void detectorReplaced();

    /// Build a fresh GeniusDevice from a JSON object. Returns false if `id` is missing or zero.
    static bool buildFromJson(JsonVariant src, GeniusDevice &out);

    /// Apply incoming JSON onto an existing device. Returns true if any field changed.
    /// Mirrors the merge semantics of the bulk-POST path (readout merge, identity update, alarms diff).
    bool mergeFromJson(JsonVariant src);
};

class GeniusDevices
{

public:
    static constexpr const char *TAG = "GeniusDevices";

    std::vector<GeniusDevice, PsramAllocator<GeniusDevice>> devices;
    std::vector<uint32_t> deletedDeviceIds; ///< Temporary storage for deleted device IDs (populated during update)

    static void read(GeniusDevices &geniusDevices, JsonObject &root)
    {
        root["version"] = GATEWAY_DEVICES_CONFIG_VERSION;
        JsonArray jsonDevices = root["devices"].to<JsonArray>();
        for (auto &device : geniusDevices.devices)
        {
            JsonObject jsonDevice = jsonDevices.add<JsonObject>();
            device.toJson(jsonDevice);
        }

        ESP_LOGV(GeniusDevices::TAG, "Smoke detector devices configurations read.");
    }

    /// Update genius devices from JSON object
    static StateUpdateResult update(JsonObject &root, GeniusDevices &geniusDevices, const String &originId);
};

/// Service for managing gateway devices and smoke detector communication
class GeniusDevicesService : public StatefulService<GeniusDevices>
{
public:
    static constexpr const char *TAG = "GeniusDevicesService"; ///< Logging tag

    GeniusDevicesService(ESP32SvelteKit *sveltekit, PsychicMqttClient *mqttClient, AlarmPublishingSettingsService *alarmPublishingSettings);

    /// Initialize the gateway devices service
    void begin();

    /// Add a genius device based on radio module and smoke detector serial numbers
    bool AddGeniusDevice(const uint32_t snRadioModule,
                         const uint32_t snSmokeDetector);

    /// Set alarm state for a device by detector serial number
    const GeniusDevice *setAlarm(uint32_t detectorSN);

    /// Reset alarm state for a device with specified ending reason
    const GeniusDevice *resetAlarm(uint32_t detectorSN, genius_alarm_ending_t endingReason);

    /// Reset all active alarms
    bool resetAllAlarms();

    /// Check if any device is currently alarming
    bool isAlarming();

    /// Get the number of devices currently alarming
    uint32_t numAlarmingDevices();

    /**
     * @brief Apply a received status change to the device with smoke detector serial @p detectorSN.
     *
     * Flags in @p setMask become active, flags in @p clearedMask inactive; both become known.
     * Idempotent - repeated copies of the same change (mesh forwarding) leave the device untouched.
     * On an actual change the device is persisted, its HA status entities are published and the
     * update handlers run with origin DEVICE_STATUS_CHANGE.
     *
     * @param detectorSN  Smoke detector serial number
     * @param setMask     Status flags newly set (alarm bit ignored)
     * @param clearedMask Status flags newly cleared (alarm bit ignored)
     * @return The updated device, or nullptr if nothing changed or the detector is unknown
     */
    const GeniusDevice *applyStatusChange(uint32_t detectorSN, uint32_t setMask, uint32_t clearedMask);

    /**
     * @brief A radio module reports another one as missing (radio link supervision)
     *
     * The reporting detector gets an activity log entry naming the missing module. The
     * detector with the missing module - if it is registered - gets the status flag
     * GENIUS_STATUS_REPORTED_MISSING, which counts as a fault. Like every status flag it
     * stays until something reports otherwise: here until a frame of that module is heard
     * again (moduleHeard()) or the status is reset by hand.
     *
     * @param reporterModuleSN Serial of the radio module that sent the report
     * @param missingModuleSN  Serial of the radio module it reports as missing
     */
    void applyModuleMissing(uint32_t reporterModuleSN, uint32_t missingModuleSN);

    /**
     * @brief A frame of these radio modules was received
     *
     * Called from the RX path for every frame. Two things follow from it:
     * - "reported missing" ends for a detector whose radio module originated or
     *   transmitted the frame (no mutex is taken while no detector is reported missing)
     * - for FM Pro X modules - the ones that send a daily supervision series - the time
     *   is kept as "last seen", and for the transmitting module the signal strength.
     *   These values live in RAM only, behind a mutex of their own that is never held
     *   during file or network operations.
     *
     * @param originModuleSN Serial of the radio module that originated the frame
     * @param senderModuleSN Serial of the radio module that transmitted it (differs when relayed)
     * @param rssiDbm        Signal strength of the frame, i.e. of the transmitting module
     * @return true if a last seen / signal value changed (the web clients should be told)
     */
    bool moduleHeard(uint32_t originModuleSN, uint32_t senderModuleSN, int8_t rssiDbm);

    /**
     * @brief Last seen / signal of the supervised radio modules as JSON
     * @param root        Receives `devices`: [{ id, lastSeen, rssi?, rssiTime? }]
     * @param changedOnly Only the entries that changed since they were last sent with this flag
     */
    void runtimeToJson(JsonObject root, bool changedOnly);

    /// Send DEVICE_RUNTIME_EVENT with the entries that changed. Not for the RX task.
    void emitRuntimeChanges();

    /// Number of devices with at least one active fault (GENIUS_STATUS_FAULT_MASK)
    uint32_t numFaultyDevices();

    /// Number of devices reporting a low battery
    uint32_t numBatteryLowDevices();

    /// Check if a smoke detector is known/registered
    bool isSmokeDetectorKnown(uint32_t detectorSN);

    /// Check if a radio module (by its serial) is registered on any device.
    bool isRadioModuleKnown(uint32_t radioModuleSN);

    /**
     * @brief Update the stored direct-link RSSI of the device whose radio module has serial
     *        @p radioModuleSN (in RAM only - does NOT persist; call persistRssiSurvey() once
     *        after a batch of updates).
     *
     * Used by the ConfigCheckProbe RSSI survey: each 0x08 response carries the responder's
     * radio-module serial (Org-SN) and is heard with a measured RSSI.
     *
     * @param radioModuleSN Radio module serial number (packet Org-SN)
     * @param rssi Measured RSSI in dBm
     * @param when Measurement timestamp
     * @return true if a registered device matched and was updated; false if unknown module
     */
    bool updateRadioModuleRssi(uint32_t radioModuleSN, int8_t rssi, time_t when);

    /**
     * @brief Stamp every known radio module NOT in @p reached as "range-tested, no response":
     *        rssi = 0 (sentinel), lastRangeTest = @p when. RAM only - call persistRssiSurvey()
     *        afterwards. Devices without a radio module (sn == 0) are skipped.
     *
     * Lets the UI distinguish "out of range this survey" (tested, rssi 0) from "never tested"
     * (lastRangeTest unset). Runs for every survey, so a sweep with zero responders still records
     * that all known modules were probed.
     *
     * @param reached Serial numbers that answered (already stamped via updateRadioModuleRssi)
     * @param when Survey timestamp
     * @return number of modules stamped as unreached
     */
    uint32_t markRadioModulesUnreached(const std::vector<uint32_t> &reached, time_t when);

    /// Persist the device list once after a batch of range-test updates (single FS write).
    void persistRssiSurvey();

    /**
     * @brief Synchronize HA sub-devices with current device list.
     *
     * Adds sub-devices for any unregistered smoke detectors, removes sub-devices
     * for deleted ones. HAService handles config and state republishing on
     * MQTT (re)connect - this function only needs to be called when the device
     * list changes.
     *
     * Detectors whose Home Assistant entity IDs have to be renamed are handed to
     * the rename task; this call returns without waiting for it.
     *
     * @param onlyUnpublished Ignored - sync is always idempotent (kept for API compatibility)
     */
    void mqttPublishAllDevices(bool onlyUnpublished = true);

    /// Publish global alarm state to the simple alarm topic (independent of HA integration)
    void mqttPublishSimpleAlarmState();

    /// Publish the smoke detectors' status summary to the simple fault topic (independent of HA integration)
    void mqttPublishSimpleFaultState();

    /**
     * @brief Re-publish alarm state for all devices.
     *
     * Calls publishState() on each smoke-alarm sensor. Used after bulk alarm resets.
     */
    void mqttPublishAllDevicesState(bool onlyUnpublished = true);

    /**
     * @brief Publish alarm state for a single device (by serial number).
     *
     * Calls sensor->publishState() - the sensor getter reads device.isAlarming live.
     *
     * @param smokeDetectorSN Smoke detector serial number
     * @param useTransaction If true, wraps access in transaction
     * @param markPublished If true, marks device.published=true on success
     * @return ESP_OK on success, ESP_ERR_NOT_FOUND if device not found
     */
    esp_err_t mqttPublishDeviceState(uint32_t smokeDetectorSN, bool useTransaction = true, bool markPublished = true);

    // ========================================================================
    // Per-Device CRUD (small-body endpoints - replace the bulk POST for UI ops)
    // ========================================================================

    /// Outcome of a per-device CRUD operation. ERROR codes are mapped to HTTP status by the endpoint layer.
    enum class DeviceOpResult
    {
        OK,
        INVALID_BODY,    ///< Missing/zero id, malformed payload
        NOT_FOUND,       ///< No device with that id
        LIMIT_REACHED,   ///< Would exceed GATEWAY_MAX_DEVICES
        UNCHANGED,       ///< Upsert matched an identical existing device
    };

    /// Create or update a single device by id. The id MUST be set by the client.
    /// On success, the persisted device is written to `out` (with current state, e.g. preserved id/alarms).
    DeviceOpResult upsertDevice(JsonVariant deviceJson, GeniusDevice &out);

    /// Delete a single device by id.
    DeviceOpResult removeDevice(uint32_t deviceId);

    /// Manually clear all status flags of a device (e.g. after the fault was fixed or the device
    /// replaced). Flags stay known, so they read as OK afterwards. UNCHANGED if none was active.
    /// On success, the updated device is written to `out`.
    DeviceOpResult resetStatus(uint32_t deviceId, GeniusDevice &out);

    /// Reorder devices to match the given id list. The list MUST contain exactly the
    /// current device id set (no additions, no removals) - otherwise INVALID_BODY.
    DeviceOpResult reorderDevices(const std::vector<uint32_t> &newOrder);

    // ========================================================================
    // Chunked Import (single global slot - only one admin can import at a time)
    // ========================================================================

    enum class ImportResult
    {
        OK,
        BUSY,            ///< Another import session is already in progress
        BAD_SESSION,     ///< Token missing/expired/unknown
        BAD_PAYLOAD,     ///< Missing required field or malformed
        LIMIT_REACHED,   ///< Staging would exceed GATEWAY_MAX_DEVICES
    };

    /// Start a new import session. Fails with BUSY if another non-stale session exists.
    /// On success, writes the session token to `tokenOut`.
    ImportResult importBegin(String &tokenOut);

    /// Append parsed devices from `devices` array to the staging buffer.
    ImportResult importChunk(const String &token, JsonArray devices);

    /// Atomically replace the live device list with the staging buffer and persist.
    /// Triggers HA sub-device sync exactly once.
    ImportResult importCommit(const String &token);

    /// Discard an in-flight import session.
    ImportResult importAbort(const String &token);

private:
    // ========================================================================
    // Member Variables
    // ========================================================================

    // Endpoints and persistence
    HttpEndpoint<GeniusDevices> _httpEndpoint;   ///< REST API endpoint handler (bulk GET/POST)
    FSPersistence<GeniusDevices> _fsPersistence; ///< File system persistence handler

    // Direct server access for the additional per-device + import endpoints registered in begin()
    PsychicHttpServer *_server;
    SecurityManager  *_securityManager;

    // Alarm state tracking
    bool _isAlarming;      ///< Current global alarming state
    uint32_t _numAlarming; ///< Number of devices currently alarming

    // Status summary tracking
    uint32_t _numFaulty = 0;      ///< Number of devices with an active fault
    uint32_t _numBatteryLow = 0;  ///< Number of devices reporting a low battery
    std::atomic<uint32_t> _numReportedMissing{0}; ///< Number of devices reported missing; read by the RX path without the mutex

    /// What the gateway heard of a radio module since it started. Kept for FM Pro X modules
    /// only - they send a daily supervision series, so "last seen" means something there.
    struct ModuleRuntime
    {
        uint32_t deviceId = 0;
        uint32_t moduleSn = 0;
        time_t lastSeen = 0;  ///< Last frame this module originated or transmitted (0: none yet)
        int8_t rssi = 0;      ///< Signal strength of the last frame it transmitted itself
        time_t rssiTime = 0;  ///< When that frame was received (0: no signal value yet)
        bool changed = false; ///< Not yet sent to the web clients
    };
    std::vector<ModuleRuntime> _runtime;         ///< RAM only; guarded by _runtimeMutex
    SemaphoreHandle_t _runtimeMutex = nullptr;   ///< Guards _runtime; never held together with the state mutex
    std::atomic<bool> _runtimeAny{false};        ///< _runtime is not empty; lets the RX path skip the mutex

    /// Bring _runtime in line with the device list: one entry per detector with an FM Pro X module
    void _syncRuntimeTable();

    /// Second half of moduleHeard(): end "reported missing" for the detectors of these modules
    void _endReportedMissing(uint32_t originModuleSN, uint32_t senderModuleSN);
    String _lastFaultPayload;     ///< Last payload sent to the simple fault topic (publish on change only)

    // MQTT
    PsychicMqttClient *_mqttClient;                   ///< MQTT client (for simple alarm topic)
    AlarmPublishingSettingsService *_alarmPublishingSettings; ///< Alarm-publishing settings service
    AlarmPublishingSettings _cachedAlarmPublishingSettings;   ///< Cached copy of alarm-publishing settings
    HAService *_haService;                            ///< HA service
    HASettingsService *_haSettingsService;            ///< HA settings service (source of the "rename entity IDs" option)
    EventSocket *_eventSocket;                        ///< WebSocket event channel (rename progress)

    // HA sub-device tracking
    struct SmokeDetectorHA {
        HADevice                                  *device;
        HABinarySensor                            *sensor;       ///< smoke alarm binary_sensor (Control)
        std::unique_ptr<HAGroupedSensorPublisher>  diagnostics;  ///< status + readout entities on one shared state topic
    };
    std::map<uint32_t, SmokeDetectorHA> _haDevices;  ///< maps device.id → HA objects

    // ------------------------------------------------------------------------
    // Import session state (single-slot, RAM-only, PSRAM-preferred staging)
    // ------------------------------------------------------------------------
    struct ImportSession {
        bool active = false;
        String token;
        time_t lastActivity = 0;
        std::vector<GeniusDevice, PsramAllocator<GeniusDevice>> staging;
    };
    ImportSession   _importSession;
    SemaphoreHandle_t _importMutex = nullptr;  ///< guards _importSession (separate from state mutex)

    /// Returns true if a session is active but past its TTL, and resets it.
    bool _expireImportSessionIfStale();

    /// Generate a new opaque session token (URL-safe).
    static String _newImportToken();

    // ------------------------------------------------------------------------
    // Post-commit background task
    // ------------------------------------------------------------------------
    // Triggered from importCommit. Runs callUpdateHandlers (FS write + HA sync)
    // OFF the HTTP server task so the commit response returns in milliseconds
    // instead of blocking 15-20 s while ~420 HA-discovery MQTT messages publish
    // for a fresh 30-device import. That blocking otherwise starves the WS
    // heartbeat and causes the front end to time out the commit request.
    TaskHandle_t _postCommitTaskHandle = nullptr;
    void _postCommitTask();

    /**
     * @brief Work that follows a committed import: flash write and Home Assistant sync
     *
     * Runs the update handlers and reports the progress with DEVICE_IMPORT_EVENT:
     * "saved" once the device list is written to flash, "done" when the handlers have
     * returned - with what was done for Home Assistant. An entity-ID rename the sync left
     * pending is carried out by the rename task afterwards, as a background cycle.
     */
    void _runPostCommit();

    /// True while the calling task runs the update handlers for a committed import
    bool _isImportSync() const { return _importSyncTask.load() == xTaskGetCurrentTaskHandle(); }

    /// Emit DEVICE_IMPORT_EVENT: "saved" (with `ha`: integration enabled) or "done" (with `ha`: outcome)
    void _emitImportEvent(const char *state);

    std::atomic<TaskHandle_t> _importSyncTask{nullptr}; ///< Task that runs the update handlers for a committed import, if any
    static void _postCommitTaskImpl(void *ctx) { static_cast<GeniusDevicesService *>(ctx)->_postCommitTask(); }

    // ------------------------------------------------------------------------
    // HA entity-ID rename task
    // ------------------------------------------------------------------------
    // Home Assistant only adopts a new entity ID when the entity is removed and
    // created again, with a pause in between. That cycle runs here, never in the
    // HTTP task or the packet RX path and never while the state mutex is held -
    // the mutex is shared with the alarm path, which must not stall for seconds.
    TaskHandle_t _haRenameTaskHandle = nullptr;
    std::atomic<bool> _haSyncRequested{false}; ///< Set when the HA settings changed: the task re-syncs all detectors first
    void _haRenameTask();
    static void _haRenameTaskImpl(void *ctx) { static_cast<GeniusDevicesService *>(ctx)->_haRenameTask(); }

    // ========================================================================
    // State Management
    // ========================================================================

    /// Update internal alarming state counters
    void _updateAlarmingState();

    /// Update the status summary counters and the simple fault topic (publishes on change only)
    void _updateStatusSummary();

    /// Build the simple fault topic payload from the current device list
    String _buildSimpleFaultPayload();

    // ========================================================================
    // ID Generation
    // ========================================================================

    /// Generate a unique device ID for new devices
    uint32_t _generateUniqueDeviceId() const;

    // ========================================================================
    // HA Sub-device Management
    // ========================================================================

    /// Add a HADevice sub-device for one smoke detector (idempotent)
    void _addSmokeDetectorSubDevice(const GeniusDevice &device);

    /**
     * @brief HA device display name for a detector
     *
     * Model name by default. With the opt-in HA setting "rename entity IDs"
     * enabled, the user-defined location is used (if set). A changed display
     * name alone is published in place and needs no rename cycle.
     */
    String _haDeviceName(const GeniusDevice &device) const;

    /**
     * @brief Entity-ID prefix a detector should have in Home Assistant
     *
     * Empty while the "rename entity IDs" option is off: the detector is not
     * managed and HA generates the IDs itself. With the option on it is the
     * slug of the location or, without a usable location, `genius_<serial>`
     * (`genius_device_<id>` if the serial number is 0), so every detector gets
     * an explicit ID and a cleared location frees the location-based IDs.
     * "Without a usable location" covers an empty location, one without any
     * letter or digit and the GENIUS_DEVICE_DEFAULT_LOCATION placeholder.
     *
     * Entity IDs are built as `<component>.<prefix>_<slug(entity name)>`.
     */
    String _entityIdPrefixFor(const GeniusDevice &device) const;

    /**
     * @brief Whether the entity IDs of a detector still have to be renamed in HA
     *
     * True while the option is on, the integration is enabled and the prefix
     * last announced differs from the desired one. This one comparison covers a
     * changed location, a change made while MQTT was down, a reboot in between
     * and the option being switched on.
     */
    bool _isEntityIdRenamePending(const GeniusDevice &device) const;

    /**
     * @brief Write the current identity of a detector into its HADevice
     * @return true if name, serial number, area, model or entity-ID prefix changed
     */
    bool _applySmokeDetectorIdentity(HADevice &haDevice, const GeniusDevice &device) const;

    /// Remove a HADevice sub-device for one smoke detector by stable device ID
    void _removeSmokeDetectorSubDevice(uint32_t deviceId);

    /**
     * @brief Sync the HA sub-device list with the current _state.devices list
     *
     * Adds and removes sub-devices, refreshes the identity of all others and
     * republishes those that changed. Never waits: detectors with a pending
     * entity-ID rename are left to the rename task.
     *
     * @param requestRename Wake the rename task if a rename is pending. False
     *                      at boot (MQTT is not up yet; the connect runs the
     *                      comparison again) and when called by the task itself.
     * @param deferredRepublish Optional: receives the IDs of the detectors that
     *                      changed instead of republishing them here. The caller
     *                      publishes them (used by the rename task, which runs
     *                      the rename first).
     */
    void _syncSmokeDetectorSubDevices(bool requestRename = true, std::vector<uint32_t> *deferredRepublish = nullptr);

    /**
     * @brief Publish the discovery configs and states of one detector
     *
     * Holds the state mutex for this one detector only.
     *
     * @param deviceId Stable device ID
     * @param announcedPrefix Optional: receives the entity-ID prefix the configs were published with
     * @return true if everything was handed to the MQTT client
     */
    bool _publishSmokeDetectorDiscovery(uint32_t deviceId, String *announcedPrefix = nullptr);

    /**
     * @brief Wake the rename task (cheap; safe from any task)
     *
     * @param background true for a cycle caused by an import: the user asked for an import,
     *                   not for a rename, so the web interface shows no progress for it. An
     *                   announced request that arrives before the cycle starts wins.
     */
    void _requestEntityIdRename(bool background = false);

    /**
     * @brief Rename the HA entities of all detectors with a pending rename, in one batch
     *
     * Clears the retained discovery configs of all of them, waits once, publishes
     * them again and then stores the announced prefixes with a single flash
     * write. Reports progress via the HA_ENTITY_RENAME_EVENT WebSocket event.
     * Without an MQTT connection nothing is sent ("deferred"); the comparison
     * runs again on the next connect. If the connection drops half-way the
     * prefixes are not stored, so the next connect repeats the cycle.
     */
    void _runEntityIdRename(bool background);

    /// Emit HA_ENTITY_RENAME_EVENT with the given state, the detector count and the locations
    /// of the first detectors of the batch, so the web interface can say who is renamed.
    /// `background` marks a cycle caused by an import (see _requestEntityIdRename()).
    void _emitEntityRenameEvent(const char *state, uint32_t count, const std::vector<String> &names, bool background);

    /// Trigger a diagnostics state publish for one device (by stable device ID).
    /// Reads all values directly from _state.devices - no separate cache needed.
    esp_err_t _publishSmokeDetectorAttributes(uint32_t deviceId);

    // ========================================================================
    // Settings Management
    // ========================================================================

    /// Update cached alarm-publishing settings from AlarmPublishingSettingsService for faster access
    void _updateAlarmPublishingSettingsCache();
};

#endif // GeniusDevicesService_h
