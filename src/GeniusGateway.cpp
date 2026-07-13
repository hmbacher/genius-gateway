/**
 * @file GeniusGateway.cpp
 * @brief Implementation of the main gateway service
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

#include <GeniusGateway.h>
#include <GatewaySettingsService.h>
#include <IPUtils.h>
#include <Utils.hpp>

TaskHandle_t GeniusGateway::xRxTaskHandle = nullptr;

static void nofifyReceivedPacket() // !!! This function is called from ISR !!!
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Notify the waiting (blocked) RX task that a packet is ready to be read from RX FIFO
    vTaskNotifyGiveIndexedFromISR(GeniusGateway::xRxTaskHandle,
                                  RX_TASK_NOTIFICATION_INDEX,
                                  &xHigherPriorityTaskWoken);

    /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch should be
       performed to ensure the interrupt returns directly to the highest priority task. */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

GeniusGateway::GeniusGateway(ESP32SvelteKit *sveltekit) : _server(sveltekit->getServer()),
                                                          _securityManager(sveltekit->getSecurityManager()),
                                                          _mqttClient(sveltekit->getMqttClient()),
                                                          _sveltekit(sveltekit),
                                                          _alarmPublishingSettingsService(sveltekit),
                                                          _reportSettingsService(sveltekit),
                                                          _gatewaySettings(sveltekit),
                                                          _gatewayDeviceMqttService(sveltekit->getHAService(), &_gatewaySettings),
                                                          _geniusDevices(sveltekit, _mqttClient, &_alarmPublishingSettingsService),
                                                          _alarmLines(sveltekit, _mqttClient, &this->_cc1101Controller, &_alarmPublishingSettingsService),
                                                          _wsLogger(sveltekit),
                                                          _visualizerSettingsService(sveltekit),
                                                          _cc1101Controller(sveltekit),
                                                          _cc1101PinsService(sveltekit),
                                                          _alarmBlocker(sveltekit),
                                                          _eventSocket(sveltekit->getSocket()),
                                                          _lastPacketHash(0),
                                                          _hasLastPacketHash(false)
{
}

void GeniusGateway::begin()
{
    /* Create packet handling task - stack forced to internal DRAM so ISR-driven
     * task notifications and real-time CC1101 reads are not slowed by PSRAM latency.
     * CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=128 would otherwise route this stack to PSRAM. */
    BaseType_t xReturned;
    xReturned = xTaskCreatePinnedToCoreWithCaps(
        this->_rx_packetsImpl,
        RX_TASK_NAME,
        RX_TASK_STACK_SIZE,
        this,
        RX_TASK_PRIORITY,
        &GeniusGateway::xRxTaskHandle,
        RX_TASK_CORE_AFFINITY,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (xReturned == pdPASS)
    {
        ESP_LOGI(TAG, "RX task created (%p).", GeniusGateway::xRxTaskHandle);
        // Radio bring-up happens below, from the persisted pin configuration, once the
        // CC1101PinsService has loaded (see _cc1101Controller.bringUp()).
    }
    else
    {
        ESP_LOGE(TAG, "RX task creation failed.");
    }

    /* Register events and start fast services BEFORE the slow FS-loading services below.
     * A WebSocket client that auto-reconnects right after MQTT connects can subscribe
     * before 50-device JSON loading completes; pre-registering here prevents the
     * "unregistered event" warnings for rem-alarm-block-time / alarm / cc1101_status. */
    _cc1101Controller.begin();
    _alarmBlocker.begin();
    _eventSocket->registerEvent(GATEWAY_EVENT_ALARM);
    _eventSocket->registerEvent(GATEWAY_EVENT_CONFIG_CHECK_PROBE);

    /* ConfigCheckProbe survey: mutex guarding the session state. The response window and
     * finalize run on the dedicated worker task created below. */
    _probeMutex = xSemaphoreCreateMutex();

    /* Initialize Alarm Publishing Settings Service first - other services depend on its settings */
    _alarmPublishingSettingsService.begin();
    /* Initialize Report Settings Service */
    _reportSettingsService.begin();
    /* Initialize Gateway Settings Service - must be before Gateway Device MQTT Service */
    _gatewaySettings.begin();
    /* Initialize Gateway Device MQTT Service */
    _gatewayDeviceMqttService.begin();
    /* Initialize Gateway Devices Service */
    _geniusDevices.begin();
    /* Initialize Alarm Lines Service */
    _alarmLines.begin();
    /* Initialize WS Logger */
    _wsLogger.begin();
    /* Initialize Packet Vizualizer Settings */
    _visualizerSettingsService.begin();
    /* Initialize CC1101 runtime pin configuration service */
    _cc1101PinsService.begin();

    /* Bring up the radio from the persisted pin configuration (must follow _cc1101PinsService.begin()).
     * Stays UNCONFIGURED if no valid pins are set; RX monitoring is enabled only on success. */
    if (GeniusGateway::xRxTaskHandle != nullptr)
    {
        _cc1101Controller.bringUp(&_cc1101PinsService, nofifyReceivedPacket);
        // Re-initialize the radio live whenever the pin configuration changes (no reboot).
        _cc1101PinsService.addUpdateHandler([this](const String & /*originId*/)
                                            { _cc1101Controller.reconfigure(); },
                                            false);
    }
    else
        ESP_LOGE(TAG, "Skipping CC1101 bring-up: RX task not available.");

    /* Perform a full publish (all devices and states), if MQTT client connects.
     * The actual work runs in a persistent task (_mqttPublishTask) that blocks on
     * a task notification. onConnect simply wakes it - the MQTT event task is
     * freed immediately, which is required for synchronous (async=false) publishes
     * to work without deadlocking. */
    _mqttClient->onConnect([this](bool /*sessionPresent*/)
                           {
                               xTaskNotifyGive(_mqttPublishTaskHandle);
                           });

    xTaskCreatePinnedToCore(
        _mqttPublishTaskImpl,
        "mqtt-ha-publish",
        6144,
        this,
        5,
        &_mqttPublishTaskHandle,
        ESP32SVELTEKIT_RUNNING_CORE);

    /* ConfigCheckProbe finalize worker: runs the survey close-out (device RSSI updates, one flash
     * write, WS emit) on a task that may block - not the HTTP handler thread that starts the probe. */
    xTaskCreatePinnedToCore(
        _probeFinalizeTaskImpl,
        "ccprobe-final",
        4096,
        this,
        5,
        &_probeFinalizeTaskHandle,
        ESP32SVELTEKIT_RUNNING_CORE);

    /* Register endpoint to end all alarms and block new alarms for a specified amount of time */
    _server->on(GATEWAY_SERVICE_PATH_END_ALARMS,
                HTTP_POST,
                _securityManager->wrapCallback(std::bind(&GeniusGateway::_handleEndAlarming, this, std::placeholders::_1, std::placeholders::_2),
                                               AuthenticationPredicates::IS_ADMIN));

    /* Register endpoint to end alarm blocking immediately */
    _server->on(GATEWAY_SERVICE_PATH_END_ALARMBLOCKING,
                HTTP_POST,
                _securityManager->wrapRequest(std::bind(&GeniusGateway::_handleEndBlocking, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_ADMIN));

    /* Register endpoint to start a ConfigCheckProbe RSSI survey (measure direct-reachable modules) */
    _server->on(GATEWAY_SERVICE_PATH_PROBE,
                HTTP_POST,
                _securityManager->wrapRequest(std::bind(&GeniusGateway::_handleStartProbe, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_ADMIN));

    /* Register endpoint to finalize the running ConfigCheckProbe survey now (user pressed "Stop probing") */
    _server->on(GATEWAY_SERVICE_PATH_PROBE_STOP,
                HTTP_POST,
                _securityManager->wrapRequest(std::bind(&GeniusGateway::_handleStopProbe, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_ADMIN));

    /* Register endpoint to abort the running ConfigCheckProbe survey (user closed the progress dialog) */
    _server->on(GATEWAY_SERVICE_PATH_PROBE_CANCEL,
                HTTP_POST,
                _securityManager->wrapRequest(std::bind(&GeniusGateway::_handleCancelProbe, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_ADMIN));

    /* Register endpoint to read the last survey's discovered (unknown) responders */
    _server->on(GATEWAY_SERVICE_PATH_PROBE_DISCOVERED,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&GeniusGateway::_handleGetDiscovered, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));
}

esp_err_t GeniusGateway::_handleEndAlarming(PsychicRequest *request, JsonVariant &json)
{
    if (!json.is<JsonObject>())
        return request->reply(400, "application/json", "{\"success\": false, \"reason\": \"Invalid JSON\"}");

    JsonObject jsonObject = json.as<JsonObject>();
    if (!jsonObject["alarmBlockingTime"].is<uint32_t>())
        return request->reply(400, "application/json", "{\"success\": false, \"reason\": \"Missing or invalid alarm blocking time.\"}");

    // Read seconds to block new alarms
    uint32_t blockingTimeS = jsonObject["alarmBlockingTime"].as<uint32_t>();

    if (blockingTimeS > GATEWAY_MAX_ALARM_BLOCKING_TIME_S)
    {
        ESP_LOGW(TAG, "Invalid alarm blocking time: %lu seconds. Must be between 1 and %lu seconds.", blockingTimeS, GATEWAY_MAX_ALARM_BLOCKING_TIME_S);
        return request->reply(400, "application/json", "{\"success\": false, \"reason\": \"Maximimum alarm blocking time exceeded.\"}");
    }

    if (_geniusDevices.resetAllAlarms())
    {
        _geniusDevices.mqttPublishAllDevicesState(); // Re-Publish all silenced devices' state
        _emitAlarmState();
    }

    ESP_LOGI(TAG, "All active alarms have been ended.");

    if (blockingTimeS > 0)
    {
        _alarmBlocker.startBlocking(blockingTimeS);
        ESP_LOGI(TAG, "New alarms will be blocked for %lu seconds.", blockingTimeS);
    }
    else
    {
        ESP_LOGI(TAG, "New alarms will not be blocked.");
    }

    return request->reply(200, "application/json", "{\"success\": true}");
}

esp_err_t GeniusGateway::_handleEndBlocking(PsychicRequest *request)
{
    esp_err_t ret = _alarmBlocker.endBlocking();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to end alarm blocking: %s", esp_err_to_name(ret));
        return request->reply(500, "application/json", "{\"success\": false, \"reason\": \"Failed to end alarm blocking.\"}");
    }

    ESP_LOGI(TAG, "Alarm blocking ended by request.");
    return request->reply(200, "application/json", "{\"success\": true}");
}

void GeniusGateway::_emitAlarmState()
{
    /* Prepare event data (payload) */
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["isAlarming"] = _geniusDevices.isAlarming();

    /* Emit event */
    _eventSocket->emitEvent(GATEWAY_EVENT_ALARM, root);
}

// ============================================================================
// ConfigCheckProbe RSSI survey
// ============================================================================

void GeniusGateway::_responderToJson(const config_check_responder_t &r, JsonObject o)
{
    o["sn"] = r.radioModuleSN;
    o["rssi"] = r.rssi;
    o["lineId"] = r.lineId;
    o["status"] = r.status;
    o["groupLine"] = r.groupLine;
}

void GeniusGateway::_respondersToJson(const std::vector<config_check_responder_t> &list, JsonArray arr)
{
    for (const auto &r : list)
        _responderToJson(r, arr.add<JsonObject>());
}

esp_err_t GeniusGateway::_handleStartProbe(PsychicRequest *request)
{
    // Radio must be up to transmit the request and collect the responses.
    if (_cc1101Controller.getRadioState() != CC1101_RADIO_OK)
        return request->reply(409, "application/json", "{\"success\":false,\"reason\":\"Radio not ready.\"}");

    uint32_t sweepId = 0;
    bool started = false;

    xSemaphoreTake(_probeMutex, portMAX_DELAY);
    if (!_probeActive)
    {
        _probeActive = true;
        _probeStopRequested = false;
        _probeResponders.clear();
        sweepId = ++_probeSweepId;
        started = true;
    }
    xSemaphoreGive(_probeMutex);

    if (!started)
        return request->reply(409, "application/json", "{\"success\":false,\"reason\":\"A survey is already running.\"}");

    // Fire the TX sweep; roll the session back if it can't be queued (e.g. radio busy).
    if (_alarmLines.transmitConfigCheckProbe() != ESP_OK)
    {
        xSemaphoreTake(_probeMutex, portMAX_DELAY);
        _probeActive = false;
        xSemaphoreGive(_probeMutex);
        return request->reply(409, "application/json", "{\"success\":false,\"reason\":\"Radio busy transmitting.\"}");
    }

    // Wake the finalize worker: it re-broadcasts and waits out the two-sweep response window, then
    // closes the survey. Total ≈ first TX + one listening window per sweep.
    uint64_t windowMs = (uint64_t)ALARMLINES_CONFIGCHECKPROBE_SWEEP_MS +
                        (uint64_t)CONFIG_CHECK_PROBE_SWEEPS * CONFIG_CHECK_PROBE_COLLECT_MS;
    xTaskNotifyGive(_probeFinalizeTaskHandle);

    // Tell the UI a survey has started so it can show progress.
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["phase"] = "started";
    root["sweepId"] = sweepId;
    root["windowMs"] = windowMs;
    _eventSocket->emitEvent(GATEWAY_EVENT_CONFIG_CHECK_PROBE, root);

    char body[96];
    snprintf(body, sizeof(body), "{\"success\":true,\"sweepId\":%lu,\"windowMs\":%llu}",
             (unsigned long)sweepId, (unsigned long long)windowMs);
    return request->reply(202, "application/json", body);
}

esp_err_t GeniusGateway::_handleStopProbe(PsychicRequest *request)
{
    // Ask the finalize worker to close the survey now instead of waiting out the window. Unlike a
    // cancel this keeps _probeActive set, so _finalizeProbe() runs its normal path (persist the
    // responders heard so far, emit 'done') - it just skips the unreached-marking. The worker's
    // interruptible listen picks up the flag within ~250 ms, so no result is emitted from here.
    xSemaphoreTake(_probeMutex, portMAX_DELAY);
    bool active = _probeActive;
    if (active)
        _probeStopRequested = true;
    xSemaphoreGive(_probeMutex);

    if (active)
        ESP_LOGI(TAG, "ConfigCheckProbe survey stop requested; finalizing early.");

    // Idempotent: stopping when nothing is running still succeeds (there is simply nothing to finalize).
    return request->reply(200, "application/json", "{\"success\":true}");
}

esp_err_t GeniusGateway::_handleCancelProbe(PsychicRequest *request)
{
    uint32_t sweepId = 0;

    // Clearing _probeActive stops response recording immediately, makes the finalize worker's
    // interruptible listen bail within ~250 ms (so no further re-broadcast fires), and makes the
    // subsequent _finalizeProbe() a no-op - so the survey ends without persisting or emitting 'done'.
    xSemaphoreTake(_probeMutex, portMAX_DELAY);
    bool wasActive = _probeActive;
    if (wasActive)
    {
        _probeActive = false;
        _probeResponders.clear();
        sweepId = _probeSweepId;
    }
    xSemaphoreGive(_probeMutex);

    // Idempotent: cancelling when nothing is running is a success (the caller's intent is already met).
    if (wasActive)
    {
        ESP_LOGI(TAG, "ConfigCheckProbe survey #%lu cancelled by request.", (unsigned long)sweepId);

        // Tell the UI the survey was aborted so it can clear its progress/spinner without waiting
        // out the window or expecting a 'done' event.
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["phase"] = "cancelled";
        root["sweepId"] = sweepId;
        _eventSocket->emitEvent(GATEWAY_EVENT_CONFIG_CHECK_PROBE, root);
    }

    return request->reply(200, "application/json", "{\"success\":true}");
}

void GeniusGateway::_recordProbeResponse(const genius_packet_t *details, const cc1101_packet_t *packet)
{
    if (!details || !packet)
        return;

    // Lock-free fast path: skip the mutex entirely when no survey is running (the common case,
    // hit on every received frame). Also avoids touching _probeMutex before it is created.
    if (!_probeActive)
        return;

    int8_t rssi = cc1101_packet_rssi_dbm(packet);
    uint8_t status = (packet->length > DATAPOS_CONFIG_CHECK_PROBE_STATUS)
                         ? packet->data[DATAPOS_CONFIG_CHECK_PROBE_STATUS] : 0;
    uint8_t groupLine = (packet->length > DATAPOS_CONFIG_CHECK_PROBE_GROUPLINE)
                            ? packet->data[DATAPOS_CONFIG_CHECK_PROBE_GROUPLINE] : 0;

    bool isNew = false;
    config_check_responder_t snapshot = {};

    xSemaphoreTake(_probeMutex, portMAX_DELAY);
    if (_probeActive)
    {
        bool found = false;
        for (auto &r : _probeResponders)
        {
            if (r.radioModuleSN == details->origin_id)
            {
                if (rssi > r.rssi) // keep the best link observed across the sweep's repeats
                    r.rssi = rssi;
                r.status = status;
                r.groupLine = groupLine;
                r.lineId = details->line_id;
                found = true;
                break;
            }
        }
        if (!found && _probeResponders.size() < CONFIG_CHECK_PROBE_MAX_RESPONDERS)
        {
            config_check_responder_t r = {};
            r.radioModuleSN = details->origin_id;
            r.lineId = details->line_id;
            r.rssi = rssi;
            r.status = status;
            r.groupLine = groupLine;
            _probeResponders.push_back(r);
            isNew = true;
            snapshot = r;
        }
    }
    xSemaphoreGive(_probeMutex);

    // Surface each newly-heard module live so the UI's progress dialog fills in as answers arrive
    // (not only at finalize). Emitted once per distinct module → bounded to the responder count.
    // Done outside the mutex to keep JSON serialization / socket push off the probe lock.
    if (isNew)
    {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["phase"] = "responder";
        root["sweepId"] = _probeSweepId; // stable during a survey; plain read is fine
        root["known"] = _geniusDevices.isRadioModuleKnown(snapshot.radioModuleSN);
        _responderToJson(snapshot, root["responder"].to<JsonObject>());
        _eventSocket->emitEvent(GATEWAY_EVENT_CONFIG_CHECK_PROBE, root);
    }
}

bool GeniusGateway::_probeListen(TickType_t total)
{
    // Poll in short slices instead of one long vTaskDelay so a cancel (_probeActive → false) or an
    // early stop (_probeStopRequested) wakes us promptly - within ~250 ms - rather than blocking out
    // the full window. Returns false when the wait was cut short by either.
    const TickType_t step = pdMS_TO_TICKS(250);
    for (TickType_t waited = 0; waited < total; waited += step)
    {
        vTaskDelay(step);
        if (!_probeActive || _probeStopRequested)
            return false; // cancelled or stopped mid-window
    }
    return true;
}

void GeniusGateway::_probeFinalizeTask()
{
    ESP_LOGI(TAG, "ConfigCheckProbe finalize task started.");
    const TickType_t listenTicks = pdMS_TO_TICKS(CONFIG_CHECK_PROBE_COLLECT_MS);
    while (1)
    {
        // Woken by _handleStartProbe right after it fired sweep 1. Mirror the Genius-Port
        // "Bahlinger" pattern: listen a full window after each sweep, re-broadcasting between
        // windows to catch nodes that missed or answered late. RX collects for the whole time.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (uint8_t sweep = 1; sweep < CONFIG_CHECK_PROBE_SWEEPS && _probeActive && !_probeStopRequested; sweep++)
        {
            if (!_probeListen(listenTicks))
                break; // survey cancelled or stopped early
            if (_alarmLines.transmitConfigCheckProbe() != ESP_OK)
                ESP_LOGW(TAG, "ConfigCheckProbe re-broadcast (sweep %u) failed to queue.", (unsigned)(sweep + 1));
        }
        if (_probeActive && !_probeStopRequested)
            _probeListen(listenTicks);
        _finalizeProbe(); // cancelled → no-op; stopped early → finalizes without unreached-marking
    }
}

void GeniusGateway::_finalizeProbe()
{
    std::vector<config_check_responder_t> responders;
    uint32_t sweepId = 0;
    bool stoppedEarly = false;

    xSemaphoreTake(_probeMutex, portMAX_DELAY);
    if (!_probeActive)
    {
        _probeStopRequested = false; // cancelled: discard and clear the flag for the next survey
        xSemaphoreGive(_probeMutex);
        return; // already finalized (e.g. cancelled, or a duplicate fire)
    }
    responders.swap(_probeResponders);
    _probeActive = false;
    stoppedEarly = _probeStopRequested;
    _probeStopRequested = false;
    sweepId = _probeSweepId;
    xSemaphoreGive(_probeMutex);

    // Known modules that answered → update; unknown answerers → discovered list for the UI.
    std::vector<config_check_responder_t> discovered;
    std::vector<uint32_t> reached;
    uint32_t knownUpdated = 0;
    time_t now = time(nullptr);

    for (const auto &r : responders)
    {
        if (_geniusDevices.updateRadioModuleRssi(r.radioModuleSN, r.rssi, now))
        {
            knownUpdated++;
            reached.push_back(r.radioModuleSN);
        }
        else
            discovered.push_back(r);
    }

    // Stamp every other known module as "range-tested, no response" (rssi = 0 sentinel) so the UI
    // can tell "out of range this survey" apart from "never tested". Skipped on an early stop: the
    // survey was cut short, so a silent module isn't reliably out of range - we'd write a misleading
    // sentinel. A full survey always runs it, recording that all known modules were probed.
    uint32_t unreached = stoppedEarly ? 0 : _geniusDevices.markRadioModulesUnreached(reached, now);

    if (knownUpdated > 0 || unreached > 0)
        _geniusDevices.persistRssiSurvey(); // single flash write for the whole survey

    xSemaphoreTake(_probeMutex, portMAX_DELAY);
    _probeDiscovered.swap(discovered);
    size_t discoveredCount = _probeDiscovered.size();
    xSemaphoreGive(_probeMutex);

    ESP_LOGI(TAG, "ConfigCheckProbe survey #%lu %s: %u responder(s), %lu reached, %lu unreached, %u discovered.",
             (unsigned long)sweepId, stoppedEarly ? "stopped early" : "done", (unsigned)responders.size(),
             (unsigned long)knownUpdated, (unsigned long)unreached, (unsigned)discoveredCount);

    // Emit the result. The UI refetches /rest/gateway-devices on this event to pick up the
    // persisted RSSI of known devices, and uses "discovered" to offer adding unknown modules.
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["phase"] = "done";
    root["sweepId"] = sweepId;
    root["stopped"] = stoppedEarly; // survey ended early by user request (label "Stopped" vs "Complete")
    root["responderCount"] = (uint32_t)responders.size();
    root["knownUpdated"] = knownUpdated;
    JsonArray all = root["responders"].to<JsonArray>();
    _respondersToJson(responders, all);
    JsonArray disc = root["discovered"].to<JsonArray>();
    xSemaphoreTake(_probeMutex, portMAX_DELAY);
    _respondersToJson(_probeDiscovered, disc);
    xSemaphoreGive(_probeMutex);
    _eventSocket->emitEvent(GATEWAY_EVENT_CONFIG_CHECK_PROBE, root);
}

esp_err_t GeniusGateway::_handleGetDiscovered(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();

    xSemaphoreTake(_probeMutex, portMAX_DELAY);
    root["sweepId"] = _probeSweepId;
    root["active"] = _probeActive;
    JsonArray arr = root["discovered"].to<JsonArray>();
    _respondersToJson(_probeDiscovered, arr);
    xSemaphoreGive(_probeMutex);

    return response.send();
}

esp_err_t GeniusGateway::_genius_analyze_packet_data(uint8_t *packet_data, size_t data_length, genius_packet_t *analyzed_packet)
{
    if (!packet_data)
    {
        ESP_LOGE(TAG, "%s(): Pointer to CC1101 packet data is NULL.", __func__);
        return ESP_ERR_INVALID_ARG;
    }

    if (!analyzed_packet)
    {
        ESP_LOGE(TAG, "%s(): Pointer to analyzed packet struct is NULL.", __func__);
        return ESP_ERR_INVALID_ARG;
    }

    /* Clear analyzed packet */
    memset(analyzed_packet, 0, sizeof(genius_packet_t));

    /* A frame must be long enough to carry the message-type byte before we can classify it. */
    if (data_length <= DATAPOS_MSG_TYPE)
    {
        analyzed_packet->type = HPT_UNKNOWN;
        return ESP_OK;
    }

    /* Determine the Genius packet type from its on-air message-type byte (offset 27), using
     * length as a validator. This mirrors how the genuine radio module routes a received
     * frame: on the type-specific tail byte plus an exact per-type length. Length ALONE is
     * ambiguous - message-type 0x00 is both Alarming (36 B) and the CommissioningProbe
     * Request (28 B), and a 36-byte frame is either Alarming (0x00) or a ConfigCheckProbe response
     * (0x08); classifying 36 B as an alarm can misread a ConfigCheckProbe response as ALARM_STOP. */
    switch (packet_data[DATAPOS_MSG_TYPE])
    {
    case MSGTYPE_COMMISSIONING: // 0x03  (alarm-line commissioning)
        analyzed_packet->type = (data_length == LEN_COMMISSIONING_PACKET) ? HPT_COMMISSIONING : HPT_UNKNOWN;
        break;

    case MSGTYPE_COMMISSIONING_PROBE_RESPONSE: // 0x01  (commissioning-context, carries Req-SN)
        analyzed_packet->type = (data_length == LEN_COMMISSIONING_PROBE_RESPONSE_PACKET) ? HPT_COMMISSIONING_PROBE_RESPONSE : HPT_UNKNOWN;
        break;

    case MSGTYPE_LINE_TEST: // 0x04  (firmware: line_state_processor, event 3)
        if (data_length != LEN_LINE_TEST_PACKET)
            analyzed_packet->type = HPT_UNKNOWN;
        else if (packet_data[DATAPOS_LINE_TEST_START_STOP_FLAG] == 0) // 0x00 = END/STOP of line test
            analyzed_packet->type = HPT_LINE_TEST_STOP;
        else if ((packet_data[DATAPOS_LINE_TEST_START_STOP_FLAG] & 0x04) > 0) // 0x04/0x06 = START
            analyzed_packet->type = HPT_LINE_TEST_START;
        else
            analyzed_packet->type = HPT_UNKNOWN;
        break;

    case MSGTYPE_CONFIG_CHECK_PROBE_REQUEST: // 0x06  (ConfigCheckProbe, wildcard direct-range probe)
        analyzed_packet->type = (data_length == LEN_CONFIG_CHECK_PROBE_REQUEST_PACKET) ? HPT_CONFIG_CHECK_PROBE_REQUEST : HPT_UNKNOWN;
        break;

    case MSGTYPE_CONFIG_CHECK_PROBE_RESPONSE: // 0x08  (36 B - kept out of the alarm branch by the type byte)
        analyzed_packet->type = (data_length == LEN_CONFIG_CHECK_PROBE_RESPONSE_PACKET) ? HPT_CONFIG_CHECK_PROBE_RESPONSE : HPT_UNKNOWN;
        break;

    case MSGTYPE_ALARM_OR_COMMISSIONING_PROBE_REQ: // 0x00  - shared value, split by length
        if (data_length == LEN_ALARM_PACKET) // Alarming
        {
            if (packet_data[DATAPOS_ALARM_ACTIVE_FLAG] == 1)
                analyzed_packet->type = HPT_ALARM_START;
            else if (packet_data[DATAPOS_ALARM_SILENCE_FLAG] == 1)
                analyzed_packet->type = HPT_ALARM_STOP;
            else
                analyzed_packet->type = HPT_UNKNOWN;
        }
        else if (data_length == LEN_COMMISSIONING_PROBE_REQUEST_PACKET) // CommissioningProbe Request
            analyzed_packet->type = HPT_COMMISSIONING_PROBE_REQUEST;
        else
            analyzed_packet->type = HPT_UNKNOWN;
        break;

    default:
        analyzed_packet->type = HPT_UNKNOWN;
        break;
    }

    if (analyzed_packet->type != HPT_UNKNOWN)
    {
        analyzed_packet->origin_id = EXTRACT32(packet_data, DATAPOS_GENERAL_ORIGIN_RADIO_MODULE_ID);
        analyzed_packet->sender_id = EXTRACT32(packet_data, DATAPOS_GENERAL_SENDER_RADIO_MODULE_ID);
        analyzed_packet->line_id = EXTRACT32(packet_data, DATAPOS_GENERAL_LINE_ID);
        analyzed_packet->hops = HOPS_FIRST - packet_data[DATAPOS_GENERAL_HOPS];
    }

    return ESP_OK;
}

void GeniusGateway::_mqttPublishTask()
{
    ESP_LOGI(TAG, "HA publish task started, waiting for MQTT connect notifications.");
    while (1)
    {
        // Block indefinitely until onConnect sends a notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "MQTT connected - publishing all app-specific devices.");
        _geniusDevices.mqttPublishAllDevices(false); // Full republish on connect: broker state unknown after reconnect
        _geniusDevices.mqttPublishSimpleAlarmState();
        ESP_LOGI(TAG, "HA publish complete.");
    }
}

void GeniusGateway::_rx_packets()
{
    cc1101_packet_t packet;
    // Buffer for alarm line names - sized for reasonable line name lengths
    char lineName[64] = {0};

    ESP_LOGI(pcTaskGetName(0), "Started.");

    while (1)
    {
        /* Wait (blocking) until being notified (by ISR), that a packet has been received.
           Note: the parameter 'xClearCountOnExit' is pdTRUE, which has the effect of
           clearing the task's notification value back to 0, making the notification
           value act like a binary semaphore. */
        if (ulTaskNotifyTakeIndexed(RX_TASK_NOTIFICATION_INDEX, pdTRUE, RX_TASK_MAX_WAITING_TICKS) == 1)
        {
            // Temprarily disable RX Monitoring
            _cc1101Controller.disableRXMonitoring();

            // Fetch the packet
            if (cc1101_receive_data(&packet) == ESP_OK)
            {
                // Duplicate detection using optimized XOR hash
                bool isDuplicate = false;
                if (packet.length > 3)
                {
                    // Skip first 3 bytes of packet data: byte 1 is always 0x02 and bytes 2-3 are the
                    // Remaining-TX-Time field, which changes on every repetition of the same logical packet
                    uint32_t currentHash = Utils::xorHash(packet.data + 3, packet.length - 3);

                    if (_hasLastPacketHash && currentHash == _lastPacketHash)
                    {
                        isDuplicate = true;
                        ESP_LOGD(TAG, "Duplicate packet detected (hash: 0x%08X)", currentHash);
                    }
                    else
                    {
                        _lastPacketHash = currentHash;
                        _hasLastPacketHash = true;
                        ESP_LOGV(TAG, "New packet hash: 0x%08X", currentHash);
                    }
                }

                // Only process packet if it's not a duplicate, i.e. repeated packet
                if (!isDuplicate)
                {
                    genius_packet_t packet_details;
                    if (_genius_analyze_packet_data(packet.data, packet.length, &packet_details) == ESP_OK)
                    {
                        if (packet_details.type == HPT_COMMISSIONING)
                        {
                            /* Store new alarm line id */
                            if (_gatewaySettings.isAddAlarmLineFromCommissioningPacketEnabled())
                            {
                                uint32_t newLineID = EXTRACT32(packet.data, DATAPOS_COMISSIONING_NEW_LINE_ID);
                                snprintf(lineName, sizeof(lineName), "Added from received comissioning packet", newLineID);
                                _alarmLines.addAlarmLine(newLineID, String(lineName), ALA_GENIUS_PACKET);
                            }
                        }
                        else if (packet_details.type == HPT_ALARM_START || packet_details.type == HPT_ALARM_STOP)
                        {
                            uint32_t source_id = EXTRACT32_REV(packet.data, DATAPOS_ALARM_SOURCE_SMOKE_ALARM_ID);

                            if (GATEWAY_ID != source_id) // only proceed for alarming/silencing packets NOT originating from Genius Gateway itself
                            {
                                if (packet_details.type == HPT_ALARM_START)
                                {
                                    bool isDetectorKnown = _geniusDevices.isSmokeDetectorKnown(source_id);

                                    bool deviceAdded = false;
                                    if (!isDetectorKnown && _gatewaySettings.isAlertOnUnknownDetectorsEnabled())
                                    {
                                        uint32_t snRM = EXTRACT32(packet.data, DATAPOS_GENERAL_ORIGIN_RADIO_MODULE_ID);
                                        deviceAdded = _geniusDevices.AddGeniusDevice(snRM, source_id);
                                        isDetectorKnown = true; // Now we know the detector, as it was intentionally added
                                    }

                                    /* Set/Reset alarm */
                                    if (isDetectorKnown)
                                    {
                                        const GeniusDevice *dev = _geniusDevices.setAlarm(source_id);
                                        if (dev)
                                        {
                                            if (deviceAdded)
                                                _geniusDevices.mqttPublishAllDevices(); // New device: publish config + state
                                            else
                                                _geniusDevices.mqttPublishDeviceState(source_id); // Known device: publish only this device's state
                                        }
                                    }
                                }
                                else // packet_details.type == HPT_ALARM_SILENCING
                                {
                                    const GeniusDevice *dev = _geniusDevices.resetAlarm(source_id, GAE_BY_SMOKE_DETECTOR);
                                    if (dev)
                                        _geniusDevices.mqttPublishDeviceState(source_id); // Publish only this device's state
                                }

                                /* Emit alarm state to front end */
                                _emitAlarmState(); // TODO: Is this necessary on every single packet???

                                /* Store alarm line id */
                                if (_gatewaySettings.isAddAlarmLineFromAlarmPacketEnabled())
                                {
                                    uint32_t lineID = EXTRACT32(packet.data, DATAPOS_GENERAL_LINE_ID);
                                    snprintf(lineName, sizeof(lineName), "Added from received alarming/silencing packet", lineID);
                                    _alarmLines.addAlarmLine(lineID, String(lineName), ALA_GENIUS_PACKET);
                                }
                            }
                        }
                        else if (packet_details.type == HPT_LINE_TEST_START ||
                                 packet_details.type == HPT_LINE_TEST_STOP)
                        {
                            if (_gatewaySettings.isAddAlarmLineFromLineTestPacketEnabled())
                            {
                                uint32_t lineID = EXTRACT32(packet.data, DATAPOS_GENERAL_LINE_ID);
                                snprintf(lineName, sizeof(lineName), "Added from received line test packet", lineID);
                                _alarmLines.addAlarmLine(lineID, String(lineName), ALA_GENIUS_PACKET);
                            }
                        }
                        else if (packet_details.type == HPT_CONFIG_CHECK_PROBE_RESPONSE)
                        {
                            // Collect the responder into the active ConfigCheckProbe survey (no-op if
                            // no survey is running). RSSI comes from this frame's appended status byte.
                            _recordProbeResponse(&packet_details, &packet);
                        }
                    }
                } // End of !isDuplicate check

                /* Send data to WebSocket logger - log ALL packets including duplicates */
                _wsLogger.logPacket(&packet);
            }

            // Re-enable RX Monitoring
            _cc1101Controller.enableRXMonitoring();
        }
        else
        {
            /* The call to ulTaskNotifyTake() timed out, if ulTaskNotifyTakeIndexed()
             * was called with xTicksToWait set to a value < portMAX_DELAY. */
            vTaskDelay(1);
        }

        /* Check for RX overflow or any orphaned data before returning to receive state,
         * as packet handling might have taken too long to fetch next packet in time. */
        cc1101_check_rx_fifo(true);
    }

    // never reach here
    vTaskDelete(NULL);
}
