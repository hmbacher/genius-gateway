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
                                                                                                                                                        _mqttSettingsService(mqttSettingsService),
                                                                                                                                                        _haService(sveltekit->getHAService())
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

    /* Sync HA sub-devices after device list changes (not for packet-originated adds or alarm state) */
    this->addUpdateHandler([&](const String &originId)
                           {
                               if (originId != GENIUS_DEVICE_ADDED_FROM_PACKET && originId != ALARM_STATE_CHANGE)
                               {
                                   _syncSmokeDetectorSubDevices();
                               } },
                           false);

    /* Publish simple alarm topic on alarm state changes */
    this->addUpdateHandler([&](const String &originId)
                           {
                               if (originId == ALARM_STATE_CHANGE)
                                   mqttPublishSimpleAlarmState();
                           },
                           false);

    /* Update cache when MQTT settings change */
    if (_mqttSettingsService != nullptr)
    {
        _mqttSettingsService->addUpdateHandler([this](const String &originId)
                                               {
                                                   this->_updateMqttSettingsCache();
                                                   this->mqttPublishSimpleAlarmState(); },
                                               false);
    }

    /* Republish smoke detector attributes on every MQTT (re)connect */
    _haService->onPublishAll([this]()
                             {
                                 beginTransaction();
                                 for (const GeniusDevice &device : _state.devices)
                                     _publishSmokeDetectorAttributes(device.smokeDetector.sn, device);
                                 endTransaction(); });

    /* Register sub-devices for devices already loaded from FS */
    _syncSmokeDetectorSubDevices();
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

StateUpdateResult GeniusDevices::update(JsonObject &root, GeniusDevices &geniusDevices, const String &originId)
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
        geniusDevices.deletedDeviceIds.clear();
        for (const auto &oldDevice : geniusDevices.devices)
        {
            if (std::find(processedDeviceIds.begin(), processedDeviceIds.end(), oldDevice.id) == processedDeviceIds.end())
            {
                // Device was deleted - store stable device ID for unpublishing
                geniusDevices.deletedDeviceIds.push_back(oldDevice.id);
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

void GeniusDevicesService::mqttPublishAllDevices(bool onlyUnpublished)
{
    (void)onlyUnpublished; // sync is idempotent; HAService handles config/state republish
    _syncSmokeDetectorSubDevices();
}

void GeniusDevicesService::mqttPublishAllDevicesState(bool onlyUnpublished)
{
    if (!_haService->isReady())
        return;

    beginTransaction();
    for (GeniusDevice &device : _state.devices)
    {
        if (!onlyUnpublished || !device.published)
        {
            uint32_t sn = device.smokeDetector.sn;
            _alarmStates[device.id] = device.isAlarming;
            auto it = _haDevices.find(device.id);
            if (it != _haDevices.end() && it->second.sensor != nullptr)
                it->second.sensor->publishState();
        }
    }
    endTransaction();
}

esp_err_t GeniusDevicesService::mqttPublishDeviceState(uint32_t smokeDetectorSN, bool useTransaction, bool markPublished)
{
    if (!_haService->isReady())
        return ESP_ERR_INVALID_STATE;

    if (useTransaction)
        beginTransaction();

    esp_err_t result = ESP_ERR_NOT_FOUND;
    for (GeniusDevice &device : _state.devices)
    {
        if (device.smokeDetector.sn == smokeDetectorSN)
        {
            _alarmStates[device.id] = device.isAlarming;
            auto it = _haDevices.find(device.id);
            if (it != _haDevices.end() && it->second.sensor != nullptr)
            {
                it->second.sensor->publishState();
                result = ESP_OK;
                if (markPublished)
                    device.published = true;
            }
            break;
        }
    }

    if (useTransaction)
        endTransaction();

    return result;
}

// ============================================================================
// Private Methods - HA Sub-device Management
// ============================================================================

void GeniusDevicesService::_addSmokeDetectorSubDevice(const GeniusDevice &device)
{
    uint32_t sn = device.smokeDetector.sn;
    if (_haDevices.count(device.id))
        return;

    String haDeviceId = "genius-" + String(device.id); // stable HA identifier — independent of SN

    HADeviceIdentity identity;
    identity.id = haDeviceId;
    identity.name = "Rauchmelder";
    identity.manufacturer = "Hekatron Vertriebs GmbH";
    identity.model = "Genius Plus X";
    identity.serialNumber = String(sn);
    identity.suggestedArea = device.location;

    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
        identity.configurationUrl = "http://" + localIP.toString() + "/gateway/smoke-detectors";

    auto dev = std::make_unique<HADevice>(_haService, std::move(identity));

    // Initialise alarm state from current device data (keyed by stable device.id)
    _alarmStates[device.id] = device.isAlarming;

    // Create binary_sensor entity — capture stable device.id so state lookup
    // survives serial-number changes without recreating the sensor.
    uint32_t devId = device.id;
    auto sensor = std::make_unique<HABinarySensor>(_haService, "smoke",
        [this, devId]() -> bool {
            auto it = _alarmStates.find(devId);
            return it != _alarmStates.end() ? it->second : false;
        },
        [this, devId](JsonObject &c) {
            c["device_class"] = "smoke";
            c["json_attributes_topic"] = _haService->getBaseTopic() + "/genius-" + String(devId) + "/smoke/attributes";
            IPAddress ip = WiFi.localIP();
            if (IPUtils::isSet(ip))
                c["entity_picture"] = "http://" + ip.toString() + "/hekatron-genius-plus-x.png";
        });
    sensor->setName("Genius Plus X");

    HABinarySensor *rawSensor = dev->registerControl(std::move(sensor));
    HADevice *rawDev = _haService->addSubDevice(std::move(dev));
    _haDevices[device.id] = {rawDev, rawSensor};

    // Publish attributes immediately if MQTT is already up
    if (_haService->isReady())
        _publishSmokeDetectorAttributes(sn, device);
}

void GeniusDevicesService::_removeSmokeDetectorSubDevice(uint32_t deviceId)
{
    _alarmStates.erase(deviceId);
    _haDevices.erase(deviceId);
    _haService->removeSubDevice("genius-" + String(deviceId));
}

void GeniusDevicesService::_syncSmokeDetectorSubDevices()
{
    beginTransaction();

    for (uint32_t deviceId : _state.deletedDeviceIds)
        _removeSmokeDetectorSubDevice(deviceId);
    _state.deletedDeviceIds.clear();

    for (GeniusDevice &device : _state.devices)
    {
        if (!_haDevices.count(device.id))
        {
            _addSmokeDetectorSubDevice(device);
            device.published = true;
        }
        else if (!device.published)
        {
            // Config changed (e.g. location or serial number) — update the
            // existing HADevice identity in place and republish its discovery.
            // Avoids remove+re-add, which causes HA to ignore the new
            // suggested_area because its device registry entry persists (Bug 2).
            auto it = _haDevices.find(device.id);
            if (it != _haDevices.end() && it->second.device != nullptr)
            {
                HADeviceIdentity &id = it->second.device->identity();
                id.serialNumber = String(device.smokeDetector.sn);
                id.suggestedArea = device.location;
                IPAddress localIP = WiFi.localIP();
                if (IPUtils::isSet(localIP))
                    id.configurationUrl = "http://" + localIP.toString() + "/gateway/smoke-detectors";
                // Keep alarm state map consistent with current SN
                _alarmStates[device.id] = device.isAlarming;
                if (_haService->isReady())
                    it->second.device->publishAll();
            }
            device.published = true;
        }
    }

    endTransaction();
}

esp_err_t GeniusDevicesService::_publishSmokeDetectorAttributes(uint32_t sn, const GeniusDevice &device)
{
    auto it = _haDevices.find(device.id);
    if (it == _haDevices.end() || it->second.device == nullptr)
        return ESP_ERR_NOT_FOUND;
    if (!_haService->isReady())
        return ESP_ERR_INVALID_STATE;

    String attrTopic = it->second.device->getBaseTopic() + "/smoke/attributes";

    JsonDocument doc;
    if (device.smokeDetector.productionDate > 0)
    {
        struct tm *tm = gmtime(&device.smokeDetector.productionDate);
        char dateBuf[9];
        strftime(dateBuf, sizeof(dateBuf), "%d.%m.%y", tm);
        doc["Production Date"] = String(dateBuf);
    }
    if (device.radioModule.sn > 0)
    {
        doc["FM Basis X - Serial"] = String(device.radioModule.sn);
        if (device.radioModule.productionDate > 0)
        {
            struct tm *tm = gmtime(&device.radioModule.productionDate);
            char dateBuf[9];
            strftime(dateBuf, sizeof(dateBuf), "%d.%m.%y", tm);
            doc["FM Basis X - Production Date"] = String(dateBuf);
        }
    }

    String payload;
    serializeJson(doc, payload);

    return it->second.device->publish(attrTopic, payload) ? ESP_OK : ESP_FAIL;
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
        ESP_LOGV(GeniusDevices::TAG, "Updated cached alarm MQTT settings (alarmEnabled: %d, topic: %s)",
                 _cachedMqttSettings.alarmEnabled,
                 _cachedMqttSettings.alarmTopic.c_str());
    }
}
