/**
 * @file AlarmLinesService.cpp
 * @brief Implementation of alarm line management and RF transmission service
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

#include <AlarmLinesService.h>
#include <cc1101.h>
#include <GeniusGateway.h>
#include <GatewayMqttSettingsService.h>
#include <PsychicMqttClient.h>
#include <GatewayMqttSettingsService.h>
#include <PsychicMqttClient.h>
#include <WiFi.h>
#include <IPUtils.h>

/// Base packet template for alarm line test operations
const uint8_t AlarmLinesService::_packet_base_linetest[] = {
    0x02,
    0x00, 0x00, // Counter
    0x00,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0x00,
    0xFF, 0xFF, 0xFF, 0xFE, // Radio module ID, originator of the packet (0xFFFFFFFE = Gateway)
    0x00,
    0xFF, 0xFF, 0xFF, 0xFE, // Radio module ID, repeater of the packet (0xFFFFFFFE = Gateway)
    0x00, 0x00, 0x00, 0x00, // Alarm line ID (#18-#21)
    0x0F,                   // Hops (#22)
    0x00,                   // Packet sequence number (#23)
    0x48,
    0x00,
    0x66,
    0x04,
    0x00}; // 0x06: Start line test / 0x00 Stop line test (#28)

/// Base packet template for fire alarm operations
const uint8_t AlarmLinesService::_packet_base_firealarm[] = {
    0x02,
    0x00, 0x00, // Counter
    0x00,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0x00,
    0xFF, 0xFF, 0xFF, 0xFE, // Radio module ID, originator of the packet (0xFFFFFFFE = Gateway)
    0x00,
    0xFF, 0xFF, 0xFF, 0xFE, // Radio module ID, repeater of the packet (0xFFFFFFFE = Gateway)
    0x00, 0x00, 0x00, 0x00, // Alarm line ID (#18-#21)
    0x0F,                   // Hops (#22)
    0x00,                   // Packet sequence number (#23)
    0x48,

    0x00,
    0x00,
    0x00,
    0x00, // Fire alarm start/active flag (#28)
    0x00,
    0x00, // Fire alarm end/inactive flag (#30)
    0x00,
    0xFF, 0xFF, 0xFF, 0xFE // SN of smoke detector sensing smoke (0xFFFFFFFE = Gateway)
};

/**
 * @brief Constructor implementation
 * @details Initializes all member variables and sets up framework dependencies.
 * HTTP endpoints and file system persistence are configured but not started.
 * RF transmission components are initialized to safe defaults.
 *
 * @note The begin() method must be called after construction to start services.
 */
AlarmLinesService::AlarmLinesService(ESP32SvelteKit *sveltekit, PsychicMqttClient *mqttClient, CC1101Controller *cc1101Ctrl, GatewayMqttSettingsService *mqttSettingsService) : _sveltekit(sveltekit),
                                                                                                                                                 _server(sveltekit->getServer()),
                                                                                                                                                 _securityManager(sveltekit->getSecurityManager()),
                                                                                                                                                 _featureService(sveltekit->getFeatureService()),
                                                                                                                                                 _eventSocket(sveltekit->getSocket()),
                                                                                                                                                 _httpEndpoint(AlarmLines::read,
                                                                                                                                                               AlarmLines::update,
                                                                                                                                                               this,
                                                                                                                                                               sveltekit->getServer(),
                                                                                                                                                               ALARMLINES_SERVICE_PATH,
                                                                                                                                                               sveltekit->getSecurityManager(),
                                                                                                                                                               AuthenticationPredicates::IS_ADMIN),
                                                                                                                                                 _fsPersistence(AlarmLines::read,
                                                                                                                                                                AlarmLines::update,
                                                                                                                                                                this,
                                                                                                                                                                sveltekit->getFS(),
                                                                                                                                                                ALARMLINES_FILE),
                                                                                                                                                 _cc1101Ctrl(cc1101Ctrl),
                                                                                                                                                 _txTaskHandle(nullptr),
                                                                                                                                                 _txSemaphore(nullptr),
                                                                                                                                                 _timerHandle(nullptr),
                                                                                                                                                 _isTransmitting(false),
                                                                                                                                                 _transmissionTimeElapsed(0),
                                                                                                                                                 _lastTXLoop(0),
                                                                                                                                                 _txRepeat(0),
                                                                                                                                                 _mqttClient(mqttClient),
                                                                                                                                                 _mqttSettingsService(mqttSettingsService),
                                                                                                                                                 _haService(sveltekit->getHAService()),
                                                                                                                                                 _lastActionLineId(0),
                                                                                                                                                 _lastActionType(String()),
                                                                                                                                                 _txDataLength(0),
                                                                                                                                                 _packetCntStep(0.0f),
                                                                                                                                                 _currentPacketCnt(0.0f),
                                                                                                                                                 _txPeriodUs(0),
                                                                                                                                                 _packet_sequence_number(ALARMLINES_NVS_SEQ_DEFAULT)
{
}

// ============================================================================
// Public Methods - Lifecycle
// ============================================================================

void AlarmLinesService::begin()
{
    _httpEndpoint.begin();
    _fsPersistence.readFromFS(); // Load the persisted packet sequence number from NVS
    if (loadPcktSeqNum() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load packet sequence number from NVS. Using default value: %u.", ALARMLINES_NVS_SEQ_DEFAULT);
        _packet_sequence_number = ALARMLINES_NVS_SEQ_DEFAULT;
    }

#if FT_ENABLED(FT_ALLOW_BROADCAST)
    _featureService->addFeature("allow_broadcast", true);
    // Ensure broadcast alarm line is always present when feature is enabled
    if (!_alarmLineExists(ALARMLINES_ID_BROADCAST))
        addAlarmLine(ALARMLINES_ID_BROADCAST, "Broadcast", ALA_MANUAL, true);
#else
    _featureService->addFeature("allow_broadcast", false);
    // Ensure broadcast alarm line is removed when feature is disabled
    if (_alarmLineExists(ALARMLINES_ID_BROADCAST))
        _removeAlarmLine(ALARMLINES_ID_BROADCAST);
#endif

    // Initialize the semaphore for TX task synchronization
    _txSemaphore = xSemaphoreCreateBinary();
    if (_txSemaphore == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create TX semaphore.");
        return;
    }

    ESP_LOGI(TAG, "TX semaphore created (%p).", _txSemaphore);

    // Create TX task for handling RF transmission operations
    BaseType_t xReturned;
    xReturned = xTaskCreatePinnedToCore(
        _txLoopImpl,
        ALARMLINES_TX_TASK_NAME,
        ALARMLINES_TX_TASK_STACK_SIZE,
        this,
        ALARMLINES_TX_TASK_PRIORITY,
        &_txTaskHandle,
        ALARMLINES_TX_TASK_CORE_AFFINITY);

    if (xReturned != pdPASS)
    {
        ESP_LOGE(TAG, "TX task creation failed.");
        return;
    }

    ESP_LOGI(TAG, "TX task created (%p).", _txTaskHandle);

    // Configure ESP timer for accurate transmission intervals
    const esp_timer_create_args_t timer_args = {
        .callback = _onTimerImpl,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = ALARMLINES_TX_PERIOD_TIMER_NAME};

    esp_err_t ret = esp_timer_create(&timer_args, &_timerHandle);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Failed to create TX timer.");

    // Register REST endpoint for triggering alarm line actions
    _server->on(ALARMLINES_PATH_ACTIONS,
                HTTP_POST,
                _securityManager->wrapCallback(std::bind(&AlarmLinesService::_performAction, this, std::placeholders::_1, std::placeholders::_2),
                                               AuthenticationPredicates::IS_ADMIN));

    // Register WebSocket events for real-time notifications
    _eventSocket->registerEvent(ALARMLINES_EVENT_NEW_LINE);
    _eventSocket->registerEvent(ALARMLINES_EVENT_ACTION_STARTED);
    _eventSocket->registerEvent(ALARMLINES_EVENT_ACTION_FINISHED);

    // Initialize cached MQTT settings
    _updateMqttSettingsCache();

    /* Sync sub-devices and republish MQTT when alarm lines change */
    this->addUpdateHandler([this](const String &originId)
                          {
                              _syncAlarmLineSubDevices();
                          },
                          false);

    /* Update cache when alarm MQTT settings change */
    if (_mqttSettingsService != nullptr)
    {
        _mqttSettingsService->addUpdateHandler([this](const String &originId)
                                               {
                                                   this->_updateMqttSettingsCache();
                                               },
                                               false);
    }

    // Register sub-devices for all lines already loaded from FS
    _syncAlarmLineSubDevices();
}

// ============================================================================
// Private Methods - Core Loops and Timing
// ============================================================================

void AlarmLinesService::_onTimer()
{
    gpio_set_level(static_cast<gpio_num_t>(GPIO_TEST2), 0); // Temporary for testing

    // Signal TX task to proceed with next packet transmission
    xTaskNotifyGiveIndexed(_txTaskHandle, ALARMLINES_TX_TASK_NOTIFICATION_INDEX);
}

void AlarmLinesService::_txLoop()
{
    ESP_LOGI(pcTaskGetName(0), "Started.");

    while (1)
    {
        // Wait for transmission request via semaphore
        if (xSemaphoreTake(_txSemaphore, portMAX_DELAY) == pdTRUE)
        {
            _isTransmitting = true;

            // Temporarily disable RX monitoring to avoid interference
            _cc1101Ctrl->disableRXMonitoring();

            // Initialize transmission monitoring
            bool timedOut = false;
            _transmissionTimeElapsed = 0;
            _lastTXLoop = millis();

            ESP_LOGI(pcTaskGetName(0), "Starting transmission: packets: %lu, period: %.3f ms, first packet count: %u (0x%04x), packet count step: %u.",
                     _txRepeat,
                     _txPeriodUs / 1000.0,
                     static_cast<uint16_t>(_currentPacketCnt),
                     static_cast<uint16_t>(_currentPacketCnt),
                     static_cast<uint16_t>(_packetCntStep));

            for (int i = 1; i <= _txRepeat; i++)
            {
                // Check for transmission timeout
                uint32_t currentMillis = millis();
                uint32_t _transmissionTimeElapsed = currentMillis - _lastTXLoop;
                if (_transmissionTimeElapsed >= ALARMLINES_TX_TIMEOUT_MS)
                {
                    ESP_LOGW(TAG, "Transmission timeout reached (%lu ms). Cancelling running transmission.", ALARMLINES_TX_TIMEOUT_MS);
                    timedOut = true;
                    break;
                }

                // GPIO timing markers for debug/analysis
                gpio_set_level(static_cast<gpio_num_t>(GPIO_TEST1), 1); // Temporary for testing
                gpio_set_level(static_cast<gpio_num_t>(GPIO_TEST2), 1); // Temporary for testing

                // Configure timer for next iteration (except for last packet)
                if (i < _txRepeat) // Don't (re)start the timer for the last iteration
                {
                    esp_err_t ret = esp_timer_start_once(_timerHandle, _txPeriodUs);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to start TX timer: %s.", esp_err_to_name(ret));
                        break;
                    }
                }

                // Update packet count in transmission buffer (2 bytes, little-endian)
                *(uint16_t *)&_txBuffer[1] = (uint16_t)_currentPacketCnt;

                // Execute RF packet transmission
                if (cc1101_send_data(_txBuffer, _txDataLength) != ESP_OK)
                    ESP_LOGE(pcTaskGetName(0), "Failed to send packet @ iteration %d.", i);

                // Progress packet count for fire alarm sequences
                _currentPacketCnt = std::max(_currentPacketCnt - _packetCntStep, 0.0f);

                _lastTXLoop = millis();

                gpio_set_level(static_cast<gpio_num_t>(GPIO_TEST1), 0); // Temporary for testing

                // Wait for timer notification before next packet (except last iteration)
                if (i < _txRepeat) // Don't wait after the last iteration
                {
                    if (ulTaskNotifyTakeIndexed(ALARMLINES_TX_TASK_NOTIFICATION_INDEX, pdTRUE, ALARMLINES_TX_TASK_ITERATION_MAX_WAITING_TICKS) != 1)
                    {
                        ESP_LOGE(TAG, "Failed to receive timer notification @ iteration %d.", i);
                        break;
                    }
                }
            }

            _isTransmitting = false;

            // Notify clients of transmission completion
            _emitActionFinishedEvent(timedOut);

            // Restore RF controller to receive state
            cc1101_set_rx_state();

            // Re-enable RX monitoring after transmission completion
            _cc1101Ctrl->enableRXMonitoring();

            ESP_LOGI(pcTaskGetName(0), "Transmission finished.");
        }
        else
        {
            // Handle semaphore timeout (should not occur with portMAX_DELAY)
            vTaskDelay(1);
        }
    }

    // Task cleanup (should never reach here)
    vTaskDelete(NULL);
}

// ============================================================================
// Private Methods - Action Handling
// ============================================================================

esp_err_t AlarmLinesService::_performAction(PsychicRequest *request, JsonVariant &json)
{
    if (!json.is<JsonObject>())
        return request->reply(400, "application/json", "{\"success\": false, \"reason\": \"Invalid JSON\"}");

    JsonObject jsonObject = json.as<JsonObject>();
    if (!jsonObject["lineId"].is<uint32_t>())
        return request->reply(400, "application/json", "{\"success\": false, \"reason\": \"Invalid line ID.\"}");

    if (!jsonObject["action"].is<String>())
        return request->reply(400, "application/json", "{\"success\": false, \"reason\": \"Action missing or of wrong type.\"}");

    uint32_t lineIdHost = jsonObject["lineId"].as<uint32_t>();
    String action = jsonObject["action"].as<String>();

    // Delegate to programmatic trigger which sets MQTT state and starts transmission
    esp_err_t ret = _triggerAction(lineIdHost, action);
    if (ret != ESP_OK)
    {
        if (ret == ESP_ERR_INVALID_STATE)
            return request->reply(503, "application/json", "{\"success\": false, \"reason\": \"Previous action is still performed.\"}");
        return request->reply(500, "application/json", "{\"success\": false, \"reason\": \"Failed to trigger action.\"}");
    }

    return request->reply(200, "application/json", "{\"success\": true}");
}

// ============================================================================
// Private Methods - Alarm Line Management
// ============================================================================

bool AlarmLinesService::_alarmLineExists(uint32_t id)
{
    bool found = false;

    beginTransaction();
    for (const auto &line : _state.lines)
    {
        if (line.id == id)
        {
            found = true;
            break;
        }
    }
    endTransaction();

    return found;
}

// ============================================================================
// Public Methods - Alarm Line Management
// ============================================================================

esp_err_t AlarmLinesService::addAlarmLine(uint32_t id, String name, alarm_line_acquisition_t acquisition, bool toFront)
{
    if (id == ALARMLINES_ID_NONE)
    {
        ESP_LOGE(TAG, "Cannot add a line with ID %lu. This ID is reserved.", id);
        return ESP_ERR_INVALID_ARG;
    }

    if (name.length() > ALARMLINES_NAME_MAX_LENGTH)
    {
        ESP_LOGE(TAG, "Alarm line name is too long. Maximum length is %d.", ALARMLINES_NAME_MAX_LENGTH);
        return ESP_ERR_INVALID_ARG;
    }

    if (acquisition <= ALA_MIN || acquisition >= ALA_MAX)
    {
        ESP_LOGE(TAG, "Invalid acquisition type provided: %d.", acquisition);
        return ESP_ERR_INVALID_ARG;
    }

    if (_alarmLineExists(id))
    {
        ESP_LOGV(TAG, "Alarm line with ID %lu already exists.", id);
        return ESP_ERR_INVALID_STATE;
    }

    genius_alarm_line_t newLine;
    newLine.id = id;
    newLine.name = name;
    newLine.created = time(nullptr);
    newLine.acquisition = acquisition;
    newLine.published = false;

    beginTransaction();
    if (toFront)
        _state.lines.insert(_state.lines.begin(), newLine);
    else
        _state.lines.push_back(newLine);
    endTransaction();

    ESP_LOGI(TAG, "Added alarm line '%s' with id %lu", newLine.name.c_str(), newLine.id);

    callUpdateHandlers(ALARMLINES_ORIGIN_ID);

    if (acquisition == ALA_GENIUS_PACKET) // Alarm line has been added from a genius packet
        _emitNewAlarmLineEvent(id);

    return ESP_OK;
}

// ============================================================================
// Private Methods - Event Emission
// ============================================================================

void AlarmLinesService::_emitNewAlarmLineEvent(uint32_t id)
{
    JsonDocument jsonDoc;
    JsonObject jsonRoot = jsonDoc.to<JsonObject>();
    jsonRoot["newAlarmLineId"] = id;
    _eventSocket->emitEvent(ALARMLINES_EVENT_NEW_LINE, jsonRoot);
}

void AlarmLinesService::_emitActionStartedEvent(uint32_t lineIdHostOrder, const String &action)
{
    JsonDocument jsonDoc;
    JsonObject jsonRoot = jsonDoc.to<JsonObject>();
    jsonRoot["lineId"] = lineIdHostOrder;
    jsonRoot["action"] = action;
    _eventSocket->emitEvent(ALARMLINES_EVENT_ACTION_STARTED, jsonRoot);
}

void AlarmLinesService::_emitActionFinishedEvent(bool timedOut)
{
    JsonDocument jsonDoc;
    JsonObject jsonRoot = jsonDoc.to<JsonObject>();
    jsonRoot["timedOut"] = timedOut;
    _eventSocket->emitEvent(ALARMLINES_EVENT_ACTION_FINISHED, jsonRoot);

    // Publish MQTT state for the last action
    _publishLastActionState(timedOut);
}

// ============================================================================
// Private Methods - Action Triggering
// ============================================================================

esp_err_t AlarmLinesService::_triggerAction(uint32_t lineIdHostOrder, const String &action)
{
    if (_isTransmitting)
    {
        ESP_LOGW(TAG, "Previous action is still performed. Wait until it finishes to start another action.");
        return ESP_ERR_INVALID_STATE;
    }

    // Find the alarm line name for logging
    String lineName;
    beginTransaction();
    for (const auto &line : _state.lines)
    {
        if (line.id == lineIdHostOrder)
        {
            lineName = line.name;
            break;
        }
    }
    endTransaction();

    ESP_LOGI(TAG, "Action triggered: '%s' for Alarm Line ID=%lu", action.c_str(), lineIdHostOrder);

    if (lineName.isEmpty())
    {
        ESP_LOGW(TAG, "Alarm Line ID=%lu not registered in Genius Gateway, ignoring.", lineIdHostOrder);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Alarm Line ID=%lu matched: '%s'", lineIdHostOrder, lineName.c_str());

    uint32_t lineId = htonl(lineIdHostOrder);

    // Configure transmission parameters based on requested action
    if (action == "line-test-start" || action == "line-test-stop")
    {
        _txPeriodUs = ALARMLINES_TX_PERIOD_LINETEST_US;
        _txRepeat = ALARMLINES_TX_NUM_REPEAT_LINETEST;
        _currentPacketCnt = ALARMLINES_LINETEST_FIRST_PCKTCNT;
        _packetCntStep = ALARMLINES_LINETEST_PCKTCNT_STEP;

        size_t datalen = std::min(sizeof(_packet_base_linetest), sizeof(_txBuffer));
        memcpy(_txBuffer, _packet_base_linetest, datalen);

        if (action == "line-test-start")
            _txBuffer[28] = 0x06; // Set line test start flag
        else
            _txBuffer[28] = 0x00; // Set line test stop flag

        _txDataLength = datalen;
    }
    else if (action == "fire-alarm-start" || action == "fire-alarm-stop")
    {
        _txPeriodUs = ALARMLINES_TX_PERIOD_FIREALARM_US;
        _txRepeat = ALARMLINES_TX_NUM_REPEAT_FIREALARM;
        _currentPacketCnt = ALARMLINES_FIREALARM_FIRST_PCKTCNT;
        _packetCntStep = ALARMLINES_FIREALARM_PCKTCNT_STEP;

        size_t datalen = std::min(sizeof(_packet_base_firealarm), sizeof(_txBuffer));
        memcpy(_txBuffer, _packet_base_firealarm, datalen);
        if (action == "fire-alarm-start")
            _txBuffer[28] = 0x01; // Set fire alarm start flag
        else
            _txBuffer[30] = 0x01; // Set fire alarm end flag

        _txDataLength = datalen;
    }
    else
    {
        ESP_LOGE(TAG, "Unknown action '%s'.", action.c_str());
        return ESP_ERR_INVALID_ARG;
    }

    // Common packet preparation
    memcpy(&_txBuffer[18], &lineId, sizeof(lineId)); // Set line id
    _txBuffer[23] = incPcktSeqNum();                 // Increment and persist sequence number

    // Record last action context for MQTT result publishing
    _lastActionLineId = lineIdHostOrder;
    _lastActionType = action;

    ESP_LOGI(TAG, "Action '%s' triggered successfully for line ID %lu ('%s').", action.c_str(), lineIdHostOrder, lineName.c_str());

    // Publish "running" state BEFORE giving the semaphore: once the TX task
    // starts the RF transmission, it interferes with WiFi enough that
    // esp_mqtt_client_enqueue blocks until the TX completes (~3 s). If we
    // publish after the give, this call can land AFTER the tx-task's
    // end-of-TX "Nothing" publish and leave the retained MQTT state stuck on
    // the running value forever.
    String state;
    if (action == "line-test-start")
        state = "Line Test Start";
    else if (action == "line-test-stop")
        state = "Line Test Stop";
    else if (action == "fire-alarm-start")
        state = "Fire Alarm Start";
    else if (action == "fire-alarm-stop")
        state = "Fire Alarm Stop";
    else
        state = action;

    _publishAlarmLineTransmissionState(lineIdHostOrder, state);

    // Notify any connected Web UI clients so they can show the running
    // spinner even when the trigger came from HA / MQTT / another tab.
    _emitActionStartedEvent(lineIdHostOrder, action);

    // Notify the pending TX task to start the transmission
    if (xSemaphoreGive(_txSemaphore) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to give semaphore.");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// ============================================================================
// Private Methods - HA Sub-device Management
// ============================================================================

void AlarmLinesService::_addAlarmLineSubDevice(uint32_t lineId, const String &lineName)
{
    if (_haDevices.count(lineId))
        return; // already registered

    String deviceId = "genius-alarmline-" + String(lineId);

    HADeviceIdentity identity;
    identity.id = deviceId;
    identity.name = "Alarm Line '" + lineName + "'";
    identity.manufacturer = "Genius Gateway Project";
    identity.model = "Genius Plus X Alarm Line";
    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
        identity.configurationUrl = "http://" + localIP.toString() + "/gateway/alarm-lines";

    auto dev = std::make_unique<HADevice>(_haService, std::move(identity));

    // Register transmission state sensor first so it publishes before buttons.
    // HA evaluates button availability immediately when it receives button config,
    // so the retained sensor state must already be on the broker.
    if (!_txStates.count(lineId))
        _txStates[lineId] = "Nothing";

    auto sensor = std::make_unique<HASensor>(_haService, "transmission",
        [this, lineId]()
        {
            auto it = _txStates.find(lineId);
            return it != _txStates.end() ? it->second : String("Nothing");
        },
        [](JsonObject &c) { c["icon"] = "mdi:radio-tower"; });
    sensor->setName("Current Transmission");
    HASensor *rawSensor = dev->registerDiagnostic(std::move(sensor));

    // Register 4 buttons: line-test start/stop, fire-alarm start/stop
    struct BtnDef
    {
        const char *id;
        const char *name;
        const char *icon;
        const char *action;
    };
    static const BtnDef btns[] = {
        {"linetest-start",  "Start Line Test",  "mdi:map-marker",     "line-test-start"},
        {"linetest-stop",   "Stop Line Test",   "mdi:map-marker-off", "line-test-stop"},
        {"firealarm-start", "Start Fire Alarm", "mdi:fire",           "fire-alarm-start"},
        {"firealarm-stop",  "Stop Fire Alarm",  "mdi:fire-off",       "fire-alarm-stop"},
    };

    for (const auto &b : btns)
    {
        String action(b.action); // capture by value
        auto btn = std::make_unique<HAButton>(_haService, b.id,
            [this, lineId, action]() { _triggerAction(lineId, action); });
        btn->setName(b.name).setIcon(b.icon);

        // Buttons are shown as unavailable while a transmission is active.
        // The availability topic is the sensor state topic on this sub-device.
        auto *rawBtn = dev->registerControl(std::move(btn));
        rawBtn->setExtraConfig([this, deviceId](JsonObject &c)
        {
            String txTopic = _haService->getBaseTopic() + "/" + deviceId + "/transmission/state";
            JsonObject availObj = c["availability"].to<JsonArray>().add<JsonObject>();
            availObj["topic"] = txTopic;
            availObj["value_template"] = "{% if value == 'Nothing' %}online{% else %}offline{% endif %}";
            c["availability_mode"] = "all";
        });
    }

    HADevice *rawDev = _haService->addSubDevice(std::move(dev));
    _haDevices[lineId] = {rawDev, rawSensor};

    ESP_LOGI(TAG, "Registered HA sub-device for alarm line ID %lu ('%s')", lineId, lineName.c_str());
}

void AlarmLinesService::_removeAlarmLineSubDevice(uint32_t lineId)
{
    _txStates.erase(lineId);
    _haDevices.erase(lineId);

    String deviceId = "genius-alarmline-" + String(lineId);
    _haService->removeSubDevice(deviceId);

    ESP_LOGI(TAG, "Removed HA sub-device for alarm line ID %lu", lineId);
}

void AlarmLinesService::_syncAlarmLineSubDevices()
{
    beginTransaction();

    // Remove sub-devices for lines deleted via HTTP (populated by AlarmLines::update)
    for (uint32_t id : _state.deletedLineIds)
        _removeAlarmLineSubDevice(id);
    _state.deletedLineIds.clear();

    // Add or refresh sub-devices for current lines
    for (auto &line : _state.lines)
    {
        if (!_haDevices.count(line.id))
        {
            // New line — add sub-device
            _addAlarmLineSubDevice(line.id, line.name);
            line.published = true;
        }
        else if (!line.published)
        {
            // Line renamed or otherwise needs refresh — replace sub-device
            _removeAlarmLineSubDevice(line.id);
            _addAlarmLineSubDevice(line.id, line.name);
            line.published = true;
        }
    }

    endTransaction();
}

// ============================================================================
// Private Methods - MQTT State Publishing
// ============================================================================

esp_err_t AlarmLinesService::_publishAlarmLineTransmissionState(uint32_t lineId, const String &state)
{
    _txStates[lineId] = state; // Update cached state (used by sensor ValueReader)

    auto it = _haDevices.find(lineId);
    if (it == _haDevices.end() || it->second.txSensor == nullptr)
    {
        ESP_LOGW(TAG, "_publishAlarmLineTransmissionState: no HA device for line ID %lu", lineId);
        return ESP_ERR_NOT_FOUND;
    }

    if (!_haService->isReady())
    {
        ESP_LOGW(TAG, "_publishAlarmLineTransmissionState: HA not ready (line ID %lu, state '%s')", lineId, state.c_str());
        return ESP_ERR_INVALID_STATE;
    }

    it->second.txSensor->publishState();
    ESP_LOGD(TAG, "_publishAlarmLineTransmissionState: published '%s' for line ID %lu", state.c_str(), lineId);
    return ESP_OK;
}

void AlarmLinesService::_publishLastActionState(bool timedOut)
{
    if (_lastActionLineId == 0 || _lastActionType.length() == 0)
        return;

    _publishAlarmLineTransmissionState(_lastActionLineId, "Nothing");

    ESP_LOGV(TAG, "Published completion state (Nothing) for line ID '%lu', action '%s', timedOut=%d",
             _lastActionLineId, _lastActionType.c_str(), timedOut);
}

// ============================================================================
// Private Methods - Alarm Line Removal
// ============================================================================

esp_err_t AlarmLinesService::_removeAlarmLine(uint32_t id)
{
    if (id == ALARMLINES_ID_NONE)
    {
        ESP_LOGE(TAG, "Cannot remove line: %lu is no valid alarm line ID.", id);
        return ESP_ERR_INVALID_ARG;
    }

    bool removed = false;

    beginTransaction();
    for (auto line = _state.lines.begin(); line != _state.lines.end(); ++line)
    {
        if (line->id == id)
        {
            _state.lines.erase(line);
            removed = true;
            break;
        }
    }
    endTransaction();

    if (!removed)
    {
        ESP_LOGW(TAG, "Alarm line with ID %lu does not exist.", id);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Removed alarm line with id %lu", id);

    // Unpublish HA entities for this alarm line
    _removeAlarmLineSubDevice(id);

    callUpdateHandlers(ALARMLINES_ORIGIN_ID);

    return ESP_OK;
}

// ============================================================================
// Public Methods - Packet Sequence Number Management
// ============================================================================

uint8_t AlarmLinesService::incPcktSeqNum()
{
    // Increment the sequence number (with rollover at 255)
    _packet_sequence_number = (_packet_sequence_number + 1) % 256;

    // Persist to NVS
    esp_err_t err = savePcktSeqNum();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to persist incremented packet sequence number: %s", esp_err_to_name(err));
        // Continue anyway, the in-memory value is still incremented
    }

    return _packet_sequence_number;
}

esp_err_t AlarmLinesService::savePcktSeqNum()
{
    // Persist to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(ALARMLINES_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK)
    {
        err = nvs_set_u8(nvs_handle, ALARMLINES_NVS_SEQ_KEY, _packet_sequence_number);
        if (err == ESP_OK)
        {
            err = nvs_commit(nvs_handle);
            if (err == ESP_OK)
            {
                ESP_LOGV(TAG, "Persisted packet sequence number %u to NVS.", _packet_sequence_number);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to commit packet sequence number to NVS: %s", esp_err_to_name(err));
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to set packet sequence number in NVS: %s", esp_err_to_name(err));
        }
        nvs_close(nvs_handle);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for packet sequence number: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t AlarmLinesService::loadPcktSeqNum()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(ALARMLINES_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK)
    {
        err = nvs_get_u8(nvs_handle, ALARMLINES_NVS_SEQ_KEY, &_packet_sequence_number);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Loaded packet sequence number from NVS: %u", _packet_sequence_number);
        }
        else if (err == ESP_ERR_NVS_NOT_FOUND) // First time - initialize to default value
        {
            ESP_LOGI(TAG, "Packet sequence number not found in NVS, creating and initializing it to %u.", ALARMLINES_NVS_SEQ_DEFAULT);
            _packet_sequence_number = ALARMLINES_NVS_SEQ_DEFAULT;
            esp_err_t save_err = savePcktSeqNum();
            if (save_err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to save initial packet sequence number: %s", esp_err_to_name(save_err));
                err = save_err;
            }
            else
            {
                err = ESP_OK;
            }
        }
        else // Any other error
        {
            ESP_LOGE(TAG, "Failed to get packet sequence number from NVS: %s.", esp_err_to_name(err));
        }

        nvs_close(nvs_handle);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND) // NVS namespace not found, create it
    {
        _packet_sequence_number = ALARMLINES_NVS_SEQ_DEFAULT;
        ESP_LOGI(TAG, "NVS namespace '%s' not found, creating and initializing packet sequence number to %u.", ALARMLINES_NVS_NAMESPACE, ALARMLINES_NVS_SEQ_DEFAULT);
        esp_err_t save_err = savePcktSeqNum();
        if (save_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create NVS namespace and save initial packet sequence number: %s", esp_err_to_name(save_err));
            err = save_err;
        }
        else
        {
            err = ESP_OK;
        }
    }
    else // Any other error
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for reading packet sequence number: %s.", esp_err_to_name(err));
    }

    return err;
}

// ============================================================================
// Private Methods - Settings Management
// ============================================================================

void AlarmLinesService::_updateMqttSettingsCache()
{
    if (_mqttSettingsService != nullptr)
    {
        _cachedMqttSettings = _mqttSettingsService->getSettingsCopy();
        ESP_LOGV(TAG, "Updated cached alarm MQTT settings (alarmEnabled: %d, topic: %s)",
                 _cachedMqttSettings.alarmEnabled,
                 _cachedMqttSettings.alarmTopic.c_str());
    }
}
