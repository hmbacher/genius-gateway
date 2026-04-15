/**
 * @file GeniusDevicesService.cpp
 * @brief Implementation of the genius devices service
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

#include <GeniusDevicesService.h>
#include <WiFi.h>
#include <Utils.hpp>

GeniusDevicesService::GeniusDevicesService(ESP32SvelteKit *sveltekit, PsychicMqttClient *mqttClient, GatewayMqttSettingsService *mqttSettingsService) : _httpEndpoint(GeniusDevices::read,
                                                                                                                                                                      GeniusDevices::update,
                                                                                                                                                                      this,
                                                                                                                                                                      sveltekit->getServer(),
                                                                                                                                                                      GATEWAY_DEVICES_SERVICE_PATH,
                                                                                                                                                                      sveltekit->getSecurityManager(),
                                                                                                                                                                      AuthenticationPredicates::IS_ADMIN),
                                                                                                                                                        _fsPersistence(GeniusDevices::read,
                                                                                                                                                                       GeniusDevices::update,
                                                                                                                                                                       this,
                                                                                                                                                                       sveltekit->getFS(),
                                                                                                                                                                       GATEWAY_DEVICES_FILE),
                                                                                                                                                        _isAlarming(false),
                                                                                                                                                        _numAlarming(0),
                                                                                                                                                        _mqttClient(mqttClient),
                                                                                                                                                        _mqttSettingsService(mqttSettingsService)
{
}

// ============================================================================
// Public Methods - Lifecycle
// ============================================================================

void GeniusDevicesService::begin()
{
    _httpEndpoint.begin();
    _fsPersistence.readFromFS();
    // FSPersistence::readFromFS uses updateWithoutPropagation, so update handlers
    // (incl. writeToFS) are never triggered on load. Force a write-back here so
    // any migration changes (e.g. v0→v1 enum remapping + version field) are
    // persisted immediately and don't re-run on the next restart.
    _fsPersistence.writeToFS();

    // Initialize cached MQTT settings
    _updateMqttSettingsCache();

    /* Update alarming state after every device update */
    this->addUpdateHandler([&](const String &originId)
                           { _updateAlarmingState(); },
                           false);

    /* Republish MQTT state after device updates (except for packet-originated or alarm state changes) */
    this->addUpdateHandler([&](const String &originId)
                           {
                               if (originId != GENIUS_DEVICE_ADDED_FROM_PACKET && originId != ALARM_STATE_CHANGE)
                               {
                                   mqttPublishAllDevices();
                               } },
                           false);

    /* Publish simple alarm topic on alarm state changes */
    this->addUpdateHandler([&](const String &originId)
                           {
                               if (originId == ALARM_STATE_CHANGE)
                                   mqttPublishSimpleAlarmState();
                           },
                           false);

    /* Update cache and republish MQTT when MQTT settings change */
    if (_mqttSettingsService != nullptr)
    {
        _mqttSettingsService->addUpdateHandler([this](const String &originId)
                                               {
                                                   this->_updateMqttSettingsCache();
                                                   this->mqttPublishAllDevices(false); // Full republish: prefix or enabled state may have changed
                                                   this->mqttPublishSimpleAlarmState(); },
                                               false);
    }
}

// ============================================================================
// Public Methods - Device Management
// ============================================================================

bool GeniusDevicesService::AddGeniusDevice(const uint32_t snRadioModule,
                                           const uint32_t snSmokeDetector)
{
    beginTransaction();

    // Create a new GeniusDevice with unknown models and production dates
    GeniusSmokeDetectorInfo sd = {};
    sd.model = static_cast<GeniusSmokeDetector>(GSD_UNKNOWN);
    sd.sn = snSmokeDetector;
    GeniusRadioModuleInfo rm = {};
    rm.model = static_cast<GeniusRadioModule>(GRM_UNKNOWN);
    rm.sn = snRadioModule;
    GeniusDevice newDevice = GeniusDevice(sd, rm, GENIUS_DEVICE_DEFAULT_LOCATION);

    // Generate unique ID with collision detection
    newDevice.id = _generateUniqueDeviceId();

    // Set registration type
    newDevice.registration = GDR_GENIUS_PACKET;
    // Add the new device to the state
    _state.devices.push_back(newDevice);

    endTransaction();

    callUpdateHandlers(GENIUS_DEVICE_ADDED_FROM_PACKET);

    return true;
}

const GeniusDevice *GeniusDevicesService::setAlarm(uint32_t detectorSN)
{
    GeniusDevice *updatedDevice = nullptr;

    beginTransaction();

    for (GeniusDevice &device : _state.devices)
    {
        if (device.smokeDetector.sn == detectorSN)
        {
            if (!device.isAlarming)
            {
                device.isAlarming = true;
                genius_device_alarm_t alarm = {.startTime = time(nullptr), // seconds precision
                                               .endTime = 0,
                                               .endingReason = GAE_ALARM_ACTIVE};
                device.alarms.push_back(alarm);

                device.published = false; // Mark as not published for MQTT publishing

                updatedDevice = &device;
                _isAlarming = true;
                _numAlarming++;

                ESP_LOGI(GeniusDevices::TAG, "Alarm started for smoke detector with SN '%lu'.", detectorSN);
            }
            break;
        }
    }

    endTransaction();

    if (updatedDevice)
        callUpdateHandlers(ALARM_STATE_CHANGE);

    return updatedDevice;
}

const GeniusDevice *GeniusDevicesService::resetAlarm(uint32_t detectorSN, genius_alarm_ending_t endingReason)
{
    GeniusDevice *updatedDevice = nullptr;

    beginTransaction();

    for (GeniusDevice &device : _state.devices)
    {
        if (device.smokeDetector.sn == detectorSN)
        {
            if (device.isAlarming)
            {
                device.isAlarming = false;

                if (!device.alarms.empty())
                {
                    device.alarms.back().endTime = time(nullptr); // seconds precision
                    device.alarms.back().endingReason = endingReason;
                }
                else
                {
                    ESP_LOGW(GeniusDevices::TAG, "No active alarm found for smoke detector with SN '%lu' when trying to reset alarm.", detectorSN);
                }

                device.published = false; // Mark as not published for MQTT publishing

                updatedDevice = &device;
                _numAlarming--;

                ESP_LOGI(GeniusDevices::TAG, "Alarm ended for smoke detector with SN '%lu'.", detectorSN);
            }
            break;
        }
    }

    if (_numAlarming == 0)
        _isAlarming = false;

    endTransaction();

    if (updatedDevice)
        callUpdateHandlers(ALARM_STATE_CHANGE);

    return updatedDevice;
}

bool GeniusDevicesService::resetAllAlarms()
{
    bool updated = false;

    beginTransaction();

    for (GeniusDevice &device : _state.devices)
    {
        if (device.isAlarming)
        {
            device.isAlarming = false;

            if (!device.alarms.empty())
            {
                device.alarms.back().endTime = time(nullptr); // seconds precision
                device.alarms.back().endingReason = GAE_BY_MANUAL;
            }
            else
            {
                ESP_LOGW(GeniusDevices::TAG, "No active alarm found for smoke detector with SN '%lu' when trying to reset all alarms.", device.smokeDetector.sn);
            }

            device.published = false; // Mark as not published for MQTT publishing

            updated = true;

            ESP_LOGI(GeniusDevices::TAG, "Alarm ended for smoke detector with SN '%lu'.", device.smokeDetector.sn);
        }
    }

    _isAlarming = false;
    _numAlarming = 0;

    endTransaction();

    if (updated)
        callUpdateHandlers(ALARM_STATE_CHANGE);

    return updated;
}

// ============================================================================
// Private Methods - State Management
// ============================================================================

void GeniusDevicesService::_updateAlarmingState()
{
    bool isAlarming = false;
    uint32_t numAlarming = 0;

    for (const GeniusDevice &device : _state.devices)
    {
        if (device.isAlarming)
        {
            isAlarming = true;
            numAlarming++;
        }
    }

    beginTransaction();
    _isAlarming = isAlarming;
    _numAlarming = numAlarming;
    endTransaction();
}

// ============================================================================
// Private Methods - ID Generation
// ============================================================================

uint32_t GeniusDevicesService::_generateUniqueDeviceId() const
{
    uint32_t candidateId = (uint32_t)time(nullptr);

    // Simple linear scan for collision detection - efficient for small device counts
    while (std::any_of(_state.devices.begin(), _state.devices.end(),
                       [candidateId](const GeniusDevice &device)
                       {
                           return device.id == candidateId;
                       }))
    {
        candidateId++;
    }

    return candidateId;
}

// ============================================================================
// Public Methods - Device Queries
// ============================================================================

bool GeniusDevicesService::isAlarming()
{
    bool isAlarming = false;

    beginTransaction();
    isAlarming = _isAlarming;
    endTransaction();

    return isAlarming;
}

uint32_t GeniusDevicesService::numAlarmingDevices()
{
    uint32_t numAlarming = 0;

    beginTransaction();
    numAlarming = _numAlarming;
    endTransaction();

    return numAlarming;
}

bool GeniusDevicesService::isSmokeDetectorKnown(uint32_t detectorSN)
{
    bool found = false;
    beginTransaction();
    for (auto &device : _state.devices)
    {
        if (device.smokeDetector.sn == detectorSN)
        {
            found = true;
            break;
        }
    }
    endTransaction();
    return found;
}

StateUpdateResult GeniusDevices::update(JsonObject &root, GeniusDevices &geniusDevices, const String &originId)
{
    bool hasChanges = false;

    // Migration: v0 had GSD_GENIUS_PLUS_X=0 and GRM_FM_BASIS_X=0 (now 3 and 4 respectively).
    // Only migrate when loading from the config file (FSPersistence uses the file path as originId).
    // Frontend PUT requests always carry version=1 after the first migration, but we guard
    // against crafted requests by checking the origin.
    int configVersion = root["version"].is<int>() ? root["version"].as<int>() : 0;
    if (configVersion < 1 && originId == GATEWAY_DEVICES_FILE)
    {
        ESP_LOGI(GeniusDevices::TAG, "Migrating device config from v%d to v%d.", configVersion, GATEWAY_DEVICES_CONFIG_VERSION);
        if (root["devices"].is<JsonArray>())
        {
            for (JsonVariant dev : root["devices"].as<JsonArray>())
            {
                // GSD_GENIUS_PLUS_X: old 0 → new 3
                if (dev["smokeDetector"]["model"].is<int>() && dev["smokeDetector"]["model"].as<int>() == 0)
                    dev["smokeDetector"]["model"] = 3;
                // GRM_FM_BASIS_X: old 0 → new 4
                if (dev["radioModule"]["model"].is<int>() && dev["radioModule"]["model"].as<int>() == 0)
                    dev["radioModule"]["model"] = 4;
                // Remove obsolete radioModule.productionDate field
                dev["radioModule"].remove("productionDate");
            }
        }
        hasChanges = true; // force save with new version
    }

    if (!root["devices"].is<JsonArray>())
    {
        ESP_LOGV(GeniusDevices::TAG, "No devices array in JSON, no changes made.");
        return StateUpdateResult::UNCHANGED;
    }

    JsonArray jsonDevices = root["devices"].as<JsonArray>();
    std::vector<GeniusDevice> newDevicesVector; // Build new devices vector in JSON order
    std::vector<uint32_t> processedDeviceIds;   // Track which device IDs we've seen in JSON

    // Process each device from JSON - add new or update existing
    int deviceCount = 0;
    for (JsonVariant jsonDeviceArrItem : jsonDevices)
    {
        if (deviceCount++ >= GATEWAY_MAX_DEVICES)
        {
            ESP_LOGE(GeniusDevices::TAG, "Too many smoke detector devices. Maximum allowed is %d.", GATEWAY_MAX_DEVICES);
            break;
        }

        JsonObject smokeDetectorJson = jsonDeviceArrItem["smokeDetector"].as<JsonObject>();
        JsonObject radioModuleJson = jsonDeviceArrItem["radioModule"].as<JsonObject>();

        uint32_t deviceId = jsonDeviceArrItem["id"].as<uint32_t>();
        processedDeviceIds.push_back(deviceId);

        // Check if device exists by ID
        auto existingDevice = std::find_if(geniusDevices.devices.begin(), geniusDevices.devices.end(),
                                           [deviceId](const GeniusDevice &device)
                                           {
                                               return device.id == deviceId;
                                           });

        if (existingDevice == geniusDevices.devices.end())
        {
            // New device - add it (use ID from JSON)
            GeniusDevice newDevice = GeniusDevice(
                GeniusSmokeDetectorInfo::fromJson(smokeDetectorJson),
                GeniusRadioModuleInfo::fromJson(radioModuleJson),
                jsonDeviceArrItem["location"].as<String>(),
                deviceId); // Use the ID from JSON

            // Set optional properties with defaults
            newDevice.isAlarming = jsonDeviceArrItem["isAlarming"].is<bool>() ? jsonDeviceArrItem["isAlarming"].as<bool>() : false;
            newDevice.registration = jsonDeviceArrItem["registration"].is<int>() ? static_cast<genius_device_registration_t>(jsonDeviceArrItem["registration"].as<int>()) : GDR_MANUAL;

            // Readout metadata
            newDevice.readoutTime = jsonDeviceArrItem["readoutTime"].is<String>()
                ? Utils::iso8601_to_time_t(jsonDeviceArrItem["readoutTime"].as<String>()) : 0;
            newDevice.readoutProtocolVersion = jsonDeviceArrItem["readoutProtocolVersion"].is<int>()
                ? jsonDeviceArrItem["readoutProtocolVersion"].as<uint8_t>() : 0;

            // Process alarms
            if (jsonDeviceArrItem["alarms"].is<JsonArray>())
            {
                int alarms_count = 0;
                for (JsonVariant jsonAlarm : jsonDeviceArrItem["alarms"].as<JsonArray>())
                {
                    if (alarms_count++ >= GATEWAY_MAX_ALARMS)
                    {
                        ESP_LOGE(GeniusDevices::TAG, "Too many alarms for smoke detector device. Maximum allowed is %d.", GATEWAY_MAX_ALARMS);
                        break;
                    }

                    newDevice.alarms.push_back(genius_device_alarm_t{
                        .startTime = Utils::iso8601_to_time_t(jsonAlarm["startTime"].as<String>()),
                        .endTime = Utils::iso8601_to_time_t(jsonAlarm["endTime"].as<String>()),
                        .endingReason = static_cast<genius_alarm_ending_t>(jsonAlarm["endingReason"].as<int>())});
                }
            }

            // Mark for republishing
            newDevice.published = false;

            // Add the new device to our ordered vector
            newDevicesVector.push_back(newDevice);

            hasChanges = true;
        }
        else
        {
            // Copy existing device and update it (preserves order from JSON)
            GeniusDevice updatedDevice = *existingDevice;
            bool deviceChanged = false;

            // Update acoustic readout data
            GeniusSmokeDetectorInfo newSD = GeniusSmokeDetectorInfo::fromJson(smokeDetectorJson);
            GeniusRadioModuleInfo newRM = GeniusRadioModuleInfo::fromJson(radioModuleJson);
            time_t newReadoutTime = jsonDeviceArrItem["readoutTime"].is<String>()
                ? Utils::iso8601_to_time_t(jsonDeviceArrItem["readoutTime"].as<String>()) : 0;
            uint8_t newReadoutProtocolVersion = jsonDeviceArrItem["readoutProtocolVersion"].is<int>()
                ? jsonDeviceArrItem["readoutProtocolVersion"].as<uint8_t>() : 0;

            // Compare identity fields (model/sn/productionDate) for smoke detector
            if (updatedDevice.smokeDetector.model != newSD.model)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': SD model %d→%d", updatedDevice.location.c_str(),
                         static_cast<int>(updatedDevice.smokeDetector.model), static_cast<int>(newSD.model));
                updatedDevice.smokeDetector.model = newSD.model;
                deviceChanged = true;
            }
            if (updatedDevice.smokeDetector.sn != newSD.sn)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': SD SN %lu→%lu", updatedDevice.location.c_str(),
                         updatedDevice.smokeDetector.sn, newSD.sn);
                updatedDevice.smokeDetector.sn = newSD.sn;
                deviceChanged = true;
            }
            if (updatedDevice.smokeDetector.productionDate != newSD.productionDate)
            {
                updatedDevice.smokeDetector.productionDate = newSD.productionDate;
                deviceChanged = true;
            }

            // Compare identity fields for radio module
            if (updatedDevice.radioModule.model != newRM.model)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': RM model %d→%d", updatedDevice.location.c_str(),
                         static_cast<int>(updatedDevice.radioModule.model), static_cast<int>(newRM.model));
                updatedDevice.radioModule.model = newRM.model;
                deviceChanged = true;
            }
            if (updatedDevice.radioModule.sn != newRM.sn)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': RM SN %lu→%lu", updatedDevice.location.c_str(),
                         updatedDevice.radioModule.sn, newRM.sn);
                updatedDevice.radioModule.sn = newRM.sn;
                deviceChanged = true;
            }
            // Update readout metadata (readout fields merge into the existing structs)
            if (updatedDevice.readoutTime != newReadoutTime)
            {
                updatedDevice.readoutTime = newReadoutTime;
                updatedDevice.readoutProtocolVersion = newReadoutProtocolVersion;
                // Merge readout status fields from JSON into the device structs
                updatedDevice.smokeDetector = newSD;
                updatedDevice.radioModule = newRM;
                deviceChanged = true;
            }

            // Update location
            String newLocation = jsonDeviceArrItem["location"].as<String>();
            if (updatedDevice.location != newLocation)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': location→'%s'",
                         updatedDevice.location.c_str(), newLocation.c_str());
                updatedDevice.location = newLocation;
                deviceChanged = true;
            }

            // NOTE: The following attributes are managed internally and should not be updated from JSON:
            // - isAlarming: Controlled by alarm detection system
            // - registration: Set when device is first added/detected
            // - alarms: Managed by alarm start/stop events

            // Mark for republishing if device changed
            if (deviceChanged)
            {
                updatedDevice.published = false;
                hasChanges = true;
            }

            // Add updated device to our ordered vector
            newDevicesVector.push_back(updatedDevice);
        }
    }

    // Check if devices have been removed and track their SNs for unpublishing
    if (geniusDevices.devices.size() != newDevicesVector.size())
    {
        hasChanges = true;

        // Find deleted devices (in old list but not in processed IDs)
        geniusDevices.deletedDeviceSNs.clear();
        for (const auto &oldDevice : geniusDevices.devices)
        {
            if (std::find(processedDeviceIds.begin(), processedDeviceIds.end(), oldDevice.id) == processedDeviceIds.end())
            {
                // Device was deleted - store SN for unpublishing
                geniusDevices.deletedDeviceSNs.push_back(oldDevice.smokeDetector.sn);
                ESP_LOGI(GeniusDevices::TAG, "Device with SN %lu marked for deletion.", oldDevice.smokeDetector.sn);
            }
        }
    }

    // Check if devices have been added
    if (!hasChanges)
        hasChanges = geniusDevices.devices.size() != newDevicesVector.size();

    // Check if order changed
    if (!hasChanges)
    {
        for (size_t i = 0; i < geniusDevices.devices.size(); i++)
        {
            if (geniusDevices.devices[i].id != newDevicesVector[i].id)
            {
                hasChanges = true;
                break;
            }
        }
    }

    // Replace the original vector with the new ordered one
    geniusDevices.devices = std::move(newDevicesVector);

    ESP_LOGV(GeniusDevices::TAG, "Smoke detector devices configurations updated.");

    return hasChanges ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}

// ============================================================================
// Public Methods - MQTT Publishing
// ============================================================================

/**
 * @brief Publishes all devices (Config + State) to MQTT/Home Assistant
 *
 * First processes any pending deleted devices (unpublishing them), then
 * iterates over all current devices and publishes both configuration and state.
 * Only marks devices as published if both operations succeed (atomic semantics).
 * Uses a single transaction for the entire operation for efficiency.
 *
 * This function synchronizes MQTT state with the current device list.
 *
 * @param onlyUnpublished If true, only publishes devices that haven't been published yet
 */
void GeniusDevicesService::mqttPublishAllDevices(bool onlyUnpublished)
{
    if (_mqttClient == nullptr || !_mqttClient->connected() || _mqttSettingsService == nullptr)
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled)
        return;

    if (_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
    {
        ESP_LOGW(GeniusDevices::TAG, "Home Assistant MQTT discovery prefix is empty. Cannot publish devices.");
        return;
    }

    beginTransaction();

    // First, unpublish any deleted devices
    if (!_state.deletedDeviceSNs.empty())
    {
        ESP_LOGI(GeniusDevices::TAG, "Processing %d deleted device(s) for MQTT unpublishing.", _state.deletedDeviceSNs.size());

        for (uint32_t sn : _state.deletedDeviceSNs)
        {
            _mqttUnpublishDevice(sn);
        }

        // Clear the list after processing
        _state.deletedDeviceSNs.clear();
    }

    // Then publish current devices
    for (GeniusDevice &device : _state.devices)
    {
        if (!onlyUnpublished || !device.published)
        {
            // Pass useTransaction=false since we're already holding the lock
            // Pass markPublished=false since we'll set it after both config and state are published
            esp_err_t resConfig = mqttPublishDeviceConfig(device, false, false);
            esp_err_t resState = mqttPublishDeviceState(device, false, false);
            if (resConfig == ESP_OK && resState == ESP_OK)  // Safe approach: only mark as published if both config and state were successfully published
            {
                device.published = true;
            }
        }
    }
    endTransaction();
}

/**
 * @brief Publishes only the state of all devices to MQTT/Home Assistant
 *
 * Lightweight function for updating only device states (e.g. alarm ON/OFF).
 * Uses a single transaction for the entire operation for efficiency.
 *
 * @param onlyUnpublished If true, only publishes devices that haven't been published yet
 */
void GeniusDevicesService::mqttPublishAllDevicesState(bool onlyUnpublished)
{
    if (_mqttClient == nullptr || !_mqttClient->connected() || _mqttSettingsService == nullptr)
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled)
        return;

    if (_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
    {
        ESP_LOGW(GeniusDevices::TAG, "Home Assistant MQTT discovery prefix is empty. Cannot publish devices.");
        return;
    }

    beginTransaction();
    for (GeniusDevice &device : _state.devices)
    {
        if (!onlyUnpublished || !device.published)
        {
            // Pass useTransaction=false since we're already holding the lock
            // The function also marks the device as published if successful
            mqttPublishDeviceState(device, false);
        }
    }
    endTransaction();
}

/**
 * @brief Publishes MQTT discovery configuration for a single device (reference variant)
 *
 * Efficient variant that works directly with device reference - no data copying.
 * Used by batch operations and after device lookup. Also publishes device
 * attributes as a separate message.
 *
 * @param device Reference to the device to publish
 * @param useTransaction If true, wraps access in transaction. Set false if caller holds lock.
 * @param markPublished If true, sets device.published to true on success
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if MQTT not ready, ESP_ERR_INVALID_ARG for missing parameters, ESP_FAIL on publish error
 */
esp_err_t GeniusDevicesService::mqttPublishDeviceConfig(GeniusDevice &device, bool useTransaction, bool markPublished)
{
    if (_mqttClient == nullptr || !_mqttClient->connected() || _mqttSettingsService == nullptr)
        return ESP_ERR_INVALID_STATE;

    if (!_cachedMqttSettings.HAIntegrationEnabled)
        return ESP_ERR_INVALID_STATE;

    if (_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
    {
        ESP_LOGW(GeniusDevices::TAG, "Home Assistant MQTT discovery prefix is empty. Cannot publish device.");
        return ESP_ERR_INVALID_ARG;
    }

    if (useTransaction)
        beginTransaction();

    esp_err_t result = _publishDeviceConfig(device);
    if (markPublished && result == ESP_OK)
    {
        device.published = true;
    }

    _publishDeviceAttributes(device);   // Ignore result - best effort for attributes, doesn't affect published state

    if (useTransaction)
        endTransaction();

    return result;
}

/**
 * @brief Publishes MQTT state for a single device (serial number variant)
 *
 * Looks up the device by serial number and publishes state directly.
 * After successful lookup, only one search is performed (O(N) complexity).
 *
 * @param smokeDetectorSN Serial number of the smoke detector
 * @param useTransaction If true, wraps access in transaction. Set false if caller holds lock.
 * @param markPublished If true, sets device.published to true on success
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if device doesn't exist, ESP_ERR_INVALID_STATE if MQTT not ready, ESP_ERR_INVALID_ARG for missing parameters, ESP_FAIL on publish error
 */
esp_err_t GeniusDevicesService::mqttPublishDeviceState(uint32_t smokeDetectorSN, bool useTransaction, bool markPublished)
{
    if (_mqttClient == nullptr || !_mqttClient->connected() || _mqttSettingsService == nullptr)
        return ESP_ERR_INVALID_STATE;

    if (!_cachedMqttSettings.HAIntegrationEnabled)
        return ESP_ERR_INVALID_STATE;

    if (_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
    {
        ESP_LOGW(GeniusDevices::TAG, "Home Assistant MQTT discovery prefix is empty. Cannot publish device.");
        return ESP_ERR_INVALID_ARG;
    }

    if (useTransaction)
        beginTransaction();

    esp_err_t result = ESP_ERR_NOT_FOUND;
    for (GeniusDevice &device : _state.devices)
    {
        if (device.smokeDetector.sn == smokeDetectorSN)
        {
            result = _publishDeviceState(device);
            if (markPublished && result == ESP_OK)
            {
                device.published = true;
            }
            break;
        }
    }

    if (useTransaction)
        endTransaction();

    return result;
}

/**
 * @brief Publishes MQTT state for a single device (reference variant)
 *
 * Efficient variant that works directly with device reference - no data copying.
 * Used by batch operations and after device lookup.
 *
 * @param device Reference to the device to publish
 * @param useTransaction If true, wraps access in transaction. Set false if caller holds lock.
 * @param markPublished If true, sets device.published to true on success
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if MQTT not ready, ESP_ERR_INVALID_ARG for missing parameters, ESP_FAIL on publish error
 */
esp_err_t GeniusDevicesService::mqttPublishDeviceState(GeniusDevice &device, bool useTransaction, bool markPublished)
{
    if (_mqttClient == nullptr || !_mqttClient->connected() || _mqttSettingsService == nullptr)
        return ESP_ERR_INVALID_STATE;

    if (!_cachedMqttSettings.HAIntegrationEnabled)
        return ESP_ERR_INVALID_STATE;

    if (_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
    {
        ESP_LOGW(GeniusDevices::TAG, "Home Assistant MQTT discovery prefix is empty. Cannot publish device.");
        return ESP_ERR_INVALID_ARG;
    }

    if (useTransaction)
        beginTransaction();

    esp_err_t result = _publishDeviceState(device);
    if (markPublished && result == ESP_OK)
    {
        device.published = true;
    }

    if (useTransaction)
        endTransaction();

    return result;
}

// ============================================================================
// Private Methods - MQTT Publishing Helpers
// ============================================================================

/**
 * @brief Internal function for publishing MQTT discovery configuration
 *
 * Creates and sends the Home Assistant MQTT Discovery message for a device.
 * Configures the device as a binary sensor with device class "smoke".
 * No transaction management - caller is responsible.
 *
 * @param device Const reference to the device
 * @return ESP_OK on successful publish, ESP_ERR_INVALID_ARG for invalid parameters, ESP_FAIL on publish error
 */
esp_err_t GeniusDevicesService::_publishDeviceConfig(const GeniusDevice &device)
{
    if (_mqttClient == nullptr || _mqttSettingsService == nullptr)
        return ESP_ERR_INVALID_ARG;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String configTopic = discoveryPrefix + GATEWAY_HA_MQTT_DEVICE_PATH + device.smokeDetector.sn + "/config";

    JsonDocument config_jsonDoc;
    config_jsonDoc["~"] = discoveryPrefix + GATEWAY_HA_MQTT_DEVICE_PATH + device.smokeDetector.sn;
    config_jsonDoc["name"] = "Genius Plus X";
    config_jsonDoc["unique_id"] = String(device.smokeDetector.sn);
    config_jsonDoc["device_class"] = "smoke";
    config_jsonDoc["state_topic"] = "~/state";
    config_jsonDoc["schema"] = "json";
    config_jsonDoc["value_template"] = "{{value_json.state}}";

    // Get the current IP address and only add entity_picture if we have a valid IP
    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
    {
        config_jsonDoc["entity_picture"] = "http://" + localIP.toString() + "/hekatron-genius-plus-x.png";
    }

    JsonObject dev_jsonObj = config_jsonDoc["device"].to<JsonObject>();
    dev_jsonObj["identifiers"][0] = String(device.smokeDetector.sn);
    dev_jsonObj["manufacturer"] = "Hekatron Vertriebs GmbH";
    dev_jsonObj["model"] = "Genius Plus X";
    dev_jsonObj["name"] = "Rauchmelder";
    dev_jsonObj["serial_number"] = String(device.smokeDetector.sn);
    dev_jsonObj["suggested_area"] = device.location;
    dev_jsonObj["via_device"] = "genius-gateway-" + SettingValue::getUniqueId();
    
    // Add configuration URL if we have a valid IP (IP was already checked above for entity_picture)
    if (IPUtils::isSet(localIP))
    {
        dev_jsonObj["configuration_url"] = "http://" + localIP.toString() + "/gateway/smoke-detectors";
    }

    // Add attributes topic for entity attributes
    config_jsonDoc["json_attributes_topic"] = "~/attributes";

    String config_payload;
    serializeJson(config_jsonDoc, config_payload);

    int res = _mqttClient->publish(configTopic.c_str(), 0, true, config_payload.c_str(), 0, false);
    if (res == -1)
    {
        ESP_LOGE(GeniusDevices::TAG, "Failed to publish config for device SN %lu", device.smokeDetector.sn);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Internal function for publishing device attributes
 *
 * Publishes additional device information (production date, FM Basis X details)
 * as JSON attributes, displayed as entity attributes in Home Assistant.
 * No transaction management - caller is responsible.
 *
 * @param device Const reference to the device
 * @return ESP_OK on successful publish, ESP_ERR_INVALID_ARG for invalid parameters, ESP_FAIL on publish error
 */
esp_err_t GeniusDevicesService::_publishDeviceAttributes(const GeniusDevice &device)
{
    if (_mqttClient == nullptr || _mqttSettingsService == nullptr)
        return ESP_ERR_INVALID_ARG;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String attrTopic = discoveryPrefix + GATEWAY_HA_MQTT_DEVICE_PATH + device.smokeDetector.sn + "/attributes";
    JsonDocument attr_jsonDoc;

    // Add production date in dd.mm.yy format
    if (device.smokeDetector.productionDate > 0)
    {
        struct tm *tm = gmtime(&device.smokeDetector.productionDate);
        char dateBuf[9];
        strftime(dateBuf, sizeof(dateBuf), "%d.%m.%y", tm);
        attr_jsonDoc["Production Date"] = String(dateBuf);
    }

    // Add radio module information as flat attributes for better rendering
    if (device.radioModule.sn > 0)
    {
        attr_jsonDoc["FM Basis X - Serial"] = String(device.radioModule.sn);
    }

    // Add readout data when available
    if (device.readoutTime > 0)
    {
        if (device.smokeDetector.lastSelftest > 0)
        {
            struct tm *tm = gmtime(&device.smokeDetector.lastSelftest);
            char dateBuf[17];
            strftime(dateBuf, sizeof(dateBuf), "%d.%m.%y %H:%M", tm);
            attr_jsonDoc["Last Self-Test"] = String(dateBuf);
        }
        if (device.smokeDetector.lastAlarm > 0)
        {
            struct tm *tm = gmtime(&device.smokeDetector.lastAlarm);
            char dateBuf[17];
            strftime(dateBuf, sizeof(dateBuf), "%d.%m.%y %H:%M", tm);
            attr_jsonDoc["Last Alarm"] = String(dateBuf);
        }
        attr_jsonDoc["Alarms (Total)"] = device.smokeDetector.alarmCountTotal;
        attr_jsonDoc["Alarms (Last 3 Months)"] = device.smokeDetector.alarmCountLast3Months;
        if (device.radioModule.radioInterference > 0.0f)
            attr_jsonDoc["Radio Interference (%)"] = device.radioModule.radioInterference;
    }

    String attr_payload;
    serializeJson(attr_jsonDoc, attr_payload);

    int res = _mqttClient->publish(attrTopic.c_str(), 0, true, attr_payload.c_str(), 0, false);
    if (res == -1)
    {
        ESP_LOGW(GeniusDevices::TAG, "Failed to publish attributes for device SN %lu", device.smokeDetector.sn);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Internal function for publishing device state
 *
 * Sends the current alarm status (ON/OFF) to the MQTT state topic.
 * No transaction management - caller is responsible.
 *
 * @param device Const reference to the device
 * @return ESP_OK on successful publish, ESP_ERR_INVALID_ARG for invalid parameters, ESP_FAIL on publish error
 */
esp_err_t GeniusDevicesService::_publishDeviceState(const GeniusDevice &device)
{
    if (_mqttClient == nullptr || _mqttSettingsService == nullptr)
        return ESP_ERR_INVALID_ARG;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String stateTopic = discoveryPrefix + GATEWAY_HA_MQTT_DEVICE_PATH + device.smokeDetector.sn + "/state";

    JsonDocument state_jsonDoc;
    state_jsonDoc["state"] = device.isAlarming ? "ON" : "OFF";

    String payload;
    serializeJson(state_jsonDoc, payload);

    int res = _mqttClient->publish(stateTopic.c_str(), 0, true, payload.c_str(), 0, false);
    if (res == -1)
    {
        ESP_LOGE(GeniusDevices::TAG, "Failed to publish state for device SN %lu", device.smokeDetector.sn);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Unpublishes (removes) a device from Home Assistant MQTT
 * 
 * Sends an empty retained message to the discovery config topic,
 * which signals Home Assistant to remove the entity. This is called when
 * a device is deleted from the system.
 * 
 * No transaction management - caller is responsible.
 * 
 * @param smokeDetectorSN Smoke detector serial number to unpublish
 */
void GeniusDevicesService::_mqttUnpublishDevice(uint32_t smokeDetectorSN)
{
    if (_mqttClient == nullptr || _mqttSettingsService == nullptr)
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled || _cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        return;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String configTopic = discoveryPrefix + GATEWAY_HA_MQTT_DEVICE_PATH + smokeDetectorSN + "/config";

    // Send empty retained payload to remove the entity from Home Assistant
    _mqttClient->publish(configTopic.c_str(), 0, true, "");

    ESP_LOGI(GeniusDevices::TAG, "Unpublished MQTT entity for device with SN %lu.", smokeDetectorSN);
}

/**
 * @brief Publishes global alarm state to the simple alarm MQTT topic
 *
 * Publishes {"isAlarming": ..., "numAlarmingDevices": ...} to the configured
 * alarm topic with retain=true. Independent of Home Assistant integration -
 * works with any MQTT-capable smart home system.
 *
 * Called on every ALARM_STATE_CHANGE and explicitly on MQTT connect
 * from GeniusGateway::_mqttPublishTask(). The retained message ensures
 * subscribers always receive the current state, and overwrites any stale
 * retained message from before a restart.
 *
 * No transaction management - reads atomic cached state values (_isAlarming, _numAlarming).
 */
void GeniusDevicesService::mqttPublishSimpleAlarmState()
{
    if (_mqttClient == nullptr || !_mqttClient->connected())
        return;

    if (!_cachedMqttSettings.alarmEnabled)
        return;

    if (_cachedMqttSettings.alarmTopic.isEmpty())
    {
        ESP_LOGE(GeniusDevices::TAG, "Alarm MQTT topic is empty. Cannot publish alarm state.");
        return;
    }

    JsonDocument doc;
    doc["isAlarming"] = _isAlarming;
    doc["numAlarmingDevices"] = _numAlarming;

    String payload;
    serializeJson(doc, payload);

    if (_mqttClient->publish(_cachedMqttSettings.alarmTopic.c_str(), 0, true, payload.c_str()) == -1)
    {
        ESP_LOGE(GeniusDevices::TAG, "Failed to publish simple alarm state.");
    }
}

// ============================================================================
// Private Methods - Settings Management
// ============================================================================

/**
 * @brief Updates the local cache of MQTT settings
 *
 * Loads current MQTT settings from MqttSettingsService and stores them
 * in cache for faster access without repeated service calls.
 */
void GeniusDevicesService::_updateMqttSettingsCache()
{
    if (_mqttSettingsService != nullptr)
    {
        _cachedMqttSettings = _mqttSettingsService->getSettingsCopy();
        ESP_LOGV(GeniusDevices::TAG, "Updated cached MQTT settings (enabled: %d, prefix: %s)",
                 _cachedMqttSettings.HAIntegrationEnabled,
                 _cachedMqttSettings.HAMQTTDiscoveryPrefix.c_str());
    }
}
