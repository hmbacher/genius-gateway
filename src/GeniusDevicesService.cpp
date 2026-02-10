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

    /* Update cache and republish MQTT when MQTT settings change */
    if (_mqttSettingsService != nullptr)
    {
        _mqttSettingsService->addUpdateHandler([this](const String &originId)
                                               {
                                                   this->_updateMqttSettingsCache();
                                                   this->mqttPublishAllDevices(); },
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
    GeniusDevice newDevice = GeniusDevice(
        GeniusComponent<GeniusSmokeDetector>(static_cast<GeniusSmokeDetector>(GSD_UNKNOWN), snSmokeDetector, 0),
        GeniusComponent<GeniusRadioModule>(static_cast<GeniusRadioModule>(GRM_UNKNOWN), snRadioModule, 0),
        GENIUS_DEVICE_DEFAULT_LOCATION);

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

StateUpdateResult GeniusDevices::update(JsonObject &root, GeniusDevices &geniusDevices)
{
    bool hasChanges = false;

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
                GeniusComponent<GeniusSmokeDetector>(
                    static_cast<GeniusSmokeDetector>(smokeDetectorJson["model"].as<int>()),
                    smokeDetectorJson["sn"].as<uint32_t>(),
                    Utils::iso8601_to_time_t(smokeDetectorJson["productionDate"].as<String>())),
                GeniusComponent<GeniusRadioModule>(
                    static_cast<GeniusRadioModule>(radioModuleJson["model"].as<int>()),
                    radioModuleJson["sn"].as<uint32_t>(),
                    Utils::iso8601_to_time_t(radioModuleJson["productionDate"].as<String>())),
                jsonDeviceArrItem["location"].as<String>(),
                deviceId); // Use the ID from JSON

            // Set optional properties with defaults
            newDevice.isAlarming = jsonDeviceArrItem["isAlarming"].is<bool>() ? jsonDeviceArrItem["isAlarming"].as<bool>() : false;
            newDevice.registration = jsonDeviceArrItem["registration"].is<int>() ? static_cast<genius_device_registration_t>(jsonDeviceArrItem["registration"].as<int>()) : GDR_MANUAL;

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

            // Update smoke detector component...
            // ...model
            GeniusSmokeDetector newSmokeDetectorModel = static_cast<GeniusSmokeDetector>(smokeDetectorJson["model"].as<int>());
            if (updatedDevice.smokeDetector.model != newSmokeDetectorModel)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Old model: %d, New model: %d",
                         updatedDevice.location.c_str(),
                         static_cast<int>(updatedDevice.smokeDetector.model),
                         static_cast<int>(newSmokeDetectorModel));

                updatedDevice.smokeDetector.model = newSmokeDetectorModel;
                deviceChanged = true;
            }
            // ...serial number
            uint32_t newSmokeDetectorSN = smokeDetectorJson["sn"].as<uint32_t>();
            if (updatedDevice.smokeDetector.sn != newSmokeDetectorSN)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Old SN: %lu, New SN: %lu",
                         updatedDevice.location.c_str(),
                         updatedDevice.smokeDetector.sn,
                         newSmokeDetectorSN);

                updatedDevice.smokeDetector.sn = newSmokeDetectorSN;
                deviceChanged = true;
            }
            // ...production date
            time_t newSmokeDetectorProdDate = Utils::iso8601_to_time_t(smokeDetectorJson["productionDate"].as<String>());
            if (updatedDevice.smokeDetector.productionDate != newSmokeDetectorProdDate)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Old production date: %s, New production date: %s",
                         updatedDevice.location.c_str(),
                         Utils::time_t_to_iso8601(updatedDevice.smokeDetector.productionDate).c_str(),
                         Utils::time_t_to_iso8601(newSmokeDetectorProdDate).c_str());

                updatedDevice.smokeDetector.productionDate = newSmokeDetectorProdDate;
                deviceChanged = true;
            }

            // Update radio module component...
            // ...model
            GeniusRadioModule newRadioModuleModel = static_cast<GeniusRadioModule>(radioModuleJson["model"].as<int>());
            if (updatedDevice.radioModule.model != newRadioModuleModel)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Old model: %d, New model: %d",
                         updatedDevice.location.c_str(),
                         static_cast<int>(updatedDevice.radioModule.model),
                         static_cast<int>(newRadioModuleModel));

                updatedDevice.radioModule.model = newRadioModuleModel;
                deviceChanged = true;
            }
            // ...serial number
            uint32_t newRadioModuleSN = radioModuleJson["sn"].as<uint32_t>();
            if (updatedDevice.radioModule.sn != newRadioModuleSN)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Old SN: %lu, New SN: %lu",
                         updatedDevice.location.c_str(),
                         updatedDevice.radioModule.sn,
                         newRadioModuleSN);

                updatedDevice.radioModule.sn = newRadioModuleSN;
                deviceChanged = true;
            }
            // ...production date
            time_t newRadioModuleProdDate = Utils::iso8601_to_time_t(radioModuleJson["productionDate"].as<String>());
            if (updatedDevice.radioModule.productionDate != newRadioModuleProdDate)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Old production date: %s, New production date: %s",
                         updatedDevice.location.c_str(),
                         Utils::time_t_to_iso8601(updatedDevice.radioModule.productionDate).c_str(),
                         Utils::time_t_to_iso8601(newRadioModuleProdDate).c_str());

                updatedDevice.radioModule.productionDate = newRadioModuleProdDate;
                deviceChanged = true;
            }

            // Update location
            String newLocation = jsonDeviceArrItem["location"].as<String>();
            if (updatedDevice.location != newLocation)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Old location: '%s', New location: '%s'",
                         updatedDevice.location.c_str(),
                         updatedDevice.location.c_str(),
                         newLocation.c_str());

                updatedDevice.location = newLocation;
                deviceChanged = true;
            }

            // NOTE: The following attributes are managed internally and should not be updated from JSON:
            // - isAlarming: Controlled by alarm detection system
            // - registration: Set when device is first added/detected
            // - alarms: Managed by alarm start/stop events

            /*
            // Update isAlarming
            bool newIsAlarming = jsonDeviceArrItem["isAlarming"].is<bool>() ?
                jsonDeviceArrItem["isAlarming"].as<bool>() : false;
            if (updatedDevice.isAlarming != newIsAlarming)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Old isAlarming: %s, New isAlarming: %s",
                         updatedDevice.location.c_str(),
                         updatedDevice.isAlarming ? "true" : "false",
                         newIsAlarming ? "true" : "false");

                updatedDevice.isAlarming = newIsAlarming;
                deviceChanged = true;
            }

            // Update registration
            genius_device_registration_t newRegistration = jsonDeviceArrItem["registration"].is<int>() ?
                static_cast<genius_device_registration_t>(jsonDeviceArrItem["registration"].as<int>()) : GDR_MANUAL;
            if (updatedDevice.registration != newRegistration)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Old registration: %d, New registration: %d",
                         updatedDevice.location.c_str(),
                         static_cast<int>(updatedDevice.registration),
                         static_cast<int>(newRegistration));

                existingDevice->registration = newRegistration;
                deviceChanged = true;
            }

            // Update alarms (for simplicity, we replace the entire alarms vector if it has changed)
            // A more sophisticated approach would compare individual alarms
            std::vector<genius_device_alarm_t> newAlarms;
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

                    newAlarms.push_back(genius_device_alarm_t{
                        .startTime = Utils::iso8601_to_time_t(jsonAlarm["startTime"].as<String>()),
                        .endTime = Utils::iso8601_to_time_t(jsonAlarm["endTime"].as<String>()),
                        .endingReason = static_cast<genius_alarm_ending_t>(jsonAlarm["endingReason"].as<int>())});
                }
            }

            // Compare alarms (simple size comparison - could be more sophisticated)
            if (updatedDevice.alarms.size() != newAlarms.size())
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': Alarms count changed from %d to %d.",
                         updatedDevice.location.c_str(),
                         updatedDevice.alarms.size(),
                         newAlarms.size());

                updatedDevice.alarms = newAlarms;
                deviceChanged = true;
            }
            */

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
    config_jsonDoc["unique_id"] = device.smokeDetector.sn;
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
    dev_jsonObj["identifiers"] = device.smokeDetector.sn;
    dev_jsonObj["manufacturer"] = "Hekatron Vertriebs GmbH";
    dev_jsonObj["model"] = "Genius Plus X";
    dev_jsonObj["name"] = "Rauchmelder";
    dev_jsonObj["serial_number"] = device.smokeDetector.sn;
    dev_jsonObj["suggested_area"] = device.location;
    dev_jsonObj["via_device"] = "genius-gateway-" + WiFi.macAddress();
    
    // Add configuration URL if we have a valid IP (IP was already checked above for entity_picture)
    if (IPUtils::isSet(localIP))
    {
        dev_jsonObj["configuration_url"] = "http://" + localIP.toString() + "/gateway/smoke-detectors";
    }

    // Add attributes topic for entity attributes
    config_jsonDoc["json_attributes_topic"] = "~/attributes";

    String config_payload;
    serializeJson(config_jsonDoc, config_payload);

    return _mqttClient->publish(configTopic.c_str(), 0, true, config_payload.c_str()) != -1 ? ESP_OK : ESP_FAIL;
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

        if (device.radioModule.productionDate > 0)
        {
            struct tm *tm = gmtime(&device.radioModule.productionDate);
            char dateBuf[9];
            strftime(dateBuf, sizeof(dateBuf), "%d.%m.%y", tm);
            attr_jsonDoc["FM Basis X - Production Date"] = String(dateBuf);
        }
    }

    String attr_payload;
    serializeJson(attr_jsonDoc, attr_payload);

    return _mqttClient->publish(attrTopic.c_str(), 0, true, attr_payload.c_str()) != -1 ? ESP_OK : ESP_FAIL;
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

    return _mqttClient->publish(stateTopic.c_str(), 0, true, payload.c_str()) != -1 ? ESP_OK : ESP_FAIL;
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
