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
#include <GatewayMqttSettingsService.h>
#include <HomeAssistant/HAService.h>
#include <HomeAssistant/HADevice.h>
#include <HomeAssistant/HABinarySensor.h>
#include <PsychicMqttClient.h>

#define GATEWAY_DEVICES_FILE "/config/gateway-devices.json"  ///< Configuration file path for device data
#define GATEWAY_DEVICES_SERVICE_PATH "/rest/gateway-devices"  ///< REST API service endpoint path

#define GATEWAY_MAX_DEVICES 50   ///< Maximum number of devices supported
#define GATEWAY_MAX_ALARMS 100   ///< Maximum number of alarms supported

#define ALARM_STATE_CHANGE "alarm-state-change"  ///< WebSocket event for alarm state changes

#define GENIUS_DEVICE_ADDED_FROM_PACKET "genius-device-added-from-packet"  ///< Event for device discovery
#define GENIUS_DEVICE_DEFAULT_LOCATION "Unknown location"  ///< Default location for new devices

typedef enum genius_alarm_ending
{
    GAE_MIN = -2,              ///< Minimum value (for enum range checks)
    GAE_ALARM_ACTIVE = -1,     ///< Alarm is currently active
    GAE_BY_SMOKE_DETECTOR = 0, ///< Alarm was ended by smoke detector
    GAE_BY_MANUAL,             ///< Alarm was ended manually via web interface
    GAE_MAX                    ///< Maximum value (for enum range checks)
} genius_alarm_ending_t;

typedef struct genius_device_alarm
{
    time_t startTime;                   ///< Alarm start timestamp
    time_t endTime;                     ///< Alarm end timestamp
    genius_alarm_ending_t endingReason; ///< How the alarm was ended
} genius_device_alarm_t;

typedef enum genius_smoke_detector
{
    GSD_UNKNOWN = -1,     ///< Unknown smoke detector type
    GSD_GENIUS_PLUS_X = 0 ///< Genius Plus X smoke detector model
} GeniusSmokeDetector;

typedef enum genius_radio_module
{
    GRM_UNKNOWN = -1,     ///< Unknown radio module type
    GRM_FM_BASIS_X = 0    ///< FM Basis X radio module model
} GeniusRadioModule;

/// Template class for Genius components
template <typename T>
class GeniusComponent
{
public:
    GeniusComponent(const T &model,
                    uint32_t sn,
                    time_t productionDate) : model(model),
                                             sn(sn),
                                             productionDate(productionDate)
    {
    }

    void toJson(JsonObject &root)
    {
        // Serial number
        root["sn"] = sn;
        // Production date (if any set)
        if (productionDate > 0)
            root["productionDate"] = Utils::time_t_to_iso8601(productionDate);
        // Model (if any set)
        if (static_cast<int>(model) != -1)
            root["model"] = static_cast<int>(model);
    }

    T model;                ///< Component model type
    uint32_t sn;           ///< Component serial number
    time_t productionDate; ///< Production date (Unix timestamp)
};

typedef enum genius_device_registration
{
    GDR_MIN = -1,      ///< Boundary check minimum value
    GDR_BUILT_IN = 0,  ///< Device is built-in
    GDR_GENIUS_PACKET, ///< Device was added via received genius packet
    GDR_MANUAL,        ///< Device registered manually (via web interface)
    GDR_MAX            ///< Boundary check maximum value
} genius_device_registration_t;

/// Class for Genius devices
class GeniusDevice
{
public:
    GeniusDevice(const GeniusComponent<GeniusSmokeDetector> &smokeDetector,
                 const GeniusComponent<GeniusRadioModule> &radioModule,
                 const String &location,
                 uint32_t id = 0) : smokeDetector(smokeDetector),
                                   radioModule(radioModule),
                                   location(location),
                                   id(id), // Use provided ID (from JSON) or will be set by service
                                   registration(GDR_MANUAL),
                                   isAlarming(false),
                                   published(false)
    {
    }

    void toJson(JsonObject &root)
    {
        // Device ID (for internal tracking, not user-editable)
        root["id"] = this->id;
        // Smoke detector
        JsonObject smokeDetector = root["smokeDetector"].to<JsonObject>();
        this->smokeDetector.toJson(smokeDetector);
        // Radio module
        JsonObject radioModule = root["radioModule"].to<JsonObject>();
        this->radioModule.toJson(radioModule);
        // Location
        root["location"] = this->location;
        // Is alarming?
        root["isAlarming"] = this->isAlarming;
        // Registration
        root["registration"] = this->registration;
        // Alarms
        JsonArray alarms = root["alarms"].to<JsonArray>();
        for (auto &alarm : this->alarms)
        {
            JsonObject alarm_as_json = alarms.add<JsonObject>();

            alarm_as_json["startTime"] = Utils::time_t_to_iso8601(alarm.startTime);
            alarm_as_json["endTime"] = Utils::time_t_to_iso8601(alarm.endTime);
            alarm_as_json["endingReason"] = alarm.endingReason;
        }
    }

    uint32_t id; // Unique identifier for device (auto-generated, immutable)
    GeniusComponent<GeniusSmokeDetector> smokeDetector;
    GeniusComponent<GeniusRadioModule> radioModule;
    String location;
    std::vector<genius_device_alarm_t> alarms;
    genius_device_registration_t registration;
    bool isAlarming;
    bool published; // Whether the current device configuration has been published to Home Assistant
};

class GeniusDevices
{

public:
    static constexpr const char *TAG = "GeniusDevices";

    std::vector<GeniusDevice> devices;
    std::vector<uint32_t> deletedDeviceIds; ///< Temporary storage for deleted device IDs (populated during update)

    static void read(GeniusDevices &geniusDevices, JsonObject &root)
    {
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

    GeniusDevicesService(ESP32SvelteKit *sveltekit, PsychicMqttClient *mqttClient, GatewayMqttSettingsService *mqttSettingsService);

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
     * Updates _alarmStates from current device data and calls publishState()
     * on each sensor. Used after bulk alarm resets.
     */
    void mqttPublishAllDevicesState(bool onlyUnpublished = true);

    /**
     * @brief Publish alarm state for a single device (by serial number).
     *
     * Updates _alarmStates[device.id] and calls sensor->publishState().
     *
     * @param smokeDetectorSN Smoke detector serial number
     * @param useTransaction If true, wraps access in transaction
     * @param markPublished If true, marks device.published=true on success
     * @return ESP_OK on success, ESP_ERR_NOT_FOUND if device not found
     */
    esp_err_t mqttPublishDeviceState(uint32_t smokeDetectorSN, bool useTransaction = true, bool markPublished = true);

private:
    // ========================================================================
    // Member Variables
    // ========================================================================

    // Endpoints and persistence
    HttpEndpoint<GeniusDevices> _httpEndpoint;   ///< REST API endpoint handler
    FSPersistence<GeniusDevices> _fsPersistence; ///< File system persistence handler

    // Alarm state tracking
    bool _isAlarming;      ///< Current global alarming state
    uint32_t _numAlarming; ///< Number of devices currently alarming

    // MQTT
    PsychicMqttClient *_mqttClient;                   ///< MQTT client (for simple alarm topic)
    GatewayMqttSettingsService *_mqttSettingsService; ///< MQTT settings service
    GatewayMqttSettings _cachedMqttSettings;          ///< Cached copy of alarm MQTT settings
    HAService *_haService;                            ///< HA service

    // HA sub-device tracking
    struct SmokeDetectorHA {
        HADevice *device;      ///< owned by HAService
        HABinarySensor *sensor; ///< owned by device
    };
    std::map<uint32_t, SmokeDetectorHA> _haDevices; ///< maps device.id → HA objects
    std::map<uint32_t, bool> _alarmStates;           ///< maps device.id → isAlarming

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

    /// Publish json_attributes_topic payload for one device
    esp_err_t _publishSmokeDetectorAttributes(uint32_t sn, const GeniusDevice &device);

    // ========================================================================
    // Settings Management
    // ========================================================================

    /// Update cached MQTT settings from GatewayMqttSettingsService for faster access
    void _updateMqttSettingsCache();
};

#endif // GeniusDevicesService_h
