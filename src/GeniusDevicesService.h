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

#include <map>
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
#include <HomeAssistant/HAService.h>
#include <HomeAssistant/HADevice.h>
#include <HomeAssistant/HABinarySensor.h>
#include <HomeAssistant/HAGroupedSensorPublisher.h>
#include <PsychicMqttClient.h>

#define GATEWAY_DEVICES_FILE "/config/gateway-devices.json"  ///< Configuration file path for device data
#define GATEWAY_DEVICES_SERVICE_PATH "/rest/gateway-devices"  ///< REST API service endpoint path (bulk GET/POST)

// Per-device CRUD endpoints (keep request bodies small regardless of total device count)
#define GATEWAY_DEVICES_DEVICE_PATH         "/rest/gateway-devices/device"          ///< PUT: upsert single device
#define GATEWAY_DEVICES_DEVICE_DELETE_PATH  "/rest/gateway-devices/device/delete"   ///< POST: delete single device (body: { id })
#define GATEWAY_DEVICES_REORDER_PATH        "/rest/gateway-devices/reorder"         ///< POST: reorder (body: { order: [id...] })

// Chunked import endpoints (let arbitrarily large device lists land without holding the whole payload in heap)
#define GATEWAY_DEVICES_IMPORT_BEGIN_PATH   "/rest/gateway-devices/import/begin"
#define GATEWAY_DEVICES_IMPORT_CHUNK_PATH   "/rest/gateway-devices/import/chunk"
#define GATEWAY_DEVICES_IMPORT_COMMIT_PATH  "/rest/gateway-devices/import/commit"
#define GATEWAY_DEVICES_IMPORT_ABORT_PATH   "/rest/gateway-devices/import/abort"

#define GATEWAY_DEVICES_IMPORT_SESSION_TTL_S 60  ///< Idle-expiry for a chunked-import session

#ifndef GATEWAY_MAX_DEVICES
#define GATEWAY_MAX_DEVICES 50   ///< Maximum number of devices supported (override via build flag for no-PSRAM boards)
#endif
#define GATEWAY_MAX_ALARMS 100   ///< Maximum number of alarms supported

#define ALARM_STATE_CHANGE "alarm-state-change"  ///< WebSocket event for alarm state changes

#define GENIUS_DEVICE_ADDED_FROM_PACKET "genius-device-added-from-packet"  ///< Event for device discovery
#define GENIUS_DEVICE_DEFAULT_LOCATION "Unknown location"  ///< Default location for new devices

#define GATEWAY_DEVICES_CONFIG_VERSION 1  ///< Current config file version (for JSON migration support)

typedef enum genius_alarm_ending
{
    GAE_MIN = -2,              ///< Minimum value (for enum range checks)
    GAE_ALARM_ACTIVE = -1,     ///< Alarm is currently active
    GAE_BY_SMOKE_DETECTOR = 0, ///< Alarm was ended by smoke detector
    GAE_BY_MANUAL,             ///< Alarm was ended manually via web interface
    GAE_BY_IMPORT,             ///< Alarm was cleared during a device config import
    GAE_MAX                    ///< Maximum value (for enum range checks)
} genius_alarm_ending_t;

typedef struct genius_device_alarm
{
    time_t startTime;                   ///< Alarm start timestamp
    time_t endTime;                     ///< Alarm end timestamp
    genius_alarm_ending_t endingReason; ///< How the alarm was ended

    bool operator==(const genius_device_alarm &o) const
    {
        return startTime == o.startTime && endTime == o.endTime && endingReason == o.endingReason;
    }
} genius_device_alarm_t;

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
    bool batteryLowFault;          ///< Battery low fault flag
    bool deviceFault;              ///< Device fault flag
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
    bool radioNetworkFault;   ///< Radio network fault flag

    void toJson(JsonObject &root) const
    {
        if (static_cast<int>(model) != -1)
            root["model"] = static_cast<int>(model);
        root["sn"] = sn;
        if (lineId)           root["lineId"] = lineId;
        if (lineCharacter)    root["lineCharacter"] = String(lineCharacter);
        root["lineNumber"] = lineNumber;
        if (radioStateMask)   root["radioStateMask"] = radioStateMask;
        if (radioSwitchMask)  root["radioSwitchMask"] = radioSwitchMask;
        if (radioInterference > 0.0f) root["radioInterference"] = radioInterference;
        if (radioNetworkFault) root["radioNetworkFault"] = radioNetworkFault;
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
        JsonArray alarms = root["alarms"].to<JsonArray>();
        for (auto &alarm : this->alarms)
        {
            JsonObject alarm_as_json = alarms.add<JsonObject>();
            alarm_as_json["startTime"] = Utils::time_t_to_iso8601(alarm.startTime);
            alarm_as_json["endTime"] = Utils::time_t_to_iso8601(alarm.endTime);
            alarm_as_json["endingReason"] = alarm.endingReason;
        }
        if (readoutTime > 0)
        {
            root["readoutTime"] = Utils::time_t_to_iso8601(readoutTime);
            root["readoutProtocolVersion"] = readoutProtocolVersion;
        }
    }

    uint32_t id;                          ///< Unique identifier for device (auto-generated, immutable)
    GeniusSmokeDetectorInfo smokeDetector;
    GeniusRadioModuleInfo radioModule;
    String location;
    std::vector<genius_device_alarm_t> alarms;
    genius_device_registration_t registration;
    bool isAlarming;
    bool published;                       ///< Whether current device config has been published via MQTT
    time_t readoutTime;                   ///< SmartSonic readout timestamp (0 = no readout performed)
    uint8_t readoutProtocolVersion;       ///< SmartSonic protocol version (valid when readoutTime > 0)

    /// Parse an "alarms" JsonArray into a vector, capped at GATEWAY_MAX_ALARMS.
    static std::vector<genius_device_alarm_t> parseAlarms(JsonArray jsonAlarms);

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

    /// Check if a smoke detector is known/registered
    bool isSmokeDetectorKnown(uint32_t detectorSN);

    /**
     * @brief Synchronize HA sub-devices with current device list.
     *
     * Adds sub-devices for any unregistered smoke detectors, removes sub-devices
     * for deleted ones. HAService handles config and state republishing on
     * MQTT (re)connect — this function only needs to be called when the device
     * list changes.
     *
     * @param onlyUnpublished Ignored — sync is always idempotent (kept for API compatibility)
     */
    void mqttPublishAllDevices(bool onlyUnpublished = true);

    /// Publish global alarm state to the simple alarm topic (independent of HA integration)
    void mqttPublishSimpleAlarmState();

    /**
     * @brief Re-publish alarm state for all devices.
     *
     * Calls publishState() on each smoke-alarm sensor. Used after bulk alarm resets.
     */
    void mqttPublishAllDevicesState(bool onlyUnpublished = true);

    /**
     * @brief Publish alarm state for a single device (by serial number).
     *
     * Calls sensor->publishState() — the sensor getter reads device.isAlarming live.
     *
     * @param smokeDetectorSN Smoke detector serial number
     * @param useTransaction If true, wraps access in transaction
     * @param markPublished If true, marks device.published=true on success
     * @return ESP_OK on success, ESP_ERR_NOT_FOUND if device not found
     */
    esp_err_t mqttPublishDeviceState(uint32_t smokeDetectorSN, bool useTransaction = true, bool markPublished = true);

    // ========================================================================
    // Per-Device CRUD (small-body endpoints — replace the bulk POST for UI ops)
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

    /// Reorder devices to match the given id list. The list MUST contain exactly the
    /// current device id set (no additions, no removals) — otherwise INVALID_BODY.
    DeviceOpResult reorderDevices(const std::vector<uint32_t> &newOrder);

    // ========================================================================
    // Chunked Import (single global slot — only one admin can import at a time)
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

    // MQTT
    PsychicMqttClient *_mqttClient;                   ///< MQTT client (for simple alarm topic)
    AlarmPublishingSettingsService *_alarmPublishingSettings; ///< Alarm-publishing settings service
    AlarmPublishingSettings _cachedAlarmPublishingSettings;   ///< Cached copy of alarm-publishing settings
    HAService *_haService;                            ///< HA service

    // HA sub-device tracking
    struct SmokeDetectorHA {
        HADevice                                  *device;
        HABinarySensor                            *sensor;       ///< smoke alarm binary_sensor (Control)
        std::unique_ptr<HAGroupedSensorPublisher>  diagnostics;  ///< 13 readout entities on one shared state topic
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
    static void _postCommitTaskImpl(void *ctx) { static_cast<GeniusDevicesService *>(ctx)->_postCommitTask(); }

    // ========================================================================
    // State Management
    // ========================================================================

    /// Update internal alarming state counters
    void _updateAlarmingState();

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

    /// Remove a HADevice sub-device for one smoke detector by stable device ID
    void _removeSmokeDetectorSubDevice(uint32_t deviceId);

    /// Sync the HA sub-device list with the current _state.devices list
    void _syncSmokeDetectorSubDevices();

    /// Trigger a diagnostics state publish for one device (by stable device ID).
    /// Reads all values directly from _state.devices — no separate cache needed.
    esp_err_t _publishSmokeDetectorAttributes(uint32_t deviceId);

    // ========================================================================
    // Settings Management
    // ========================================================================

    /// Update cached alarm-publishing settings from AlarmPublishingSettingsService for faster access
    void _updateAlarmPublishingSettingsCache();
};

#endif // GeniusDevicesService_h
