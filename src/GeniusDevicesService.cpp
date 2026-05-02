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

    /* Sync HA sub-devices after device list changes (not for packet-originated adds or alarm state) */
    this->addUpdateHandler([&](const String &originId)
                           {
                               if (originId != GENIUS_DEVICE_ADDED_FROM_PACKET && originId != ALARM_STATE_CHANGE)
                               {
                                   _syncSmokeDetectorSubDevices();
                                   if (_haService->isReady())
                                   {
                                       for (const GeniusDevice &device : _state.devices)
                                           _publishSmokeDetectorAttributes(device.smokeDetector.sn, device);
                                   }
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
                                     _publishSmokeDetectorAttributes(device.smokeDetector.sn, device, false);
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

static const char *smokeDetectorModelName(GeniusSmokeDetector model)
{
    switch (model)
    {
        case GSD_GENIUS_H:      return "Genius H";
        case GSD_GENIUS_HX:     return "Genius Hx";
        case GSD_GENIUS_PLUS:   return "Genius Plus";
        case GSD_GENIUS_PLUS_X: return "Genius Plus X";
        default:                return "Genius Smoke Detector";
    }
}

static const char *radioModuleModelName(GeniusRadioModule model)
{
    switch (model)
    {
        case GRM_FM_BASIS:   return "FM Basis";
        case GRM_FM_PRO:     return "FM Pro";
        case GRM_FM_MCP:     return "FM MCP";
        case GRM_FM_BASIS_X: return "FM Basis X";
        case GRM_FM_PRO_X:   return "FM Pro X";
        default:             return nullptr;
    }
}

void GeniusDevicesService::_addSmokeDetectorSubDevice(const GeniusDevice &device)
{
    uint32_t sn = device.smokeDetector.sn;
    if (_haDevices.count(device.id))
        return;

    String haDeviceId = "genius-" + String(device.id); // stable HA identifier — independent of SN

    HADeviceIdentity identity;
    identity.id = haDeviceId;
    identity.name = smokeDetectorModelName(device.smokeDetector.model);
    identity.manufacturer = "Hekatron Vertriebs GmbH";
    identity.model = smokeDetectorModelName(device.smokeDetector.model);
    identity.serialNumber = String(sn);
    identity.suggestedArea = device.location;

    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
        identity.configurationUrl = "http://" + localIP.toString() + "/gateway/smoke-detectors";

    // Readout availability topic — published "online" when acoustic readout data is present
    uint32_t devId = device.id;
    String readoutAvailTopic = _haService->getBaseTopic() + "/genius-" + String(devId) + "/readout/avail";

    auto dev = std::make_unique<HADevice>(_haService, std::move(identity));

    // Initialise state caches keyed by stable device.id
    _alarmStates[device.id] = device.isAlarming;
    _readoutStates.emplace(device.id, SmokeDetectorReadout{});

    // ---- Main smoke alarm binary_sensor (Control) ---------------------------
    auto sensor = std::make_unique<HABinarySensor>(_haService, "smoke",
        [this, devId]() -> bool {
            auto it = _alarmStates.find(devId);
            return it != _alarmStates.end() ? it->second : false;
        },
        [this, devId](JsonObject &c) {
            c["device_class"] = "smoke";
            IPAddress ip = WiFi.localIP();
            if (IPUtils::isSet(ip))
                c["entity_picture"] = "http://" + ip.toString() + "/hekatron-genius-plus-x.png";
        });
    sensor->setName("Smoke Detector");
    HABinarySensor *rawSensor = dev->registerControl(std::move(sensor));

    // ---- Readout-derived Diagnostic entities --------------------------------
    // All share readoutAvailTopic — shown as "unavailable" until first readout.

    auto battLow = std::make_unique<HABinarySensor>(_haService, "battery_low",
        [this, devId]() -> bool { return _readoutStates[devId].batteryLowFault; });
    battLow->setName("Battery")
           .setDeviceClass("battery")
           .setAvailabilityTopic(readoutAvailTopic);
    HABinarySensor *rawBattLow = dev->registerDiagnostic(std::move(battLow));

    auto devFault = std::make_unique<HABinarySensor>(_haService, "device_fault",
        [this, devId]() -> bool { return _readoutStates[devId].deviceFault; });
    devFault->setName("Smoke Detector State")
            .setDeviceClass("problem")
            .setAvailabilityTopic(readoutAvailTopic);
    HABinarySensor *rawDevFault = dev->registerDiagnostic(std::move(devFault));

    auto radioFault = std::make_unique<HABinarySensor>(_haService, "radio_fault",
        [this, devId]() -> bool { return _readoutStates[devId].radioNetworkFault; });
    radioFault->setName("Radio Module State")
              .setDeviceClass("problem")
              .setAvailabilityTopic(readoutAvailTopic);
    HABinarySensor *rawRadioFault = dev->registerDiagnostic(std::move(radioFault));

    auto deinstall = std::make_unique<HASensor>(_haService, "deinstallation_count",
        [this, devId]() -> String { return String(_readoutStates[devId].deinstallationCount); },
        [](JsonObject &c) { c["state_class"] = "total_increasing"; });
    deinstall->setName("Deinstallation Count")
             .setIcon("mdi:autorenew")
             .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawDeinstall = dev->registerDiagnostic(std::move(deinstall));

    auto lastReadout = std::make_unique<HASensor>(_haService, "last_readout",
        [this, devId]() -> String { return _readoutStates[devId].lastReadout; },
        [](JsonObject &c) { c["device_class"] = "timestamp"; });
    lastReadout->setName("Last Service")
               .setIcon("mdi:clock-check-outline")
               .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawLastReadout = dev->registerDiagnostic(std::move(lastReadout));

    auto rmModel = std::make_unique<HASensor>(_haService, "radio_module_model",
        [this, devId]() -> String { return _readoutStates[devId].radioModuleModel; });
    rmModel->setName("Radio Module Model")
           .setIcon("mdi:radio-tower")
           .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawRmModel = dev->registerDiagnostic(std::move(rmModel));

    auto alarmLineId = std::make_unique<HASensor>(_haService, "alarm_line_id",
        [this, devId]() -> String {
            uint32_t id = _readoutStates[devId].lineId;
            return id > 0 ? String(id) : "None";
        });
    alarmLineId->setName("Alarm Line ID")
               .setIcon("mdi:identifier")
               .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawAlarmLineId = dev->registerDiagnostic(std::move(alarmLineId));

    auto alarmLine = std::make_unique<HASensor>(_haService, "alarm_line",
        [this, devId]() -> String {
            const String &l = _readoutStates[devId].alarmLine;
            return l.isEmpty() ? "None" : l;
        });
    alarmLine->setName("Alarm Line")
             .setIcon("mdi:alarm-light-outline")
             .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawAlarmLine = dev->registerDiagnostic(std::move(alarmLine));

    auto prodDate = std::make_unique<HASensor>(_haService, "production_date",
        [this, devId]() -> String { return _readoutStates[devId].productionDate; });
    prodDate->setName("Production Date")
            .setIcon("mdi:calendar-badge")
            .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawProdDate = dev->registerDiagnostic(std::move(prodDate));

    auto rmSer = std::make_unique<HASensor>(_haService, "radio_module_serial",
        [this, devId]() -> String {
            uint32_t sn = _readoutStates[devId].rmSerial;
            return sn > 0 ? String(sn) : String("Unknown");
        });
    rmSer->setName("Radio Module Serial")
         .setIcon("mdi:barcode")
         .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawRmSer = dev->registerDiagnostic(std::move(rmSer));

    auto alarmTotal = std::make_unique<HASensor>(_haService, "alarm_count_total",
        [this, devId]() -> String { return String(_readoutStates[devId].alarmCountTotal); },
        [](JsonObject &c) { c["state_class"] = "total_increasing"; });
    alarmTotal->setName("Alarms (Total)")
              .setIcon("mdi:alarm-light")
              .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawAlarmTotal = dev->registerDiagnostic(std::move(alarmTotal));

    auto alarm3m = std::make_unique<HASensor>(_haService, "alarm_count_3m",
        [this, devId]() -> String { return String(_readoutStates[devId].alarmCount3m); },
        [](JsonObject &c) { c["state_class"] = "measurement"; });
    alarm3m->setName("Alarms (3 Months)")
           .setIcon("mdi:calendar-clock")
           .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawAlarm3m = dev->registerDiagnostic(std::move(alarm3m));

    auto radioIntf = std::make_unique<HASensor>(_haService, "radio_interference",
        [this, devId]() -> String { return String(_readoutStates[devId].radioInterference, 1); },
        [](JsonObject &c) {
            c["unit_of_measurement"] = "%";
            c["state_class"] = "measurement";
        });
    radioIntf->setName("Radio Interference")
             .setIcon("mdi:wifi-alert")
             .setAvailabilityTopic(readoutAvailTopic);
    HASensor *rawRadioIntf = dev->registerDiagnostic(std::move(radioIntf));

    // -------------------------------------------------------------------------
    HADevice *rawDev = _haService->addSubDevice(std::move(dev));
    _haDevices[device.id] = {
        rawDev, rawSensor,
        rawBattLow, rawDevFault, rawRadioFault,
        rawDeinstall, rawLastReadout,
        rawRmModel, rawAlarmLineId, rawAlarmLine,
        rawProdDate, rawRmSer,
        rawAlarmTotal, rawAlarm3m, rawRadioIntf,
        readoutAvailTopic
    };

    // Publish attributes immediately if MQTT is already up
    if (_haService->isReady())
        _publishSmokeDetectorAttributes(sn, device);
}

void GeniusDevicesService::_removeSmokeDetectorSubDevice(uint32_t deviceId)
{
    _alarmStates.erase(deviceId);
    _readoutStates.erase(deviceId);
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
                id.name = smokeDetectorModelName(device.smokeDetector.model);
                id.serialNumber = String(device.smokeDetector.sn);
                id.suggestedArea = device.location;
                id.model = smokeDetectorModelName(device.smokeDetector.model);
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

esp_err_t GeniusDevicesService::_publishSmokeDetectorAttributes(uint32_t sn, const GeniusDevice &device, bool emitStates)
{
    auto it = _haDevices.find(device.id);
    if (it == _haDevices.end() || it->second.device == nullptr)
        return ESP_ERR_NOT_FOUND;
    if (!_haService->isReady())
        return ESP_ERR_INVALID_STATE;

    SmokeDetectorHA &ha = it->second;

    // ---- Readout-derived entities: update cache + publish state -------------
    if (device.readoutTime > 0)
    {
        SmokeDetectorReadout &rs = _readoutStates[device.id];

        rs.batteryLowFault    = device.smokeDetector.batteryLowFault;
        rs.deviceFault        = device.smokeDetector.deviceFault;
        rs.radioNetworkFault  = device.radioModule.radioNetworkFault;
        rs.deinstallationCount = device.smokeDetector.deinstallationCount;
        rs.lastReadout        = Utils::time_t_to_iso8601(device.readoutTime);

        const char *rmName = radioModuleModelName(device.radioModule.model);
        rs.radioModuleModel   = rmName ? String(rmName) : String();

        rs.lineId = device.radioModule.lineId;

        if (device.radioModule.lineCharacter)
            rs.alarmLine = String(device.radioModule.lineCharacter) + "." + String(device.radioModule.lineNumber);
        else
            rs.alarmLine = String();

        rs.rmSerial        = device.radioModule.sn;
        rs.alarmCountTotal = device.smokeDetector.alarmCountTotal;
        rs.alarmCount3m    = device.smokeDetector.alarmCountLast3Months;
        rs.radioInterference = device.radioModule.radioInterference;

        rs.productionDate = "";
        if (device.smokeDetector.productionDate > 0)
        {
            struct tm *pd_tm = gmtime(&device.smokeDetector.productionDate);
            char pdBuf[9];
            strftime(pdBuf, sizeof(pdBuf), "%d.%m.%y", pd_tm);
            rs.productionDate = String(pdBuf);
        }

        // Mark readout entities available; push states only when not called from
        // an onPublishAll context (HADevice::publishAll will emit them right after).
        ha.device->publish(ha.readoutAvailTopic, "online");
        if (emitStates)
        {
            if (ha.batteryLow)       ha.batteryLow->publishState();
            if (ha.deviceFault)      ha.deviceFault->publishState();
            if (ha.radioFault)       ha.radioFault->publishState();
            if (ha.deinstallCount)   ha.deinstallCount->publishState();
            if (ha.lastReadout)      ha.lastReadout->publishState();
            if (ha.rmModel)          ha.rmModel->publishState();
            if (ha.alarmLineId)      ha.alarmLineId->publishState();
            if (ha.alarmLine)        ha.alarmLine->publishState();
            if (ha.productionDate)   ha.productionDate->publishState();
            if (ha.rmSerial)         ha.rmSerial->publishState();
            if (ha.alarmCountTotal)  ha.alarmCountTotal->publishState();
            if (ha.alarmCount3m)     ha.alarmCount3m->publishState();
            if (ha.radioInterference) ha.radioInterference->publishState();
        }
    }

    return ESP_OK;
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
