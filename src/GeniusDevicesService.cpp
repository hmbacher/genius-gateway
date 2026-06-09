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
#include <esp_random.h>
#include <Utils.hpp>

GeniusDevicesService::GeniusDevicesService(ESP32SvelteKit *sveltekit, PsychicMqttClient *mqttClient, AlarmPublishingSettingsService *alarmPublishingSettings) : _httpEndpoint(GeniusDevices::read,
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
                                                                                                                                                        _server(sveltekit->getServer()),
                                                                                                                                                        _securityManager(sveltekit->getSecurityManager()),
                                                                                                                                                        _isAlarming(false),
                                                                                                                                                        _numAlarming(0),
                                                                                                                                                        _mqttClient(mqttClient),
                                                                                                                                                        _alarmPublishingSettings(alarmPublishingSettings),
                                                                                                                                                        _haService(sveltekit->getHAService())
{
    _importMutex = xSemaphoreCreateRecursiveMutex();
}

// ============================================================================
// Public Methods - Lifecycle
// ============================================================================

// Small helper to write a JSON {success, reason} reply with an HTTP status code.
static esp_err_t replyJson(PsychicRequest *request, int status, const char *body)
{
    return request->reply(status, "application/json", body);
}

// Serialize one device into a 200 response.
static esp_err_t replyDevice(PsychicRequest *request, GeniusDevice &device)
{
    PsychicJsonResponse response(request, false);
    JsonObject root = response.getRoot().to<JsonObject>();
    device.toJson(root);
    return response.send();
}

void GeniusDevicesService::begin()
{
    _httpEndpoint.begin();
    _fsPersistence.readFromFS();

    // Spawn the post-commit task — it sleeps on a notification and runs the
    // update-handler chain (FS write + HA sync) for chunked-import commits so
    // those don't block the HTTP server task.
    xTaskCreatePinnedToCore(
        _postCommitTaskImpl,
        "gd-post-commit",
        6144,
        this,
        4,
        &_postCommitTaskHandle,
        ESP32SVELTEKIT_RUNNING_CORE);

    // ---- Per-device CRUD: PUT /rest/gateway-devices/device (upsert by id) ----
    _server->on(
        GATEWAY_DEVICES_DEVICE_PATH,
        HTTP_PUT,
        _securityManager->wrapCallback(
            [this](PsychicRequest *request, JsonVariant &json) -> esp_err_t {
                if (!json.is<JsonObject>())
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Body must be a device object\"}");
                GeniusDevice updated(GeniusSmokeDetectorInfo{}, GeniusRadioModuleInfo{}, String(), 0);
                switch (this->upsertDevice(json, updated))
                {
                case DeviceOpResult::OK:
                case DeviceOpResult::UNCHANGED:
                    return replyDevice(request, updated);
                case DeviceOpResult::INVALID_BODY:
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Invalid device payload (id required and non-zero)\"}");
                case DeviceOpResult::LIMIT_REACHED:
                    return replyJson(request, 409, "{\"success\":false,\"reason\":\"Device limit reached\"}");
                default:
                    return replyJson(request, 500, "{\"success\":false}");
                }
            },
            AuthenticationPredicates::IS_ADMIN));

    // ---- Per-device CRUD: POST /rest/gateway-devices/device/delete  body: { id } ----
    _server->on(
        GATEWAY_DEVICES_DEVICE_DELETE_PATH,
        HTTP_POST,
        _securityManager->wrapCallback(
            [this](PsychicRequest *request, JsonVariant &json) -> esp_err_t {
                if (!json.is<JsonObject>() || !json["id"].is<uint32_t>())
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Missing id\"}");
                uint32_t id = json["id"].as<uint32_t>();
                switch (this->removeDevice(id))
                {
                case DeviceOpResult::OK:
                    return replyJson(request, 200, "{\"success\":true}");
                case DeviceOpResult::NOT_FOUND:
                    return replyJson(request, 404, "{\"success\":false,\"reason\":\"Device not found\"}");
                case DeviceOpResult::INVALID_BODY:
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Invalid id\"}");
                default:
                    return replyJson(request, 500, "{\"success\":false}");
                }
            },
            AuthenticationPredicates::IS_ADMIN));

    // ---- Per-device CRUD: POST /rest/gateway-devices/reorder  body: { order: [id...] } ----
    _server->on(
        GATEWAY_DEVICES_REORDER_PATH,
        HTTP_POST,
        _securityManager->wrapCallback(
            [this](PsychicRequest *request, JsonVariant &json) -> esp_err_t {
                if (!json.is<JsonObject>() || !json["order"].is<JsonArray>())
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Missing order array\"}");
                std::vector<uint32_t> order;
                order.reserve(json["order"].as<JsonArray>().size());
                for (JsonVariant v : json["order"].as<JsonArray>())
                    order.push_back(v.as<uint32_t>());
                switch (this->reorderDevices(order))
                {
                case DeviceOpResult::OK:
                case DeviceOpResult::UNCHANGED:
                    return replyJson(request, 200, "{\"success\":true}");
                case DeviceOpResult::INVALID_BODY:
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Order does not match current device set\"}");
                default:
                    return replyJson(request, 500, "{\"success\":false}");
                }
            },
            AuthenticationPredicates::IS_ADMIN));

    // ---- Chunked import: POST /rest/gateway-devices/import/begin ----
    _server->on(
        GATEWAY_DEVICES_IMPORT_BEGIN_PATH,
        HTTP_POST,
        _securityManager->wrapRequest(
            [this](PsychicRequest *request) -> esp_err_t {
                String token;
                switch (this->importBegin(token))
                {
                case ImportResult::OK: {
                    String body = String("{\"sessionId\":\"") + token + "\"}";
                    return replyJson(request, 200, body.c_str());
                }
                case ImportResult::BUSY:
                    return replyJson(request, 409, "{\"success\":false,\"reason\":\"Another import session is in progress\"}");
                default:
                    return replyJson(request, 500, "{\"success\":false}");
                }
            },
            AuthenticationPredicates::IS_ADMIN));

    // ---- Chunked import: POST /rest/gateway-devices/import/chunk  body: { sessionId, devices: [...] } ----
    _server->on(
        GATEWAY_DEVICES_IMPORT_CHUNK_PATH,
        HTTP_POST,
        _securityManager->wrapCallback(
            [this](PsychicRequest *request, JsonVariant &json) -> esp_err_t {
                if (!json.is<JsonObject>() || !json["sessionId"].is<const char *>() || !json["devices"].is<JsonArray>())
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Body must include sessionId and devices array\"}");
                String token = json["sessionId"].as<String>();
                switch (this->importChunk(token, json["devices"].as<JsonArray>()))
                {
                case ImportResult::OK:
                    return replyJson(request, 200, "{\"success\":true}");
                case ImportResult::BAD_SESSION:
                    return replyJson(request, 410, "{\"success\":false,\"reason\":\"Unknown or expired session\"}");
                case ImportResult::BAD_PAYLOAD:
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Malformed device entry in chunk\"}");
                case ImportResult::LIMIT_REACHED:
                    return replyJson(request, 409, "{\"success\":false,\"reason\":\"Device limit reached\"}");
                default:
                    return replyJson(request, 500, "{\"success\":false}");
                }
            },
            AuthenticationPredicates::IS_ADMIN));

    // ---- Chunked import: POST /rest/gateway-devices/import/commit  body: { sessionId } ----
    _server->on(
        GATEWAY_DEVICES_IMPORT_COMMIT_PATH,
        HTTP_POST,
        _securityManager->wrapCallback(
            [this](PsychicRequest *request, JsonVariant &json) -> esp_err_t {
                if (!json.is<JsonObject>() || !json["sessionId"].is<const char *>())
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Missing sessionId\"}");
                String token = json["sessionId"].as<String>();
                switch (this->importCommit(token))
                {
                case ImportResult::OK:
                    return replyJson(request, 200, "{\"success\":true}");
                case ImportResult::BAD_SESSION:
                    return replyJson(request, 410, "{\"success\":false,\"reason\":\"Unknown or expired session\"}");
                default:
                    return replyJson(request, 500, "{\"success\":false}");
                }
            },
            AuthenticationPredicates::IS_ADMIN));

    // ---- Chunked import: POST /rest/gateway-devices/import/abort  body: { sessionId } ----
    _server->on(
        GATEWAY_DEVICES_IMPORT_ABORT_PATH,
        HTTP_POST,
        _securityManager->wrapCallback(
            [this](PsychicRequest *request, JsonVariant &json) -> esp_err_t {
                if (!json.is<JsonObject>() || !json["sessionId"].is<const char *>())
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Missing sessionId\"}");
                String token = json["sessionId"].as<String>();
                switch (this->importAbort(token))
                {
                case ImportResult::OK:
                    return replyJson(request, 200, "{\"success\":true}");
                case ImportResult::BAD_SESSION:
                    return replyJson(request, 410, "{\"success\":false,\"reason\":\"Unknown session\"}");
                default:
                    return replyJson(request, 500, "{\"success\":false}");
                }
            },
            AuthenticationPredicates::IS_ADMIN));
    // FSPersistence::readFromFS uses updateWithoutPropagation, so update handlers
    // (incl. writeToFS) are never triggered on load. Force a write-back here so
    // any migration changes (e.g. v0→v1 enum remapping + version field) are
    // persisted immediately and don't re-run on the next restart.
    _fsPersistence.writeToFS();

    // Initialize cached alarm-publishing settings
    _updateAlarmPublishingSettingsCache();

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
                                           _publishSmokeDetectorAttributes(device.id);
                                   }
                               } },
                           false);

    /* Update cache when alarm-publishing settings change */
    if (_alarmPublishingSettings != nullptr)
    {
        _alarmPublishingSettings->addUpdateHandler([this](const String &originId)
                                                   {
                                                       this->_updateAlarmPublishingSettingsCache();
                                                       this->mqttPublishSimpleAlarmState(); },
                                                   false);
    }

    /* Register sub-devices for devices already loaded from FS */
    _syncSmokeDetectorSubDevices();

    // Heap snapshot after all HA sub-devices are constructed — useful for verifying
    // PSRAM-routing sdkconfig changes actually moved allocations off internal RAM.
    // Compare internal/PSRAM free before vs. after the import → boot → settle cycle.
    ESP_LOGI(GeniusDevices::TAG,
             "heap after begin(): devices=%u  internal_free=%u (largest=%u)  psram_free=%u (largest=%u)  total_free=%u",
             (unsigned)_state.devices.size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
             (unsigned)esp_get_free_heap_size());
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

                ESP_LOGI(GeniusDevices::TAG, "Alarm ended for smoke detector with SN '%lu'.", detectorSN);
            }
            break;
        }
    }

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
    bool changed = (_isAlarming != isAlarming || _numAlarming != numAlarming);
    _isAlarming = isAlarming;
    _numAlarming = numAlarming;
    endTransaction();

    if (changed)
        mqttPublishSimpleAlarmState();
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

// ============================================================================
// GeniusDevice JSON helpers (shared between bulk-POST and per-device endpoints)
// ============================================================================

std::vector<genius_device_alarm_t> GeniusDevice::parseAlarms(JsonArray jsonAlarms)
{
    std::vector<genius_device_alarm_t> result;
    int count = 0;
    for (JsonVariant jsonAlarm : jsonAlarms)
    {
        if (count++ >= GATEWAY_MAX_ALARMS)
        {
            ESP_LOGE(GeniusDevices::TAG, "Too many alarms for smoke detector device. Maximum allowed is %d.", GATEWAY_MAX_ALARMS);
            break;
        }
        result.push_back(genius_device_alarm_t{
            .startTime = Utils::iso8601_to_time_t(jsonAlarm["startTime"].as<String>()),
            .endTime = Utils::iso8601_to_time_t(jsonAlarm["endTime"].as<String>()),
            .endingReason = static_cast<genius_alarm_ending_t>(jsonAlarm["endingReason"].as<int>())});
    }
    return result;
}

bool GeniusDevice::buildFromJson(JsonVariant src, GeniusDevice &out)
{
    uint32_t deviceId = src["id"].as<uint32_t>();
    if (deviceId == 0)
        return false;

    JsonObject smokeDetectorJson = src["smokeDetector"].as<JsonObject>();
    JsonObject radioModuleJson = src["radioModule"].as<JsonObject>();

    out = GeniusDevice(
        GeniusSmokeDetectorInfo::fromJson(smokeDetectorJson),
        GeniusRadioModuleInfo::fromJson(radioModuleJson),
        src["location"].as<String>(),
        deviceId);

    out.isAlarming = src["isAlarming"].is<bool>() ? src["isAlarming"].as<bool>() : false;
    out.registration = src["registration"].is<int>()
                           ? static_cast<genius_device_registration_t>(src["registration"].as<int>())
                           : GDR_MANUAL;
    out.readoutTime = src["readoutTime"].is<String>()
                          ? Utils::iso8601_to_time_t(src["readoutTime"].as<String>())
                          : 0;
    out.readoutProtocolVersion = src["readoutProtocolVersion"].is<int>()
                                     ? src["readoutProtocolVersion"].as<uint8_t>()
                                     : 0;

    if (src["alarms"].is<JsonArray>())
        out.alarms = parseAlarms(src["alarms"].as<JsonArray>());

    out.published = false; // new device — needs publish
    return true;
}

bool GeniusDevice::mergeFromJson(JsonVariant src)
{
    bool changed = false;

    JsonObject smokeDetectorJson = src["smokeDetector"].as<JsonObject>();
    JsonObject radioModuleJson = src["radioModule"].as<JsonObject>();

    GeniusSmokeDetectorInfo newSD = GeniusSmokeDetectorInfo::fromJson(smokeDetectorJson);
    GeniusRadioModuleInfo newRM = GeniusRadioModuleInfo::fromJson(radioModuleJson);
    time_t newReadoutTime = src["readoutTime"].is<String>()
                                ? Utils::iso8601_to_time_t(src["readoutTime"].as<String>())
                                : 0;
    uint8_t newReadoutProtocolVersion = src["readoutProtocolVersion"].is<int>()
                                            ? src["readoutProtocolVersion"].as<uint8_t>()
                                            : 0;

    // Identity fields: smoke detector
    if (this->smokeDetector.model != newSD.model)
    {
        this->smokeDetector.model = newSD.model;
        changed = true;
    }
    if (this->smokeDetector.sn != newSD.sn)
    {
        this->smokeDetector.sn = newSD.sn;
        changed = true;
    }
    if (this->smokeDetector.productionDate != newSD.productionDate)
    {
        this->smokeDetector.productionDate = newSD.productionDate;
        changed = true;
    }

    // Identity fields: radio module
    if (this->radioModule.model != newRM.model)
    {
        this->radioModule.model = newRM.model;
        changed = true;
    }
    if (this->radioModule.sn != newRM.sn)
    {
        this->radioModule.sn = newRM.sn;
        changed = true;
    }

    // Readout-time bump: replace full SD/RM structs and bump registration
    if (this->readoutTime != newReadoutTime)
    {
        this->readoutTime = newReadoutTime;
        this->readoutProtocolVersion = newReadoutProtocolVersion;
        this->smokeDetector = newSD;
        this->radioModule = newRM;
        if (src["registration"].is<int>())
            this->registration = static_cast<genius_device_registration_t>(src["registration"].as<int>());
        changed = true;
    }

    String newLocation = src["location"].as<String>();
    if (this->location != newLocation)
    {
        this->location = newLocation;
        changed = true;
    }

    if (src["alarms"].is<JsonArray>())
    {
        auto newAlarms = parseAlarms(src["alarms"].as<JsonArray>());
        if (this->alarms != newAlarms)
        {
            this->alarms = std::move(newAlarms);
            changed = true;
        }
    }

    if (src["isAlarming"].is<bool>())
    {
        bool newIsAlarming = src["isAlarming"].as<bool>();
        if (this->isAlarming != newIsAlarming)
        {
            this->isAlarming = newIsAlarming;
            changed = true;
        }
    }

    if (changed)
        this->published = false;

    return changed;
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
    std::vector<GeniusDevice, PsramAllocator<GeniusDevice>> newDevicesVector; // Build new devices vector in JSON order
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
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': SD model %d->%d", updatedDevice.location.c_str(),
                         static_cast<int>(updatedDevice.smokeDetector.model), static_cast<int>(newSD.model));
                updatedDevice.smokeDetector.model = newSD.model;
                deviceChanged = true;
            }
            if (updatedDevice.smokeDetector.sn != newSD.sn)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': SD SN %lu->%lu", updatedDevice.location.c_str(),
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
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': RM model %d->%d", updatedDevice.location.c_str(),
                         static_cast<int>(updatedDevice.radioModule.model), static_cast<int>(newRM.model));
                updatedDevice.radioModule.model = newRM.model;
                deviceChanged = true;
            }
            if (updatedDevice.radioModule.sn != newRM.sn)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': RM SN %lu->%lu", updatedDevice.location.c_str(),
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
                // Upgrade registration when an acoustic readout arrives for a manually/packet-added device
                if (jsonDeviceArrItem["registration"].is<int>())
                    updatedDevice.registration = static_cast<genius_device_registration_t>(jsonDeviceArrItem["registration"].as<int>());
                deviceChanged = true;
            }

            // Update location
            String newLocation = jsonDeviceArrItem["location"].as<String>();
            if (updatedDevice.location != newLocation)
            {
                ESP_LOGD(GeniusDevices::TAG, "Device @ '%s': location->'%s'",
                         updatedDevice.location.c_str(), newLocation.c_str());
                updatedDevice.location = newLocation;
                deviceChanged = true;
            }

            // Update alarms
            if (jsonDeviceArrItem["alarms"].is<JsonArray>())
            {
                std::vector<genius_device_alarm_t> newAlarms;
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
                if (updatedDevice.alarms != newAlarms)
                {
                    updatedDevice.alarms = newAlarms;
                    deviceChanged = true;
                }
            }

            // Update isAlarming from frontend (frontend is responsible for correct state, e.g. clearing on import)
            if (jsonDeviceArrItem["isAlarming"].is<bool>())
            {
                bool newIsAlarming = jsonDeviceArrItem["isAlarming"].as<bool>();
                if (updatedDevice.isAlarming != newIsAlarming)
                {
                    updatedDevice.isAlarming = newIsAlarming;
                    deviceChanged = true;
                }
            }

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
    if (_haDevices.count(device.id))
        return;

    uint32_t devId = device.id;

    HADeviceIdentity identity;
    identity.id           = "genius-" + String(devId);
    identity.name         = smokeDetectorModelName(device.smokeDetector.model);
    identity.manufacturer = "Hekatron Vertriebs GmbH";
    identity.model        = smokeDetectorModelName(device.smokeDetector.model);
    identity.serialNumber = String(device.smokeDetector.sn);
    identity.suggestedArea = device.location;

    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
        identity.configurationUrl = "http://" + localIP.toString() + "/gateway/smoke-detectors";

    auto dev = std::make_unique<HADevice>(_haService, std::move(identity));

    // ---- Smoke alarm binary_sensor (Control) — reads live from _state.devices ----
    auto sensor = std::make_unique<HABinarySensor>(_haService, "smoke",
        [this, devId]() -> bool {
            bool alarming = false;
            beginTransaction();
            for (const auto &d : _state.devices)
                if (d.id == devId) { alarming = d.isAlarming; break; }
            endTransaction();
            return alarming;
        },
        [](JsonObject &c) {
            c["device_class"] = "smoke";
            IPAddress ip = WiFi.localIP();
            if (IPUtils::isSet(ip))
                c["entity_picture"] = "http://" + ip.toString() + "/hekatron-genius-plus-x.png";
        });
    sensor->setName("Smoke Detector");
    HABinarySensor *rawSensor = dev->registerControl(std::move(sensor));

    // ---- Diagnostics grouped publisher — all 13 readout entities share one state topic ----
    // Availability is folded into the JSON: {"available": bool, ...fields}
    // Entities show "unavailable" in HA until the first acoustic readout arrives.
    HADevice *rawDev = _haService->addSubDevice(std::move(dev));

    auto diag = std::make_unique<HAGroupedSensorPublisher>(
        _haService, "diagnostics",
        [this, devId](JsonObject &state) {
            beginTransaction();
            const GeniusDevice *d = nullptr;
            for (const auto &dev : _state.devices)
                if (dev.id == devId) { d = &dev; break; }

            if (!d || d->readoutTime == 0) {
                state["available"] = false;
                endTransaction();
                return;
            }

            state["available"]           = true;
            state["battery_low"]         = d->smokeDetector.batteryLowFault;
            state["device_fault"]        = d->smokeDetector.deviceFault;
            state["radio_fault"]         = d->radioModule.radioNetworkFault;
            state["deinstall_count"]     = d->smokeDetector.deinstallationCount;
            state["last_readout"]        = Utils::time_t_to_iso8601(d->readoutTime);
            const char *rmName = radioModuleModelName(d->radioModule.model);
            if (rmName) state["radio_module_model"] = rmName;
            if (d->radioModule.lineId) state["alarm_line_id"] = d->radioModule.lineId;
            if (d->radioModule.lineCharacter)
                state["alarm_line"] = String(d->radioModule.lineCharacter) + "." + String(d->radioModule.lineNumber);
            if (d->smokeDetector.productionDate > 0) {
                char buf[9];
                struct tm *pt = gmtime(&d->smokeDetector.productionDate);
                strftime(buf, sizeof(buf), "%d.%m.%y", pt);
                state["production_date"] = buf;
            }
            state["radio_module_serial"] = d->radioModule.sn;
            state["alarm_count_total"]   = d->smokeDetector.alarmCountTotal;
            state["alarm_count_3m"]      = d->smokeDetector.alarmCountLast3Months;
            state["radio_interference"]  = d->radioModule.radioInterference;

            endTransaction();
        },
        rawDev);

    // Availability template shared by all diagnostic entities.
    // Evaluated from the same state topic message — no separate avail topic needed.
    // Captured by value below: the std::function copies stored in Sensor::extraConfig
    // outlive this function, so a [&availConfig] capture would dangle once we return.
    auto availConfig = [](JsonObject &c) {
        c["availability_topic"]   = "~/diagnostics/state";
        c["availability_template"] = "{{ 'online' if value_json.available else 'offline' }}";
    };

    diag->addSensor("binary_sensor", "battery_low", "Battery", "", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ 'ON' if value_json.battery_low else 'OFF' }}";
            c["device_class"]   = "battery";
            availConfig(c);
        })
    .addSensor("binary_sensor", "device_fault", "Smoke Detector State", "", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ 'ON' if value_json.device_fault else 'OFF' }}";
            c["device_class"]   = "problem";
            availConfig(c);
        })
    .addSensor("binary_sensor", "radio_fault", "Radio Module State", "", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ 'ON' if value_json.radio_fault else 'OFF' }}";
            c["device_class"]   = "problem";
            availConfig(c);
        })
    .addSensor("sensor", "deinstallation_count", "Deinstallation Count", "mdi:autorenew", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ value_json.deinstall_count }}";
            c["state_class"]    = "total_increasing";
            availConfig(c);
        })
    .addSensor("sensor", "last_readout", "Last Service", "mdi:clock-check-outline", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ value_json.last_readout }}";
            c["device_class"]   = "timestamp";
            availConfig(c);
        })
    .addSensor("sensor", "radio_module_model", "Radio Module Model", "mdi:radio-tower", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ value_json.radio_module_model | default('') }}";
            availConfig(c);
        })
    .addSensor("sensor", "alarm_line_id", "Alarm Line ID", "mdi:identifier", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ value_json.alarm_line_id | default('None') }}";
            availConfig(c);
        })
    .addSensor("sensor", "alarm_line", "Alarm Line", "mdi:alarm-light-outline", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ value_json.alarm_line | default('None') }}";
            availConfig(c);
        })
    .addSensor("sensor", "production_date", "Production Date", "mdi:calendar-badge", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ value_json.production_date | default('Unknown') }}";
            availConfig(c);
        })
    .addSensor("sensor", "radio_module_serial", "Radio Module Serial", "mdi:barcode", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ value_json.radio_module_serial }}";
            availConfig(c);
        })
    .addSensor("sensor", "alarm_count_total", "Alarms (Total)", "mdi:alarm-light", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ value_json.alarm_count_total }}";
            c["state_class"]    = "total_increasing";
            availConfig(c);
        })
    .addSensor("sensor", "alarm_count_3m", "Alarms (3 Months)", "mdi:calendar-clock", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"] = "{{ value_json.alarm_count_3m }}";
            c["state_class"]    = "measurement";
            availConfig(c);
        })
    .addSensor("sensor", "radio_interference", "Radio Interference", "mdi:wifi-alert", HACategory::Diagnostic,
        [availConfig](JsonObject &c) {
            c["value_template"]      = "{{ value_json.radio_interference }}";
            c["unit_of_measurement"] = "%";
            c["state_class"]         = "measurement";
            availConfig(c);
        });

    diag->begin();

    _haDevices[device.id] = {rawDev, rawSensor, std::move(diag)};

    // For devices added at runtime (e.g. via REST POST while MQTT is live):
    // publishAll() sends both the discovery config for all 13 entities AND the
    // initial state. begin() only registers a future onPublishAll callback and
    // does not publish immediately, so calling it alone would silently skip the
    // diagnostic configs for dynamically added devices.
    if (_haService->isReady())
    {
        auto &entry = _haDevices[device.id];
        if (entry.diagnostics)
            entry.diagnostics->publishAll();
    }
}

void GeniusDevicesService::_removeSmokeDetectorSubDevice(uint32_t deviceId)
{
    auto it = _haDevices.find(deviceId);
    if (it != _haDevices.end())
    {
        if (it->second.diagnostics && _haService->isReady())
            it->second.diagnostics->unpublishAll();
        _haDevices.erase(it);
    }
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
            vTaskDelay(pdMS_TO_TICKS(1));
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
                if (_haService->isReady())
                {
                    it->second.device->publishAll();
                    if (it->second.diagnostics)
                        it->second.diagnostics->publishAll();
                }
            }
            device.published = true;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    endTransaction();
}

esp_err_t GeniusDevicesService::_publishSmokeDetectorAttributes(uint32_t deviceId)
{
    auto it = _haDevices.find(deviceId);
    if (it == _haDevices.end() || !it->second.diagnostics)
        return ESP_ERR_NOT_FOUND;
    if (!_haService->isReady())
        return ESP_ERR_INVALID_STATE;

    it->second.diagnostics->publishState();
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

    if (!_cachedAlarmPublishingSettings.alarmEnabled)
        return;

    if (_cachedAlarmPublishingSettings.alarmTopic.isEmpty())
    {
        ESP_LOGE(GeniusDevices::TAG, "Alarm MQTT topic is empty. Cannot publish alarm state.");
        return;
    }

    JsonDocument doc;
    doc["isAlarming"] = _isAlarming;
    doc["numAlarmingDevices"] = _numAlarming;

    String payload;
    serializeJson(doc, payload);

    if (_mqttClient->publish(_cachedAlarmPublishingSettings.alarmTopic.c_str(), 0, true, payload.c_str()) == -1)
    {
        ESP_LOGE(GeniusDevices::TAG, "Failed to publish simple alarm state.");
    }
}

// ============================================================================
// Private Methods - Settings Management
// ============================================================================

/**
 * @brief Updates the local cache of alarm-publishing settings
 *
 * Loads current alarm-publishing settings from AlarmPublishingSettingsService
 * and stores them in cache for faster access without repeated service calls.
 */
void GeniusDevicesService::_updateAlarmPublishingSettingsCache()
{
    if (_alarmPublishingSettings != nullptr)
    {
        _cachedAlarmPublishingSettings = _alarmPublishingSettings->getSettingsCopy();
        ESP_LOGV(GeniusDevices::TAG, "Updated cached alarm-publishing settings (alarmEnabled: %d, topic: %s)",
                 _cachedAlarmPublishingSettings.alarmEnabled,
                 _cachedAlarmPublishingSettings.alarmTopic.c_str());
    }
}

// ============================================================================
// Public Methods - Per-Device CRUD
// ============================================================================

GeniusDevicesService::DeviceOpResult
GeniusDevicesService::upsertDevice(JsonVariant deviceJson, GeniusDevice &out)
{
    uint32_t deviceId = deviceJson["id"].as<uint32_t>();
    if (deviceId == 0)
        return DeviceOpResult::INVALID_BODY;

    DeviceOpResult result = DeviceOpResult::OK;
    bool stateChanged = false;

    beginTransaction();
    auto it = std::find_if(_state.devices.begin(), _state.devices.end(),
                           [deviceId](const GeniusDevice &d) { return d.id == deviceId; });

    if (it != _state.devices.end())
    {
        // Update in place. `published=false` is set inside mergeFromJson when something changed.
        bool changed = it->mergeFromJson(deviceJson);
        out = *it;
        result = changed ? DeviceOpResult::OK : DeviceOpResult::UNCHANGED;
        stateChanged = changed;
    }
    else
    {
        if (_state.devices.size() >= GATEWAY_MAX_DEVICES)
        {
            endTransaction();
            return DeviceOpResult::LIMIT_REACHED;
        }
        GeniusDevice newDevice(GeniusSmokeDetectorInfo{}, GeniusRadioModuleInfo{}, String(), 0);
        if (!GeniusDevice::buildFromJson(deviceJson, newDevice))
        {
            endTransaction();
            return DeviceOpResult::INVALID_BODY;
        }
        _state.devices.push_back(std::move(newDevice));
        out = _state.devices.back();
        stateChanged = true;
    }
    endTransaction();

    if (stateChanged)
        callUpdateHandlers(HTTP_ENDPOINT_ORIGIN_ID);

    return result;
}

GeniusDevicesService::DeviceOpResult
GeniusDevicesService::removeDevice(uint32_t deviceId)
{
    if (deviceId == 0)
        return DeviceOpResult::INVALID_BODY;

    bool removed = false;
    beginTransaction();
    auto it = std::find_if(_state.devices.begin(), _state.devices.end(),
                           [deviceId](const GeniusDevice &d) { return d.id == deviceId; });
    if (it != _state.devices.end())
    {
        ESP_LOGI(GeniusDevices::TAG, "Device with SN %lu marked for deletion.", it->smokeDetector.sn);
        _state.deletedDeviceIds.push_back(it->id);
        _state.devices.erase(it);
        removed = true;
    }
    endTransaction();

    if (!removed)
        return DeviceOpResult::NOT_FOUND;

    callUpdateHandlers(HTTP_ENDPOINT_ORIGIN_ID);
    return DeviceOpResult::OK;
}

GeniusDevicesService::DeviceOpResult
GeniusDevicesService::reorderDevices(const std::vector<uint32_t> &newOrder)
{
    bool orderChanged = false;
    bool valid = true;

    beginTransaction();

    // The new order must contain exactly the current id set — same size, same elements.
    if (newOrder.size() != _state.devices.size())
    {
        valid = false;
    }
    else
    {
        for (uint32_t id : newOrder)
        {
            auto found = std::find_if(_state.devices.begin(), _state.devices.end(),
                                      [id](const GeniusDevice &d) { return d.id == id; });
            if (found == _state.devices.end())
            {
                valid = false;
                break;
            }
        }
    }

    if (valid)
    {
        std::vector<GeniusDevice, PsramAllocator<GeniusDevice>> reordered;
        reordered.reserve(newOrder.size());
        for (uint32_t id : newOrder)
        {
            auto found = std::find_if(_state.devices.begin(), _state.devices.end(),
                                      [id](const GeniusDevice &d) { return d.id == id; });
            // Detect actual order change before moving
            if (!orderChanged && (size_t)(found - _state.devices.begin()) != reordered.size())
                orderChanged = true;
            reordered.push_back(std::move(*found));
        }
        _state.devices = std::move(reordered);
    }
    endTransaction();

    if (!valid)
        return DeviceOpResult::INVALID_BODY;

    if (orderChanged)
        callUpdateHandlers(HTTP_ENDPOINT_ORIGIN_ID);

    return orderChanged ? DeviceOpResult::OK : DeviceOpResult::UNCHANGED;
}

// ============================================================================
// Public Methods - Chunked Import
// ============================================================================

String GeniusDevicesService::_newImportToken()
{
    // 16 hex chars = 64 bits — plenty of entropy for a single-slot session id.
    // esp_random() is hardware-backed when WiFi/BT is enabled (always true here).
    char buf[17];
    snprintf(buf, sizeof(buf), "%08lx%08lx",
             (unsigned long)esp_random(), (unsigned long)esp_random());
    return String(buf);
}

bool GeniusDevicesService::_expireImportSessionIfStale()
{
    if (!_importSession.active)
        return false;
    if ((time(nullptr) - _importSession.lastActivity) < GATEWAY_DEVICES_IMPORT_SESSION_TTL_S)
        return false;

    ESP_LOGW(GeniusDevices::TAG, "Import session %s expired (idle > %ds), discarding %u staged devices.",
             _importSession.token.c_str(),
             GATEWAY_DEVICES_IMPORT_SESSION_TTL_S,
             (unsigned)_importSession.staging.size());
    _importSession.active = false;
    _importSession.token = String();
    _importSession.staging.clear();
    _importSession.staging.shrink_to_fit();
    return true;
}

GeniusDevicesService::ImportResult
GeniusDevicesService::importBegin(String &tokenOut)
{
    xSemaphoreTakeRecursive(_importMutex, portMAX_DELAY);
    _expireImportSessionIfStale();
    if (_importSession.active)
    {
        xSemaphoreGiveRecursive(_importMutex);
        return ImportResult::BUSY;
    }

    _importSession.active = true;
    _importSession.token = _newImportToken();
    _importSession.lastActivity = time(nullptr);
    _importSession.staging.clear();
    _importSession.staging.reserve(GATEWAY_MAX_DEVICES);
    tokenOut = _importSession.token;
    ESP_LOGI(GeniusDevices::TAG, "Import session %s started.", _importSession.token.c_str());

    xSemaphoreGiveRecursive(_importMutex);
    return ImportResult::OK;
}

GeniusDevicesService::ImportResult
GeniusDevicesService::importChunk(const String &token, JsonArray devices)
{
    // Entry log promoted to INFO so the serial monitor confirms the handler runs.
    // Useful when troubleshooting reports where the chunk request appears to "hang":
    // absence of this line means PsychicHttp blocked before invoking the callback
    // (body recv, JSON parse, etc.), not anything in our code.
    ESP_LOGI(GeniusDevices::TAG, "importChunk: token=%s incoming=%u devices, staged=%u",
             token.c_str(), (unsigned)devices.size(), (unsigned)_importSession.staging.size());

    xSemaphoreTakeRecursive(_importMutex, portMAX_DELAY);
    _expireImportSessionIfStale();
    if (!_importSession.active || _importSession.token != token || token.length() == 0)
    {
        xSemaphoreGiveRecursive(_importMutex);
        return ImportResult::BAD_SESSION;
    }

    for (JsonVariant deviceJson : devices)
    {
        if (_importSession.staging.size() >= GATEWAY_MAX_DEVICES)
        {
            ESP_LOGE(GeniusDevices::TAG, "Import chunk rejected: would exceed GATEWAY_MAX_DEVICES (%d).", GATEWAY_MAX_DEVICES);
            xSemaphoreGiveRecursive(_importMutex);
            return ImportResult::LIMIT_REACHED;
        }
        GeniusDevice newDevice(GeniusSmokeDetectorInfo{}, GeniusRadioModuleInfo{}, String(), 0);
        if (!GeniusDevice::buildFromJson(deviceJson, newDevice))
        {
            ESP_LOGE(GeniusDevices::TAG, "Import chunk rejected: malformed device entry.");
            xSemaphoreGiveRecursive(_importMutex);
            return ImportResult::BAD_PAYLOAD;
        }
        _importSession.staging.push_back(std::move(newDevice));
    }
    _importSession.lastActivity = time(nullptr);
    ESP_LOGI(GeniusDevices::TAG, "importChunk: %u staged after this chunk.",
             (unsigned)_importSession.staging.size());

    xSemaphoreGiveRecursive(_importMutex);
    return ImportResult::OK;
}

GeniusDevicesService::ImportResult
GeniusDevicesService::importCommit(const String &token)
{
    xSemaphoreTakeRecursive(_importMutex, portMAX_DELAY);
    _expireImportSessionIfStale();
    if (!_importSession.active || _importSession.token != token || token.length() == 0)
    {
        xSemaphoreGiveRecursive(_importMutex);
        return ImportResult::BAD_SESSION;
    }

    // Atomic swap: replace live device list with staging. Track deletions so HA cleans up.
    beginTransaction();
    _state.deletedDeviceIds.clear();
    for (const auto &oldDevice : _state.devices)
    {
        // A staged device with the same id counts as an update, not a deletion.
        bool stagedHasSameId = std::any_of(_importSession.staging.begin(), _importSession.staging.end(),
                                           [&oldDevice](const GeniusDevice &s) { return s.id == oldDevice.id; });
        if (!stagedHasSameId)
            _state.deletedDeviceIds.push_back(oldDevice.id);
    }

    _state.devices.clear();
    _state.devices.reserve(_importSession.staging.size());
    for (auto &dev : _importSession.staging)
        _state.devices.push_back(std::move(dev));
    endTransaction();

    ESP_LOGI(GeniusDevices::TAG, "Import session %s committed: %u devices, %u deletions.",
             _importSession.token.c_str(),
             (unsigned)_state.devices.size(),
             (unsigned)_state.deletedDeviceIds.size());

    _importSession.active = false;
    _importSession.token = String();
    _importSession.staging.clear();
    _importSession.staging.shrink_to_fit();
    xSemaphoreGiveRecursive(_importMutex);

    // Defer the slow part (FS persistence + HA discovery republish) to a background
    // task so this handler returns immediately. For a fresh 30-device import the
    // HA publish chain takes ~18 s and would otherwise block the HTTP task, time out
    // the commit response, and starve the WS heartbeat.
    if (_postCommitTaskHandle)
        xTaskNotifyGive(_postCommitTaskHandle);
    else
        callUpdateHandlers(HTTP_ENDPOINT_ORIGIN_ID); // fallback if task wasn't created
    return ImportResult::OK;
}

void GeniusDevicesService::_postCommitTask()
{
    ESP_LOGI(GeniusDevices::TAG, "post-commit task started.");
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(GeniusDevices::TAG, "post-commit: running deferred FS write + HA sync.");
        callUpdateHandlers(HTTP_ENDPOINT_ORIGIN_ID);
        ESP_LOGI(GeniusDevices::TAG, "post-commit: deferred work complete.");
    }
}

GeniusDevicesService::ImportResult
GeniusDevicesService::importAbort(const String &token)
{
    xSemaphoreTakeRecursive(_importMutex, portMAX_DELAY);
    if (!_importSession.active || _importSession.token != token || token.length() == 0)
    {
        xSemaphoreGiveRecursive(_importMutex);
        return ImportResult::BAD_SESSION;
    }
    ESP_LOGI(GeniusDevices::TAG, "Import session %s aborted (%u devices staged).",
             _importSession.token.c_str(), (unsigned)_importSession.staging.size());
    _importSession.active = false;
    _importSession.token = String();
    _importSession.staging.clear();
    _importSession.staging.shrink_to_fit();
    xSemaphoreGiveRecursive(_importMutex);
    return ImportResult::OK;
}
