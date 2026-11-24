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
#include <HomeAssistant/HAUtils.h>
#include <WiFi.h>
#include <esp_random.h>
#include <Utils.hpp>

GeniusDevicesService::GeniusDevicesService(ESP32SvelteKit *sveltekit, PsychicMqttClient *mqttClient, AlarmPublishingSettingsService *alarmPublishingSettings) : _httpEndpoint(GeniusDevices::read,
                                                                                                                                                                      GeniusDevices::update,
                                                                                                                                                                      this,
                                                                                                                                                                      sveltekit->getServer(),
                                                                                                                                                                      GATEWAY_DEVICES_SERVICE_PATH,
                                                                                                                                                                      sveltekit->getSecurityManager(),
                                                                                                                                                                      AuthenticationPredicates::IS_ADMIN,
                                                                                                                                                                      // The Overview shows the detectors to every signed-in user; only reading is opened up
                                                                                                                                                                      AuthenticationPredicates::IS_AUTHENTICATED),
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
                                                                                                                                                        _haService(sveltekit->getHAService()),
                                                                                                                                                        _haSettingsService(sveltekit->getHASettingsService()),
                                                                                                                                                        _eventSocket(sveltekit->getSocket())
{
    _importMutex = xSemaphoreCreateRecursiveMutex();
    _runtimeMutex = xSemaphoreCreateMutex();
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
    // Register the event before the slow device load below: a WebSocket client that
    // reconnects early can then subscribe without an "unregistered event" warning.
    _eventSocket->registerEvent(HA_ENTITY_RENAME_EVENT);
    _eventSocket->registerEvent(DEVICE_IMPORT_EVENT);
    _eventSocket->registerEvent(DEVICE_RUNTIME_EVENT);

    _httpEndpoint.begin();
    _fsPersistence.readFromFS();

    // Spawn the post-commit task - it sleeps on a notification and runs the
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

    // Spawn the HA entity-ID rename task - it sleeps on a notification and runs the
    // remove / wait / re-publish cycle outside the HTTP task, the RX path and the
    // state mutex (see _runEntityIdRename()).
    xTaskCreatePinnedToCore(
        _haRenameTaskImpl,
        "gd-ha-rename",
        6144,
        this,
        4,
        &_haRenameTaskHandle,
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

    // ---- Per-device: POST /rest/gateway-devices/device/reset-status  body: { id } ----
    _server->on(
        GATEWAY_DEVICES_RESET_STATUS_PATH,
        HTTP_POST,
        _securityManager->wrapCallback(
            [this](PsychicRequest *request, JsonVariant &json) -> esp_err_t {
                if (!json.is<JsonObject>() || !json["id"].is<uint32_t>())
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Missing id\"}");
                GeniusDevice updated(GeniusSmokeDetectorInfo{}, GeniusRadioModuleInfo{}, String(), 0);
                switch (this->resetStatus(json["id"].as<uint32_t>(), updated))
                {
                case DeviceOpResult::OK:
                case DeviceOpResult::UNCHANGED:
                    return replyDevice(request, updated);
                case DeviceOpResult::NOT_FOUND:
                    return replyJson(request, 404, "{\"success\":false,\"reason\":\"Device not found\"}");
                case DeviceOpResult::INVALID_BODY:
                    return replyJson(request, 400, "{\"success\":false,\"reason\":\"Invalid id\"}");
                default:
                    return replyJson(request, 500, "{\"success\":false}");
                }
            },
            AuthenticationPredicates::IS_ADMIN));

    // ---- GET /rest/gateway-devices/runtime: last seen / signal of the supervised modules ----
    _server->on(
        GATEWAY_DEVICES_RUNTIME_PATH,
        HTTP_GET,
        _securityManager->wrapRequest(
            [this](PsychicRequest *request) -> esp_err_t {
                PsychicJsonResponse response(request, false);
                JsonObject root = response.getRoot().to<JsonObject>();
                this->runtimeToJson(root, false);
                return response.send();
            },
            AuthenticationPredicates::IS_AUTHENTICATED));

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

    // Status counters for the devices just loaded (readFromFS does not run the update handlers)
    _updateStatusSummary();
    _syncRuntimeTable();

    /* Update alarming state and status summary after every device update */
    this->addUpdateHandler([&](const String &originId)
                           {
                               _updateAlarmingState();
                               _updateStatusSummary();
                               _syncRuntimeTable(); },
                           false);

    /* Sync HA sub-devices after device list changes (not for packet-originated adds, alarm or
     * status changes, or writes that only persist backend-owned values) */
    this->addUpdateHandler([&](const String &originId)
                           {
                               if (originId != GENIUS_DEVICE_ADDED_FROM_PACKET && originId != ALARM_STATE_CHANGE &&
                                   originId != DEVICE_STATUS_CHANGE && originId != RSSI_SURVEY_UPDATE &&
                                   originId != HA_ENTITY_ID_PREFIX_UPDATE)
                               {
                                   // Import: the persistence handler was registered first (in the
                                   // constructor) and has written the list to flash by now.
                                   if (_isImportSync())
                                       _emitImportEvent("saved");

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
                                                       this->mqttPublishSimpleAlarmState();
                                                       this->mqttPublishSimpleFaultState(); },
                                                   false);
    }

    /* Re-evaluate all detectors when the HA settings change: the "rename entity IDs" option
     * decides their display name and whether their entity IDs are managed. This handler is
     * registered after the one HASettingsService adds in its constructor, so HAService already
     * carries the new values when it runs. The work is handed to the rename task - the settings
     * are applied from the HTTP task, which must not publish dozens of discovery configs. */
    if (_haSettingsService != nullptr)
    {
        _haSettingsService->addUpdateHandler([this](const String &originId)
                                             {
                                                 _haSyncRequested = true;
                                                 _requestEntityIdRename(); },
                                             false);
    }

    /* Register sub-devices for devices already loaded from FS. MQTT is not up yet, so a pending
     * entity-ID rename is not requested here - the first connect runs the comparison again. */
    _syncSmokeDetectorSubDevices(false);

    // Heap snapshot after all HA sub-devices are constructed - useful for verifying
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
    newDevice.logEvent(GDE_CREATED, GDR_GENIUS_PACKET);
    // A detector first seen in a packet has no entities in Home Assistant yet: its first
    // publish already carries the right entity IDs, so there is nothing to rename later.
    newDevice.haEntityIdPrefix = _entityIdPrefixFor(newDevice);
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
                device.status.startAlarm(time(nullptr)); // seconds precision
                device.alarmCount++;

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

                if (!device.status.endAlarm(time(nullptr), static_cast<uint32_t>(endingReason)))
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

            if (!device.status.endAlarm(time(nullptr), static_cast<uint32_t>(GAE_BY_MANUAL)))
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

const GeniusDevice *GeniusDevicesService::applyStatusChange(uint32_t detectorSN, uint32_t setMask, uint32_t clearedMask)
{
    setMask &= GENIUS_STATUS_TRACKED_MASK;
    clearedMask &= GENIUS_STATUS_TRACKED_MASK;

    GeniusDevice *updatedDevice = nullptr;
    uint32_t deviceId = 0;

    beginTransaction();

    for (GeniusDevice &device : _state.devices)
    {
        if (device.smokeDetector.sn != detectorSN)
            continue;

        const time_t now = time(nullptr);
        const GeniusStatusDelta delta = device.status.apply(setMask, clearedMask, now, GDE_STATUS_PACKET);
        if (delta.any())
        {
            // The log names what changed here, not what the packet carried - the many repeated
            // and mesh-forwarded copies of one change add nothing.
            device.status.addEvent({now, GDE_STATUS_PACKET, delta.set, delta.cleared | delta.removed, 0});
            updatedDevice = &device;
            deviceId = device.id;

            ESP_LOGI(GeniusDevices::TAG, "Status of smoke detector with SN '%lu' changed (set 0x%04lX, cleared 0x%04lX) - active now 0x%04lX.",
                     detectorSN, (unsigned long)delta.set, (unsigned long)(delta.cleared | delta.removed),
                     (unsigned long)device.status.activeMask());
        }
        break;
    }

    endTransaction();

    if (updatedDevice)
    {
        callUpdateHandlers(DEVICE_STATUS_CHANGE); // persist, update summary, notify front end

        // HA: battery / fault / dismount entities
        beginTransaction();
        _publishSmokeDetectorAttributes(deviceId);
        endTransaction();
    }

    return updatedDevice;
}

void GeniusDevicesService::applyModuleMissing(uint32_t reporterModuleSN, uint32_t missingModuleSN)
{
    if (missingModuleSN == 0 || reporterModuleSN == missingModuleSN)
        return;

    bool changed = false;
    uint32_t missingDeviceId = 0;
    const time_t now = time(nullptr);

    beginTransaction();

    for (GeniusDevice &device : _state.devices)
    {
        if (device.radioModule.sn == reporterModuleSN)
        {
            // The report is repeated and forwarded through the mesh: one log entry per report,
            // not one per copy that arrives.
            if (!device.status.hasRecentEvent(GDE_MODULE_MISSING, missingModuleSN, now, GENIUS_MODULE_MISSING_REPEAT_S))
            {
                device.status.addEvent({now, GDE_MODULE_MISSING, 0, 0, missingModuleSN});
                changed = true;
            }
        }
        else if (device.radioModule.sn == missingModuleSN)
        {
            // Edge-triggered like every status flag: the state stays until a frame of this
            // module is heard again (moduleHeard()) or the status is reset by hand.
            const GeniusStatusDelta delta = device.status.apply(GENIUS_STATUS_REPORTED_MISSING, 0, now, GDE_MODULE_MISSING);
            if (delta.any())
            {
                device.status.addEvent({now, GDE_MODULE_MISSING, GENIUS_STATUS_REPORTED_MISSING, 0, reporterModuleSN});
                missingDeviceId = device.id;
                changed = true;
            }
        }
    }

    endTransaction();

    ESP_LOGI(GeniusDevices::TAG, "Radio module %lu reports radio module %lu as missing%s.",
             (unsigned long)reporterModuleSN, (unsigned long)missingModuleSN,
             missingDeviceId ? "" : (changed ? " (not a registered detector or already known)" : " (nothing new)"));

    if (!changed)
        return;

    callUpdateHandlers(DEVICE_STATUS_CHANGE); // persist, update summary, notify front end

    if (missingDeviceId)
    {
        beginTransaction();
        _publishSmokeDetectorAttributes(missingDeviceId); // HA: radio module state
        endTransaction();
    }
}

bool GeniusDevicesService::moduleHeard(uint32_t originModuleSN, uint32_t senderModuleSN, int8_t rssiDbm)
{
    bool runtimeChanged = false;

    // ---- Last seen / signal of the supervised modules: RAM only, own mutex ----
    // Without a valid clock there is no time to keep; the values would be useless.
    const time_t now = time(nullptr);
    if (_runtimeAny.load() && geniusTimeValid(now))
    {
        xSemaphoreTake(_runtimeMutex, portMAX_DELAY);
        for (ModuleRuntime &entry : _runtime)
        {
            const bool transmitted = entry.moduleSn == senderModuleSN;
            if (!transmitted && entry.moduleSn != originModuleSN)
                continue;

            entry.lastSeen = now;
            if (transmitted)
            {
                // The signal belongs to the module whose radio was actually heard: the
                // sender, not the originator of a relayed frame.
                entry.rssi = rssiDbm;
                entry.rssiTime = now;
            }
            entry.changed = true;
            runtimeChanged = true;
        }
        xSemaphoreGive(_runtimeMutex);
    }

    _endReportedMissing(originModuleSN, senderModuleSN);
    return runtimeChanged;
}

void GeniusDevicesService::runtimeToJson(JsonObject root, bool changedOnly)
{
    JsonArray devices = root["devices"].to<JsonArray>();

    xSemaphoreTake(_runtimeMutex, portMAX_DELAY);
    for (ModuleRuntime &entry : _runtime)
    {
        if (changedOnly ? !entry.changed : entry.lastSeen == 0)
            continue;
        if (changedOnly)
            entry.changed = false;

        JsonObject obj = devices.add<JsonObject>();
        obj["id"] = entry.deviceId;
        obj["lastSeen"] = Utils::time_t_to_iso8601(entry.lastSeen);
        if (entry.rssiTime != 0)
        {
            obj["rssi"] = entry.rssi;
            obj["rssiTime"] = Utils::time_t_to_iso8601(entry.rssiTime);
        }
    }
    xSemaphoreGive(_runtimeMutex);
}

void GeniusDevicesService::emitRuntimeChanges()
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    runtimeToJson(root, true);
    if (root["devices"].size() == 0)
        return;

    _eventSocket->emitEvent(DEVICE_RUNTIME_EVENT, root);

    // Home Assistant: last seen / signal strength of the detectors that changed
    if (!_haService->isReady())
        return;
    beginTransaction();
    for (JsonObject changed : root["devices"].as<JsonArray>())
    {
        auto it = _haDevices.find(changed["id"].as<uint32_t>());
        if (it != _haDevices.end() && it->second.link)
            it->second.link->publishState();
    }
    endTransaction();
}

bool GeniusDevicesService::_runtimeOf(uint32_t deviceId, ModuleRuntime &out)
{
    bool found = false;
    xSemaphoreTake(_runtimeMutex, portMAX_DELAY);
    for (const ModuleRuntime &entry : _runtime)
    {
        if (entry.deviceId == deviceId)
        {
            out = entry;
            found = true;
            break;
        }
    }
    xSemaphoreGive(_runtimeMutex);
    return found;
}

void GeniusDevicesService::_syncRuntimeTable()
{
    // Which detectors have a radio module that takes part in the radio link supervision.
    // The criterion is the stored module model - from a readout or entered by hand.
    std::vector<ModuleRuntime> wanted;

    beginTransaction();
    for (const GeniusDevice &device : _state.devices)
    {
        if (device.radioModule.model != GRM_FM_PRO_X || device.radioModule.sn == 0)
            continue;
        ModuleRuntime entry;
        entry.deviceId = device.id;
        entry.moduleSn = device.radioModule.sn;
        wanted.push_back(entry);
    }
    endTransaction();

    // Keep what is known about the modules that stay. The runtime mutex is not taken while
    // the state mutex is needed afterwards - only the other order occurs.
    xSemaphoreTake(_runtimeMutex, portMAX_DELAY);
    for (ModuleRuntime &entry : wanted)
    {
        for (const ModuleRuntime &known : _runtime)
        {
            if (known.deviceId == entry.deviceId && known.moduleSn == entry.moduleSn)
            {
                entry = known;
                break;
            }
        }
    }
    _runtime = std::move(wanted);
    _runtimeAny = !_runtime.empty();
    xSemaphoreGive(_runtimeMutex);
}

void GeniusDevicesService::_endReportedMissing(uint32_t originModuleSN, uint32_t senderModuleSN)
{
    // Called from the RX path for every frame: stay out of the mutex unless a detector is
    // actually reported missing.
    if (_numReportedMissing.load() == 0)
        return;

    std::vector<uint32_t> heardDeviceIds;
    const time_t now = time(nullptr);

    beginTransaction();
    for (GeniusDevice &device : _state.devices)
    {
        const uint32_t sn = device.radioModule.sn;
        if (sn == 0 || (sn != originModuleSN && sn != senderModuleSN))
            continue;
        if (!(device.status.activeMask() & GENIUS_STATUS_REPORTED_MISSING))
            continue;

        // A frame this module originated or transmitted proves that it is there.
        const GeniusStatusDelta delta = device.status.apply(0, GENIUS_STATUS_REPORTED_MISSING, now, GDE_MODULE_MISSING);
        if (delta.any())
        {
            device.status.addEvent({now, GDE_MODULE_MISSING, 0, GENIUS_STATUS_REPORTED_MISSING, 0});
            heardDeviceIds.push_back(device.id);
            ESP_LOGI(GeniusDevices::TAG, "Radio module %lu, reported missing before, was heard again.", (unsigned long)sn);
        }
    }
    endTransaction();

    if (heardDeviceIds.empty())
        return;

    callUpdateHandlers(DEVICE_STATUS_CHANGE); // persist, update summary, notify front end

    beginTransaction();
    for (uint32_t deviceId : heardDeviceIds)
        _publishSmokeDetectorAttributes(deviceId);
    endTransaction();
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

void GeniusDevicesService::_updateStatusSummary()
{
    uint32_t numFaulty = 0;
    uint32_t numBatteryLow = 0;
    uint32_t numReportedMissing = 0;

    beginTransaction();
    for (const GeniusDevice &device : _state.devices)
    {
        // Only active flags count - an unknown flag is not a fault.
        const uint32_t active = device.status.activeMask();
        if (active & GENIUS_STATUS_FAULT_MASK)
            numFaulty++;
        if (active & GENIUS_STATUS_BATT_LOW)
            numBatteryLow++;
        if (active & GENIUS_STATUS_REPORTED_MISSING)
            numReportedMissing++;
    }
    _numFaulty = numFaulty;
    _numBatteryLow = numBatteryLow;
    _numReportedMissing = numReportedMissing; // read by the RX path, see moduleHeard()
    endTransaction();

    // The payload names devices and their locations, so it can change without the counters
    // changing - compare the whole payload. Only worth building while publishing is enabled.
    if (!_cachedAlarmPublishingSettings.faultEnabled)
        return;

    beginTransaction();
    const bool payloadChanged = _buildSimpleFaultPayload() != _lastFaultPayload;
    endTransaction();

    if (payloadChanged)
        mqttPublishSimpleFaultState();
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

uint32_t GeniusDevicesService::numFaultyDevices()
{
    beginTransaction();
    uint32_t numFaulty = _numFaulty;
    endTransaction();
    return numFaulty;
}

uint32_t GeniusDevicesService::numBatteryLowDevices()
{
    beginTransaction();
    uint32_t numBatteryLow = _numBatteryLow;
    endTransaction();
    return numBatteryLow;
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

bool GeniusDevicesService::isRadioModuleKnown(uint32_t radioModuleSN)
{
    bool found = false;
    beginTransaction();
    for (auto &device : _state.devices)
    {
        if (device.radioModule.sn == radioModuleSN)
        {
            found = true;
            break;
        }
    }
    endTransaction();
    return found;
}

bool GeniusDevicesService::updateRadioModuleRssi(uint32_t radioModuleSN, int8_t rssi, time_t when)
{
    bool updated = false;
    beginTransaction();
    for (auto &device : _state.devices)
    {
        if (device.radioModule.sn == radioModuleSN)
        {
            device.radioModule.rssi = rssi;
            device.radioModule.lastRangeTest = when;
            updated = true;
            break;
        }
    }
    endTransaction();
    return updated;
}

uint32_t GeniusDevicesService::markRadioModulesUnreached(const std::vector<uint32_t> &reached, time_t when)
{
    uint32_t stamped = 0;
    beginTransaction();
    for (auto &device : _state.devices)
    {
        if (device.radioModule.sn == 0)
            continue; // no radio module to range-test

        bool wasReached = false;
        for (uint32_t sn : reached)
        {
            if (sn == device.radioModule.sn)
            {
                wasReached = true;
                break;
            }
        }
        if (wasReached)
            continue; // already stamped with its measured RSSI

        device.radioModule.rssi = 0; // sentinel: tested, no response (out of direct range)
        device.radioModule.lastRangeTest = when;
        stamped++;
    }
    endTransaction();
    return stamped;
}

void GeniusDevicesService::persistRssiSurvey()
{
    // RSSI_SURVEY_UPDATE origin is skipped by the HA/sub-device sync handler (RSSI is not an HA
    // attribute), but still triggers the FSPersistence write handler - one flash write per survey.
    callUpdateHandlers(RSSI_SURVEY_UPDATE);
}

// ============================================================================
// GeniusDevice JSON helpers (shared between bulk-POST and per-device endpoints)
// ============================================================================

void GeniusDevice::importLegacyAlarms(JsonArray jsonAlarms)
{
    uint32_t count = 0;
    for (JsonVariant jsonAlarm : jsonAlarms)
    {
        const time_t start = Utils::iso8601_to_time_t(jsonAlarm["startTime"].as<String>());
        const time_t end = Utils::iso8601_to_time_t(jsonAlarm["endTime"].as<String>());
        const int reason = jsonAlarm["endingReason"].as<int>();

        GeniusDeviceEvent event = {};
        event.time = start;
        event.type = GDE_ALARM;
        if (reason == GAE_ALARM_ACTIVE)
        {
            event.value = GENIUS_ALARM_RUNNING;
        }
        else
        {
            event.set = end > start ? static_cast<uint32_t>(end - start) : 0;
            event.value = static_cast<uint32_t>(reason);
        }
        status.events.push_back(event);
        count++;
    }
    if (count == 0)
        return;

    // The alarms join whatever is in the log already (e.g. the readout the status was derived
    // from): order by time first, then keep the newest entries.
    status.sortEvents();
    status.trimEvents();
    alarmCount += count;
}

void GeniusDevice::detectorReplaced()
{
    status.forgetAll();
    alarmCount = 0;
    logEvent(GDE_CREATED, registration);
}

void GeniusDevice::statusToJson(JsonObject &root) const
{
    if (!status.flags.empty())
    {
        JsonArray arr = root["status"].to<JsonArray>();
        for (const GeniusFlagState &state : status.flags)
        {
            JsonObject obj = arr.add<JsonObject>();
            obj["flag"] = state.flag;
            obj["active"] = state.active;
            if (geniusTimeValid(state.since))
                obj["since"] = Utils::time_t_to_iso8601(state.since);
            obj["source"] = state.source;
        }
    }

    if (!status.events.empty())
    {
        JsonArray arr = root["events"].to<JsonArray>();
        for (const GeniusDeviceEvent &event : status.events)
        {
            JsonObject obj = arr.add<JsonObject>();
            if (geniusTimeValid(event.time))
                obj["time"] = Utils::time_t_to_iso8601(event.time);
            obj["type"] = event.type;
            switch (event.type)
            {
            case GDE_CREATED:
                obj["registration"] = event.value;
                break;
            case GDE_READOUT:
            case GDE_STATUS_PACKET:
                obj["set"] = event.set;
                obj["cleared"] = event.cleared;
                break;
            case GDE_MODULE_MISSING:
                // Three shapes: this device reports another module as missing (cause = that
                // module), this device is reported missing (set, cause = the reporter), or its
                // radio module was heard again (cleared).
                if (event.value)
                    obj["cause"] = event.value;
                if (event.set)
                    obj["set"] = event.set;
                if (event.cleared)
                    obj["cleared"] = event.cleared;
                break;
            case GDE_ALARM:
                if (event.value != GENIUS_ALARM_RUNNING && geniusTimeValid(event.time))
                    obj["end"] = Utils::time_t_to_iso8601(event.time + event.set);
                obj["endingReason"] = static_cast<int32_t>(event.value); // -1 while it lasts
                break;
            default:
                break;
            }
        }
    }
}

bool GeniusDevice::statusFromJson(JsonVariant src)
{
    const bool hasStatus = src["status"].is<JsonArray>();
    const bool hasEvents = src["events"].is<JsonArray>();
    if (!hasStatus && !hasEvents)
        return false;

    status.flags.clear();
    status.events.clear();

    if (hasStatus)
    {
        for (JsonVariant item : src["status"].as<JsonArray>())
        {
            const uint32_t flag = item["flag"].as<uint32_t>();
            // exactly one storable bit, and each flag only once
            if (flag == 0 || (flag & (flag - 1)) != 0 || !(flag & GENIUS_STATUS_STORED_MASK) || status.find(flag))
                continue;
            GeniusFlagState state = {};
            state.flag = flag;
            state.active = item["active"].as<bool>();
            state.since = item["since"].is<String>() ? Utils::iso8601_to_time_t(item["since"].as<String>()) : 0;
            state.source = item["source"].as<uint8_t>();
            status.flags.push_back(state);
        }
    }

    if (hasEvents)
    {
        for (JsonVariant item : src["events"].as<JsonArray>())
        {
            GeniusDeviceEvent event = {};
            event.type = item["type"].as<uint8_t>();
            if (event.type >= GDE_MAX)
                continue;
            event.time = item["time"].is<String>() ? Utils::iso8601_to_time_t(item["time"].as<String>()) : 0;
            switch (event.type)
            {
            case GDE_CREATED:
                event.value = item["registration"].as<uint32_t>();
                break;
            case GDE_READOUT:
            case GDE_STATUS_PACKET:
                event.set = item["set"].as<uint32_t>();
                event.cleared = item["cleared"].as<uint32_t>();
                break;
            case GDE_MODULE_MISSING:
                event.value = item["cause"] | 0U;
                event.set = item["set"] | 0U;
                event.cleared = item["cleared"] | 0U;
                break;
            case GDE_ALARM:
            {
                const int32_t reason = item["endingReason"].is<int>() ? item["endingReason"].as<int32_t>() : GAE_ALARM_ACTIVE;
                event.value = reason == GAE_ALARM_ACTIVE ? GENIUS_ALARM_RUNNING : static_cast<uint32_t>(reason);
                if (item["end"].is<String>())
                {
                    const time_t end = Utils::iso8601_to_time_t(item["end"].as<String>());
                    event.set = end > event.time ? static_cast<uint32_t>(end - event.time) : 0;
                }
                break;
            }
            default:
                break;
            }
            status.addEvent(event); // keeps the newest GENIUS_MAX_DEVICE_EVENTS
        }
    }
    return true;
}

bool GeniusDevice::applyReadoutStatus()
{
    if (readoutTime == 0)
        return false;

    uint32_t bits = GENIUS_STATUS_READOUT_MASK;
    // Old FM modules (FM.Basis / FM.Pro) report no radio status over SmartSonic - the readout's
    // radio fault flag is just a default there and must not mark the radio link as OK.
    if (isOldFmModule(radioModule.model))
        bits &= ~GENIUS_STATUS_RADIO;

    uint32_t fromReadout = 0;
    if (smokeDetector.batteryLowFault)
        fromReadout |= GENIUS_STATUS_BATT_LOW;
    if (smokeDetector.deviceFault)
        fromReadout |= GENIUS_STATUS_ERROR;
    if (radioModule.radioNetworkFault)
        fromReadout |= GENIUS_STATUS_RADIO;

    // A readout reports its flags completely: each one is either active or OK. It is always
    // logged, also when it only confirms what is known.
    const uint32_t reportedActive = fromReadout & bits;
    const uint32_t reportedOk = ~fromReadout & bits;
    const GeniusStatusDelta delta = status.apply(reportedActive, reportedOk, readoutTime, GDE_READOUT);
    status.addEvent({readoutTime, GDE_READOUT, reportedActive, reportedOk, 0});

    // The three flags are input only. They went into the status and the log above and are not
    // kept as a second, ageing copy next to them.
    smokeDetector.batteryLowFault = false;
    smokeDetector.deviceFault = false;
    radioModule.radioNetworkFault = false;
    return delta.any();
}

void GeniusDevice::logEvent(genius_device_event_t type, uint32_t value)
{
    status.addEvent({time(nullptr), static_cast<uint8_t>(type), 0, 0, value});
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

    // haEntityIdPrefix is backend-owned and deliberately not read from the client's JSON.
    // Status and activity log as stored (import); without them (new device, older export) the
    // status is derived from the readout
    if (!out.statusFromJson(src))
        out.applyReadoutStatus();

    // Exports from before v1.6.0 carry the alarms as a list of their own
    if (src["alarms"].is<JsonArray>())
        out.importLegacyAlarms(src["alarms"].as<JsonArray>());
    if (src["alarmCount"].is<uint32_t>())
        out.alarmCount = src["alarmCount"].as<uint32_t>();

    out.published = false; // new device - needs publish
    return true;
}

bool GeniusDevice::mergeFromJson(JsonVariant src)
{
    bool changed = false;

    JsonObject smokeDetectorJson = src["smokeDetector"].as<JsonObject>();
    JsonObject radioModuleJson = src["radioModule"].as<JsonObject>();

    GeniusSmokeDetectorInfo newSD = GeniusSmokeDetectorInfo::fromJson(smokeDetectorJson);
    GeniusRadioModuleInfo newRM = GeniusRadioModuleInfo::fromJson(radioModuleJson);

    // RSSI is backend-owned telemetry (written only by the ConfigCheckProbe survey), never by a
    // client edit. Preserve the current value across this merge so a location/identity edit that
    // round-trips the device object can't clobber a fresh probe measurement.
    // The same holds for haEntityIdPrefix, which this merge never reads from the JSON.
    const int8_t keepRssi = this->radioModule.rssi;
    const time_t keepLastRangeTest = this->radioModule.lastRangeTest;
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
        // Another smoke detector: status, activity log and alarm counter of the old one do not
        // hold for it. A readout that comes with the change is applied below.
        this->detectorReplaced();
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

    // Alarm line: merged independently of the readout-time bump below, since manual
    // line entry (old FM modules) updates these fields without a new readout.
    if (this->radioModule.lineId != newRM.lineId ||
        this->radioModule.lineCharacter != newRM.lineCharacter ||
        this->radioModule.lineNumber != newRM.lineNumber ||
        this->radioModule.lineManual != newRM.lineManual)
    {
        this->radioModule.lineId = newRM.lineId;
        this->radioModule.lineCharacter = newRM.lineCharacter;
        this->radioModule.lineNumber = newRM.lineNumber;
        this->radioModule.lineManual = newRM.lineManual;
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
        // A fresh readout is the newest word on battery and faults - take them over as current
        // status. The client's "status" object itself is ignored: status is backend-owned, like RSSI.
        this->applyReadoutStatus();
        changed = true;
    }

    String newLocation = src["location"].as<String>();
    if (this->location != newLocation)
    {
        this->location = newLocation;
        changed = true;
    }

    // The alarms are entries of the activity log, which is backend-owned like the status: a
    // client cannot change them.
    if (src["isAlarming"].is<bool>())
    {
        bool newIsAlarming = src["isAlarming"].as<bool>();
        if (this->isAlarming != newIsAlarming)
        {
            this->isAlarming = newIsAlarming;
            if (!newIsAlarming)
                this->status.endAlarm(time(nullptr), static_cast<uint32_t>(GAE_BY_IMPORT));
            changed = true;
        }
    }

    // Restore backend-owned RSSI telemetry (see note above): a client merge must not change it.
    this->radioModule.rssi = keepRssi;
    this->radioModule.lastRangeTest = keepLastRangeTest;

    if (changed)
        this->published = false;

    return changed;
}

StateUpdateResult GeniusDevices::update(JsonObject &root, GeniusDevices &geniusDevices, const String &originId)
{
    bool hasChanges = false;

    // FSPersistence::applyDefaults() calls update() with an empty JsonObject when no
    // config file exists yet (fresh device / after a flash erase). There's nothing to
    // migrate in that case - bail out before the version checks below mistake the
    // default-application path for a v0 file and log a spurious migration.
    if (!root["devices"].is<JsonArray>())
    {
        ESP_LOGV(GeniusDevices::TAG, "No devices array in JSON, no changes made.");
        return StateUpdateResult::UNCHANGED;
    }

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

    // Migration v1→v2: old FM modules (FM.Basis / FM.Pro) cannot expose a trustworthy
    // alarm line. Earlier firmware copied the unreliable acoustic line onto the device;
    // purge it unless it was entered by hand, so the UI surfaces "Line required" and
    // prompts manual entry. lineManual=true lines are kept.
    if (configVersion < 2 && originId == GATEWAY_DEVICES_FILE)
    {
        ESP_LOGI(GeniusDevices::TAG, "Migrating device config from v%d to v%d.", configVersion, GATEWAY_DEVICES_CONFIG_VERSION);
        if (root["devices"].is<JsonArray>())
        {
            for (JsonVariant dev : root["devices"].as<JsonArray>())
            {
                JsonObject rm = dev["radioModule"].as<JsonObject>();
                if (rm.isNull())
                    continue;
                int model = rm["model"].is<int>() ? rm["model"].as<int>() : GRM_UNKNOWN;
                if (isOldFmModule(static_cast<GeniusRadioModule>(model)) && !rm["lineManual"].as<bool>())
                {
                    rm.remove("lineId");
                    rm.remove("lineCharacter");
                    rm.remove("lineNumber");
                }
            }
        }
        hasChanges = true; // force save with new version
    }

    // v3: the alarms moved into the activity log ("events"), the status is kept per flag. Both
    // are converted while the devices are read below - only the write-back has to be forced.
    if (configVersion < 3 && originId == GATEWAY_DEVICES_FILE)
    {
        ESP_LOGI(GeniusDevices::TAG, "Migrating device config from v%d to v%d.", configVersion, GATEWAY_DEVICES_CONFIG_VERSION);
        hasChanges = true; // force save with new version
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

        // Skip duplicate IDs in the input JSON - without this, two entries with the same id
        // both pass the "not in existing state" check below (state is empty at startup) and
        // both land in newDevicesVector, perpetuating the corruption on every restart.
        if (std::find(processedDeviceIds.begin(), processedDeviceIds.end(), deviceId) != processedDeviceIds.end())
        {
            ESP_LOGW(GeniusDevices::TAG, "Duplicate device ID %lu in input JSON, skipping.", deviceId);
            hasChanges = true; // force a clean re-write so the duplicate is purged from the file
            continue;
        }
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

            // Status as stored; without one derive it from the readout - on the first boot after
            // the upgrade this gives read-out devices their battery / fault status right away.
            if (!newDevice.statusFromJson(jsonDeviceArrItem))
                newDevice.applyReadoutStatus();

            // Files from before v1.6.0 carry the alarms as a list of their own
            if (jsonDeviceArrItem["alarms"].is<JsonArray>())
                newDevice.importLegacyAlarms(jsonDeviceArrItem["alarms"].as<JsonArray>());
            if (jsonDeviceArrItem["alarmCount"].is<uint32_t>())
                newDevice.alarmCount = jsonDeviceArrItem["alarmCount"].as<uint32_t>();

            // The entity-ID prefix announced to Home Assistant is backend-owned: restore it
            // from the config file, never accept it from a client.
            if (originId == GATEWAY_DEVICES_FILE)
                newDevice.haEntityIdPrefix = jsonDeviceArrItem["haEntityIdPrefix"] | "";

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
                updatedDevice.detectorReplaced(); // another smoke detector: status, log and alarm counter start over
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
                // Fresh readout: take battery and faults over as current status (status is backend-owned otherwise)
                updatedDevice.applyReadoutStatus();
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

            // The alarms are entries of the activity log, which is backend-owned: not taken from a client.

            // Update isAlarming from frontend (frontend is responsible for correct state, e.g. clearing on import)
            if (jsonDeviceArrItem["isAlarming"].is<bool>())
            {
                bool newIsAlarming = jsonDeviceArrItem["isAlarming"].as<bool>();
                if (updatedDevice.isAlarming != newIsAlarming)
                {
                    updatedDevice.isAlarming = newIsAlarming;
                    if (!newIsAlarming)
                        updatedDevice.status.endAlarm(time(nullptr), static_cast<uint32_t>(GAE_BY_IMPORT));
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

String GeniusDevicesService::_haDeviceName(const GeniusDevice &device) const
{
    if (_haService->renameEntityIds() && !device.location.isEmpty())
        return device.location;
    return smokeDetectorModelName(device.smokeDetector.model);
}

String GeniusDevicesService::_entityIdPrefixFor(const GeniusDevice &device) const
{
    if (!_haService->renameEntityIds())
        return String();

    // The placeholder given to a detector that was added from a radio packet is not a
    // location - several such detectors would otherwise compete for the same IDs.
    String prefix;
    if (device.location != GENIUS_DEVICE_DEFAULT_LOCATION)
        prefix = HAUtils::slugify(device.location);

    if (prefix.isEmpty())
    {
        // No usable location: fall back to something unique, so the detector still gets an
        // explicit ID and the location-based IDs are freed. The serial number is what the
        // user can read off the detector.
        if (device.smokeDetector.sn != 0)
            prefix = "genius_" + String(device.smokeDetector.sn);
        else
            prefix = "genius_device_" + String(device.id);
    }
    return prefix;
}

bool GeniusDevicesService::_isEntityIdRenamePending(const GeniusDevice &device) const
{
    return _haService->isEnabled() && _haService->renameEntityIds() &&
           _entityIdPrefixFor(device) != device.haEntityIdPrefix;
}

bool GeniusDevicesService::_applySmokeDetectorIdentity(HADevice &haDevice, const GeniusDevice &device) const
{
    HADeviceIdentity &id = haDevice.identity();
    bool changed = false;

    auto assign = [&changed](String &field, const String &value)
    {
        if (field != value)
        {
            field = value;
            changed = true;
        }
    };

    assign(id.name, _haDeviceName(device));
    assign(id.serialNumber, String(device.smokeDetector.sn));
    assign(id.suggestedArea, device.location);
    assign(id.model, String(smokeDetectorModelName(device.smokeDetector.model)));

    // Kept current, but not a reason to republish on its own: the IP is not known at boot,
    // and HAService republishes every device on connect anyway.
    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
        id.configurationUrl = "http://" + localIP.toString() + "/gateway/smoke-detectors";

    const String prefix = _entityIdPrefixFor(device);
    if (haDevice.entityIdPrefix() != prefix)
    {
        haDevice.setEntityIdPrefix(prefix);
        changed = true;
    }

    return changed;
}

void GeniusDevicesService::_addSmokeDetectorSubDevice(const GeniusDevice &device)
{
    if (_haDevices.count(device.id))
        return;

    uint32_t devId = device.id;

    HADeviceIdentity identity;
    identity.id           = "genius-" + String(devId);
    identity.name         = _haDeviceName(device);
    identity.manufacturer = "Hekatron Vertriebs GmbH";
    identity.model        = smokeDetectorModelName(device.smokeDetector.model);
    identity.serialNumber = String(device.smokeDetector.sn);
    identity.suggestedArea = device.location;

    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
        identity.configurationUrl = "http://" + localIP.toString() + "/gateway/smoke-detectors";

    auto dev = std::make_unique<HADevice>(_haService, std::move(identity));
    dev->setEntityIdPrefix(_entityIdPrefixFor(device));

    // ---- Smoke alarm binary_sensor (Control) - reads live from _state.devices ----
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

    // ---- Diagnostics grouped publisher - status and readout entities share one state topic ----
    // Availability is folded into the JSON, in two flavours:
    //   - status entities (battery, detector/radio fault, dismount): their field is present once
    //     the flag's state is known - from a received status change, a readout or a manual reset
    //   - readout entities: {"available": bool}, true once an acoustic readout exists
    // Until then, HA shows them as "unavailable" rather than suggesting an OK nobody confirmed.
    HADevice *rawDev = _haService->addSubDevice(std::move(dev));

    auto diag = std::make_unique<HAGroupedSensorPublisher>(
        _haService, "diagnostics",
        [this, devId](JsonObject &state) {
            beginTransaction();
            const GeniusDevice *d = nullptr;
            for (const auto &dev : _state.devices)
                if (dev.id == devId) { d = &dev; break; }

            if (!d) {
                state["available"] = false;
                endTransaction();
                return;
            }

            // Status entities: current status, each flag only once known
            const uint32_t known = d->status.knownMask();
            const uint32_t flags = d->status.activeMask();
            if (known & GENIUS_STATUS_BATT_LOW) state["battery_low"]  = (flags & GENIUS_STATUS_BATT_LOW) != 0;
            if (known & GENIUS_STATUS_ERROR)    state["device_fault"] = (flags & GENIUS_STATUS_ERROR) != 0;
            // A radio module that other detectors report as missing is a radio problem of this
            // detector too. OK needs the radio link flag itself to be known: that a module is no
            // longer reported missing says nothing about its own radio link.
            const uint32_t radioFlags = GENIUS_STATUS_RADIO | GENIUS_STATUS_REPORTED_MISSING;
            if (flags & radioFlags)             state["radio_fault"]  = true;
            else if (known & GENIUS_STATUS_RADIO) state["radio_fault"] = false;
            if (known & GENIUS_STATUS_DISMOUNT) state["dismounted"]   = (flags & GENIUS_STATUS_DISMOUNT) != 0;

            // Readout entities
            if (d->readoutTime == 0) {
                state["available"] = false;
                endTransaction();
                return;
            }

            state["available"]           = true;
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
    // Evaluated from the same state topic message - no separate avail topic needed.
    // Captured by value below: the std::function copies stored in Sensor::extraConfig
    // outlive this function, so a [&availConfig] capture would dangle once we return.
    auto availConfig = [](JsonObject &c) {
        c["availability_topic"]   = "~/diagnostics/state";
        c["availability_template"] = "{{ 'online' if value_json.available else 'offline' }}";
    };
    // Status entities are available as soon as their own field is present (flag state known)
    auto statusAvailConfig = [](JsonObject &c, const char *field) {
        const String availTemplate = String("{{ 'online' if value_json.") + field + " is defined else 'offline' }}";
        c["availability_topic"]    = "~/diagnostics/state";
        c["availability_template"] = availTemplate;
    };

    diag->addSensor("binary_sensor", "battery_low", "Battery", "", HACategory::Diagnostic,
        [statusAvailConfig](JsonObject &c) {
            c["value_template"] = "{{ 'ON' if value_json.battery_low else 'OFF' }}";
            c["device_class"]   = "battery";
            statusAvailConfig(c, "battery_low");
        })
    .addSensor("binary_sensor", "device_fault", "Smoke Detector State", "", HACategory::Diagnostic,
        [statusAvailConfig](JsonObject &c) {
            c["value_template"] = "{{ 'ON' if value_json.device_fault else 'OFF' }}";
            c["device_class"]   = "problem";
            statusAvailConfig(c, "device_fault");
        })
    .addSensor("binary_sensor", "radio_fault", "Radio Module State", "", HACategory::Diagnostic,
        [statusAvailConfig](JsonObject &c) {
            c["value_template"] = "{{ 'ON' if value_json.radio_fault else 'OFF' }}";
            c["device_class"]   = "problem";
            statusAvailConfig(c, "radio_fault");
        })
    .addSensor("binary_sensor", "dismounted", "Dismount", "", HACategory::Diagnostic,
        [statusAvailConfig](JsonObject &c) {
            c["value_template"] = "{{ 'ON' if value_json.dismounted else 'OFF' }}";
            c["device_class"]   = "tamper";
            statusAvailConfig(c, "dismounted");
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
    // publishAll() sends both the discovery config for all diagnostic entities AND the
    // initial state. begin() only registers a future onPublishAll callback and
    // does not publish immediately, so calling it alone would silently skip the
    // diagnostic configs for dynamically added devices.
    if (_haService->isReady())
    {
        auto &entry = _haDevices[device.id];
        if (entry.diagnostics)
            entry.diagnostics->publishAll();
    }

    _syncLinkEntities(device);
}

void GeniusDevicesService::_syncLinkEntities(const GeniusDevice &device)
{
    auto it = _haDevices.find(device.id);
    if (it == _haDevices.end() || it->second.device == nullptr)
        return;

    const bool supervised = device.radioModule.model == GRM_FM_PRO_X && device.radioModule.sn != 0;
    if (supervised == (it->second.link != nullptr))
        return;

    if (!supervised)
    {
        // The module was replaced by one without radio link supervision: the entities go
        if (_haService->isReady())
            it->second.link->unpublishAll();
        it->second.link.reset();
        return;
    }

    const uint32_t devId = device.id;
    auto link = std::make_unique<HAGroupedSensorPublisher>(
        _haService, "link",
        [this, devId](JsonObject &state) {
            // RAM-only values. A field is present once it is known, i.e. once the module was
            // heard since the gateway started - until then the entities are unavailable.
            ModuleRuntime entry;
            if (!_runtimeOf(devId, entry) || entry.lastSeen == 0)
                return;
            state["last_seen"] = Utils::time_t_to_iso8601(entry.lastSeen);
            if (entry.rssiTime != 0)
                state["signal"] = entry.rssi;
        },
        it->second.device);

    link->addSensor("sensor", "last_seen", "Last Seen", "mdi:radio-tower", HACategory::Diagnostic,
        [](JsonObject &c) {
            c["value_template"]        = "{{ value_json.last_seen }}";
            c["device_class"]          = "timestamp";
            c["availability_topic"]    = "~/link/state";
            c["availability_template"] = "{{ 'online' if value_json.last_seen is defined else 'offline' }}";
        })
    .addSensor("sensor", "signal_strength", "Signal Strength", "", HACategory::Diagnostic,
        [](JsonObject &c) {
            c["value_template"]        = "{{ value_json.signal }}";
            c["device_class"]          = "signal_strength";
            c["unit_of_measurement"]   = "dBm";
            c["state_class"]           = "measurement";
            c["availability_topic"]    = "~/link/state";
            c["availability_template"] = "{{ 'online' if value_json.signal is defined else 'offline' }}";
        });

    link->begin();
    if (_haService->isReady())
        link->publishAll();
    it->second.link = std::move(link);
}

void GeniusDevicesService::_removeSmokeDetectorSubDevice(uint32_t deviceId)
{
    auto it = _haDevices.find(deviceId);
    if (it != _haDevices.end())
    {
        if (it->second.diagnostics && _haService->isReady())
            it->second.diagnostics->unpublishAll();
        if (it->second.link && _haService->isReady())
            it->second.link->unpublishAll();
        _haDevices.erase(it);
    }
    _haService->removeSubDevice("genius-" + String(deviceId));
}

void GeniusDevicesService::_syncSmokeDetectorSubDevices(bool requestRename, std::vector<uint32_t> *deferredRepublish)
{
    bool renamePending = false;
    std::vector<uint32_t> republish;

    // Lock order: HA registry lock first, state mutex second. HAService::publishAll()
    // runs in its own task, holds the registry lock per device and calls the state
    // readers below, which take the state mutex. Entering the transaction first and
    // asking for the registry lock afterwards (as removeSubDevice() and the destructor
    // of the diagnostics publisher do) could deadlock with it. Waiting for the registry
    // lock out here also keeps the state mutex - and with it the alarm path - free
    // while a publishAll() is busy with a device.
    // The lock covers the changes to the HA objects only. It is released again before
    // the detectors are published further down.
    _haService->lock();
    beginTransaction();

    for (uint32_t deviceId : _state.deletedDeviceIds)
        _removeSmokeDetectorSubDevice(deviceId);
    _state.deletedDeviceIds.clear();

    for (GeniusDevice &device : _state.devices)
    {
        // HA never changes an entity_id once registered. Whether a detector's entities
        // have to be removed and created again is decided by comparing the prefix last
        // announced to HA with the one the detector should have - for every detector,
        // on every sync.
        const bool needsRename = _isEntityIdRenamePending(device);
        renamePending = renamePending || needsRename;

        auto it = _haDevices.find(device.id);
        if (it == _haDevices.end())
        {
            _addSmokeDetectorSubDevice(device);
            device.published = true;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (it->second.device == nullptr)
            continue;

        // Config changed (e.g. location or serial number) or the HA settings changed the
        // display name - update the existing HADevice identity in place and republish its
        // discovery. Avoids remove+re-add, which causes HA to ignore the new
        // suggested_area because its device registry entry persists (Bug 2).
        const bool identityChanged = _applySmokeDetectorIdentity(*it->second.device, device);
        _syncLinkEntities(device); // the radio module model decides whether they exist
        if (!device.published || identityChanged)
        {
            // A detector with a pending rename is published by the rename task, after its
            // old configs were cleared - publishing it here as well would only double the
            // traffic.
            if (!needsRename)
                republish.push_back(device.id);
            device.published = true;
        }
    }

    endTransaction();
    _haService->unlock();

    if (deferredRepublish != nullptr)
    {
        // The caller publishes them, after it has done what is more urgent.
        *deferredRepublish = std::move(republish);
        republish.clear();
    }

    // Publish detector by detector, taking the mutex for each one separately, so the alarm
    // path is not locked out for the whole list.
    for (uint32_t deviceId : republish)
    {
        _publishSmokeDetectorDiscovery(deviceId);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (renamePending && requestRename)
    {
        // A sync that belongs to an import: the user asked for an import, not for a rename.
        // The cycle runs in the background - the import dialog of the web interface reports
        // it as one of its steps instead of the rename progress dialog.
        _requestEntityIdRename(_isImportSync());
    }
}

bool GeniusDevicesService::_publishSmokeDetectorDiscovery(uint32_t deviceId, String *announcedPrefix)
{
    bool published = false;

    beginTransaction();

    auto it = _haDevices.find(deviceId);
    if (it != _haDevices.end() && it->second.device != nullptr && _haService->isReady())
    {
        HADevice *haDevice = it->second.device;
        const uint32_t failuresBefore = haDevice->configPublishFailures();

        if (announcedPrefix != nullptr)
            *announcedPrefix = haDevice->entityIdPrefix();

        haDevice->publishAll();
        if (it->second.diagnostics)
            it->second.diagnostics->publishAll();
        if (it->second.link)
            it->second.link->publishAll();

        published = _haService->isReady() && haDevice->configPublishFailures() == failuresBefore;
    }

    endTransaction();

    return published;
}

// ============================================================================
// Private Methods - HA Entity-ID Rename
// ============================================================================

void GeniusDevicesService::_requestEntityIdRename(bool background)
{
    // One bit per kind of request, so an announced request is not lost behind a background
    // one that is still waiting for the task.
    if (_haRenameTaskHandle != nullptr)
        xTaskNotify(_haRenameTaskHandle,
                    background ? HA_RENAME_NOTIFY_BACKGROUND : HA_RENAME_NOTIFY_ANNOUNCED,
                    eSetBits);
}

void GeniusDevicesService::_haRenameTask()
{
    ESP_LOGI(GeniusDevices::TAG, "HA entity-ID rename task started.");
    while (true)
    {
        uint32_t requests = 0;
        xTaskNotifyWait(0, UINT32_MAX, &requests, portMAX_DELAY);
        // In the background only if nothing else asked for this cycle
        const bool background = (requests & HA_RENAME_NOTIFY_ANNOUNCED) == 0;

        // The HA settings were applied: refresh name and entity-ID prefix of every detector
        // here instead of in the HTTP task. The sync does not notify this task again -
        // the rename below picks up whatever it left pending.
        std::vector<uint32_t> republish;
        if (_haSyncRequested.exchange(false))
        {
            ESP_LOGI(GeniusDevices::TAG, "HA settings applied - refreshing detector identities.");
            _syncSmokeDetectorSubDevices(false, &republish);
        }

        _runEntityIdRename(background);

        // Detectors that keep their entity IDs but changed otherwise - the rename option
        // changes the display name of every detector. They are sent after the rename, so
        // the progress of the rename shows up at once instead of after dozens of configs.
        for (uint32_t deviceId : republish)
        {
            // A location edited in the meantime made a rename pending: the next cycle
            // publishes that detector, after its old configs were cleared.
            beginTransaction();
            auto device = std::find_if(_state.devices.begin(), _state.devices.end(),
                                       [deviceId](const GeniusDevice &d) { return d.id == deviceId; });
            const bool publish = device != _state.devices.end() && !_isEntityIdRenamePending(*device);
            endTransaction();

            if (publish)
                _publishSmokeDetectorDiscovery(deviceId);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void GeniusDevicesService::_emitEntityRenameEvent(const char *state, uint32_t count, const std::vector<String> &names, bool background)
{
    JsonDocument jsonDoc;
    JsonObject jsonRoot = jsonDoc.to<JsonObject>();
    jsonRoot["state"] = state;
    jsonRoot["count"] = count;
    // A rename can start without the user having asked for it on this page (import, option
    // switched on, catch-up after a reconnect), so the event says which detectors are meant.
    JsonArray jsonNames = jsonRoot["detectors"].to<JsonArray>();
    for (const String &name : names)
        jsonNames.add(name);
    // Caused by an import: the web interface shows no progress, only what could not be sent
    if (background)
        jsonRoot["background"] = true;
    _eventSocket->emitEvent(HA_ENTITY_RENAME_EVENT, jsonRoot);
}

void GeniusDevicesService::_runEntityIdRename(bool background)
{
    // ---- Collect the detectors whose announced prefix differs from the desired one ----
    std::vector<uint32_t> targets;
    std::vector<String> names; // locations of the first detectors, for the progress event

    beginTransaction();
    for (const GeniusDevice &device : _state.devices)
    {
        if (_isEntityIdRenamePending(device) && _haDevices.count(device.id))
        {
            targets.push_back(device.id);
            if (names.size() < HA_ENTITY_RENAME_EVENT_MAX_NAMES)
                names.push_back(device.location);
        }
    }
    endTransaction();

    if (targets.empty())
        return;

    const uint32_t count = targets.size();

    if (!_haService->isReady())
    {
        // Nothing can be sent now. The prefixes stay as they are, so the comparison on the
        // next MQTT connect finds the same detectors again - also across a reboot.
        ESP_LOGI(GeniusDevices::TAG, "HA entity-ID rename of %lu detector(s) deferred until MQTT is connected.",
                 (unsigned long)count);
        _emitEntityRenameEvent("deferred", count, names, background);
        return;
    }

    ESP_LOGI(GeniusDevices::TAG, "Renaming the HA entities of %lu detector(s).", (unsigned long)count);
    _emitEntityRenameEvent("started", count, names, background);

    // ---- Step 1: clear the retained discovery configs, so HA removes the entities ----
    // The HADevices already carry the new identity (written by the sync), so a concurrent
    // HAService::publishAll() would publish the new configs too - still correct for HA,
    // which queues a config behind a running removal of the same topic.
    // QoS 1: a removal that got lost would leave the old entity ID in place.
    // The retained diagnostics state stays on the broker: HA reads it the moment the
    // entities are back, so they do not sit at "unavailable" until the republished state
    // has trickled through the MQTT outbox.
    _emitEntityRenameEvent("removing", count, names, background);
    for (uint32_t deviceId : targets)
    {
        beginTransaction();
        auto it = _haDevices.find(deviceId);
        if (it != _haDevices.end() && it->second.device != nullptr)
        {
            it->second.device->unpublishAll(1);
            if (it->second.diagnostics)
                it->second.diagnostics->unpublishAll(1, true);
            if (it->second.link)
                it->second.link->unpublishAll(1, true);
        }
        endTransaction();
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (!_haService->isReady())
    {
        ESP_LOGW(GeniusDevices::TAG, "MQTT connection lost while removing HA entities - the rename is repeated on the next connect.");
        _emitEntityRenameEvent("failed", count, names, background);
        return;
    }

    // ---- Step 2: give HA time to finish the removal - without holding the mutex ----
    // HA remembers removed entities and restores their old entity_id when the same
    // unique_id comes back, unless the new config carries a different default_entity_id.
    _emitEntityRenameEvent("waiting", count, names, background);
    vTaskDelay(pdMS_TO_TICKS(HA_ENTITY_RENAME_SETTLE_MS));

    // ---- Step 3: publish again and remember what was announced ----
    _emitEntityRenameEvent("publishing", count, names, background);
    uint32_t sent = 0;
    bool prefixStored = false;
    for (uint32_t deviceId : targets)
    {
        // One transaction per detector keeps publishing and bookkeeping together while
        // leaving the mutex free between detectors.
        beginTransaction();

        String announced;
        const bool published = _publishSmokeDetectorDiscovery(deviceId, &announced);

        auto device = std::find_if(_state.devices.begin(), _state.devices.end(),
                                   [deviceId](const GeniusDevice &d) { return d.id == deviceId; });
        if (device == _state.devices.end())
        {
            sent++; // deleted while the cycle ran - nothing left to rename
        }
        else if (published)
        {
            sent++;
            // Store the prefix the configs were actually published with. It may differ from
            // the one this cycle started with (location edited again during the wait) or be
            // empty (option switched off meanwhile) - then the detector is unmanaged and the
            // stored prefix stays untouched.
            if (!announced.isEmpty() && device->haEntityIdPrefix != announced)
            {
                device->haEntityIdPrefix = announced;
                prefixStored = true;
            }
        }

        endTransaction();
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // One flash write for the whole batch. The origin keeps the sync handler from running again.
    if (prefixStored)
        callUpdateHandlers(HA_ENTITY_ID_PREFIX_UPDATE);

    if (sent == count)
    {
        // HA does not acknowledge discovery messages: "sent" is all that can be claimed.
        ESP_LOGI(GeniusDevices::TAG, "New HA entity IDs of %lu detector(s) sent.", (unsigned long)count);
        _emitEntityRenameEvent("sent", count, names, background);
    }
    else
    {
        ESP_LOGW(GeniusDevices::TAG, "HA entity-ID rename incomplete (%lu of %lu detector(s) published) - the rest is repeated on the next connect.",
                 (unsigned long)sent, (unsigned long)count);
        _emitEntityRenameEvent("failed", count, names, background);
    }

    // A full cycle is the deepest this task ever goes (JSON document, MQTT publish, log
    // formatting). Report what is left, so a stack that got too tight shows in the log
    // before it overflows.
    ESP_LOGI(GeniusDevices::TAG, "HA entity-ID rename task: %u bytes of stack never used.",
             (unsigned)uxTaskGetStackHighWaterMark(nullptr));
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

/**
 * @brief Builds the simple fault topic payload
 *
 * {"numFaulty": n, "numBatteryLow": n, "devices": [{"sn": ..., "location": "...", "status": ["battery_low", ...]}]}
 *
 * Lists exactly the devices that count in numFaulty: those with at least one active fault.
 * "status" names their faults by the key from GENIUS_STATUS_FLAGS. Other active flags of such a
 * device (a fault muted on the detector, flags not known yet as "flag_0xNNNN") follow in an
 * optional "info" list, so nothing is swallowed.
 */
String GeniusDevicesService::_buildSimpleFaultPayload()
{
    JsonDocument doc;

    beginTransaction();
    doc["numFaulty"] = _numFaulty;
    doc["numBatteryLow"] = _numBatteryLow;
    JsonArray devices = doc["devices"].to<JsonArray>();
    for (const GeniusDevice &device : _state.devices)
    {
        const uint32_t active = device.status.activeMask();
        const uint32_t faults = active & GENIUS_STATUS_FAULT_MASK;
        if (faults == 0)
            continue; // list and counters name the same devices

        JsonObject entry = devices.add<JsonObject>();
        entry["sn"] = device.smokeDetector.sn;
        entry["location"] = device.location;
        JsonArray flags = entry["status"].to<JsonArray>();
        JsonArray others; // created with the first flag that is no fault
        for (uint32_t bit = 1; bit != 0; bit <<= 1)
        {
            if (!(active & bit))
                continue;

            char key[20];
            const GeniusStatusFlagInfo *info = geniusStatusFlagInfo(bit);
            if (info)
                snprintf(key, sizeof(key), "%s", info->key);
            else
                snprintf(key, sizeof(key), "flag_0x%04lX", (unsigned long)bit);

            if (faults & bit)
            {
                flags.add(key);
            }
            else
            {
                if (others.isNull())
                    others = entry["info"].to<JsonArray>();
                others.add(key);
            }
        }
    }
    endTransaction();

    String payload;
    serializeJson(doc, payload);
    return payload;
}

/**
 * @brief Publishes the smoke detectors' status summary to the simple fault MQTT topic
 *
 * Retained, so subscribers always get the current state. Independent of Home Assistant
 * integration. Called whenever the payload changes (see _updateStatusSummary()), on MQTT
 * connect from GeniusGateway::_mqttPublishTask() and when the publishing settings change.
 */
void GeniusDevicesService::mqttPublishSimpleFaultState()
{
    if (!_cachedAlarmPublishingSettings.faultEnabled)
        return;

    if (_mqttClient == nullptr || !_mqttClient->connected())
        return;

    if (_cachedAlarmPublishingSettings.faultTopic.isEmpty())
    {
        ESP_LOGE(GeniusDevices::TAG, "Fault MQTT topic is empty. Cannot publish fault state.");
        return;
    }

    beginTransaction(); // guards _lastFaultPayload against concurrent publishers
    String payload = _buildSimpleFaultPayload();
    if (_mqttClient->publish(_cachedAlarmPublishingSettings.faultTopic.c_str(), 0, true, payload.c_str()) == -1)
        ESP_LOGE(GeniusDevices::TAG, "Failed to publish simple fault state.");
    else
        _lastFaultPayload = payload;
    endTransaction();
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
        const bool faultWasEnabled = _cachedAlarmPublishingSettings.faultEnabled;
        const String oldFaultTopic = _cachedAlarmPublishingSettings.faultTopic;

        _cachedAlarmPublishingSettings = _alarmPublishingSettings->getSettingsCopy();

        // Fault publishing was switched off or moved to another topic: remove the retained
        // summary from the old topic, so no subscriber keeps reading a state that is no longer
        // maintained.
        if (faultWasEnabled && !oldFaultTopic.isEmpty() &&
            (!_cachedAlarmPublishingSettings.faultEnabled || _cachedAlarmPublishingSettings.faultTopic != oldFaultTopic))
        {
            if (_mqttClient != nullptr && _mqttClient->connected())
                _mqttClient->publish(oldFaultTopic.c_str(), 0, true, "");
            beginTransaction();
            _lastFaultPayload = String();
            endTransaction();
        }
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
        // A detector that is new to the gateway has no entities in Home Assistant yet: its
        // first publish already carries the right entity IDs, so there is nothing to rename.
        newDevice.haEntityIdPrefix = _entityIdPrefixFor(newDevice);
        newDevice.logEvent(GDE_CREATED, newDevice.registration);
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
GeniusDevicesService::resetStatus(uint32_t deviceId, GeniusDevice &out)
{
    if (deviceId == 0)
        return DeviceOpResult::INVALID_BODY;

    DeviceOpResult result = DeviceOpResult::NOT_FOUND;

    beginTransaction();
    for (GeniusDevice &device : _state.devices)
    {
        if (device.id != deviceId)
            continue;

        const uint32_t active = device.status.activeMask();
        if (active != 0)
        {
            ESP_LOGI(GeniusDevices::TAG, "Status of smoke detector with SN '%lu' reset manually (was 0x%04lX).",
                     device.smokeDetector.sn, (unsigned long)active);
            // The active flags become unknown, not OK: the gateway cannot check that the fault
            // is gone, and the detector does not announce a fault that persists a second time.
            device.status.reset();
            device.logEvent(GDE_STATUS_RESET);
            result = DeviceOpResult::OK;
        }
        else
        {
            result = DeviceOpResult::UNCHANGED;
        }
        out = device;
        break;
    }
    endTransaction();

    if (result == DeviceOpResult::OK)
    {
        callUpdateHandlers(DEVICE_STATUS_CHANGE); // persist, update summary, notify front end

        beginTransaction();
        _publishSmokeDetectorAttributes(deviceId);
        endTransaction();
    }

    return result;
}

GeniusDevicesService::DeviceOpResult
GeniusDevicesService::reorderDevices(const std::vector<uint32_t> &newOrder)
{
    bool orderChanged = false;
    bool valid = true;

    beginTransaction();

    // The new order must contain exactly the current id set - same size, same elements.
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
    // 16 hex chars = 64 bits - plenty of entropy for a single-slot session id.
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

    // The import file is client data and never carries a trusted entity-ID prefix. Detectors
    // that stay keep what was last announced to Home Assistant; all others start empty, so
    // their entity IDs are reconciled (they may already exist in HA from an earlier setup).
    for (auto &staged : _importSession.staging)
    {
        auto existing = std::find_if(_state.devices.begin(), _state.devices.end(),
                                     [&staged](const GeniusDevice &d) { return d.id == staged.id; });
        if (existing != _state.devices.end())
            staged.haEntityIdPrefix = existing->haEntityIdPrefix;
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
        _runPostCommit(); // fallback if task wasn't created
    return ImportResult::OK;
}

void GeniusDevicesService::_runPostCommit()
{
    // The handlers run in this task. Marking it lets the sync handler tell an import from an
    // edit, although both arrive with the same origin id.
    _importSyncTask = xTaskGetCurrentTaskHandle();
    callUpdateHandlers(HTTP_ENDPOINT_ORIGIN_ID);
    _importSyncTask = nullptr;

    _emitImportEvent("done");
}

void GeniusDevicesService::_emitImportEvent(const char *state)
{
    JsonDocument jsonDoc;
    JsonObject jsonRoot = jsonDoc.to<JsonObject>();
    jsonRoot["state"] = state;

    if (strcmp(state, "saved") == 0)
    {
        // Whether a Home Assistant step follows at all
        jsonRoot["ha"] = _haService->isEnabled();
    }
    else
    {
        // What was done for Home Assistant. Discovery messages are not acknowledged, so
        // "sent" is all that can be claimed.
        const char *ha = "sent";
        if (!_haService->isEnabled())
        {
            ha = "disabled";
        }
        else if (!_haService->isReady())
        {
            ha = "offline"; // nothing was sent; the next MQTT connect publishes everything
        }
        else
        {
            // Detectors whose entity IDs still have to change are handled by the rename task,
            // which reports the end of its (background) cycle with HA_ENTITY_RENAME_EVENT.
            bool renamePending = false;
            beginTransaction();
            for (const GeniusDevice &device : _state.devices)
            {
                if (_isEntityIdRenamePending(device) && _haDevices.count(device.id))
                {
                    renamePending = true;
                    break;
                }
            }
            endTransaction();
            if (renamePending)
                ha = "renaming";
        }
        jsonRoot["ha"] = ha;
    }

    _eventSocket->emitEvent(DEVICE_IMPORT_EVENT, jsonRoot);
}

void GeniusDevicesService::_postCommitTask()
{
    ESP_LOGI(GeniusDevices::TAG, "post-commit task started.");
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(GeniusDevices::TAG, "post-commit: running deferred FS write + HA sync.");
        _runPostCommit();
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
