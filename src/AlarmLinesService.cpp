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
                                                                                                                                                 _lastActionLineId(0),
                                                                                                                                                 _lastActionType(String()),
                                                                                                                                                 _txDataLength(0),
                                                                                                                                                 _packetCntStep(0.0f),
                                                                                                                                                 _currentPacketCnt(0.0f),
                                                                                                                                                 _txPeriodUs(0),
                                                                                                                                                 _packet_sequence_number(ALARMLINES_NVS_SEQ_DEFAULT),
                                                                                                                                                 _lastMqttPrefix(String())
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
    _eventSocket->registerEvent(ALARMLINES_EVENT_ACTION_FINISHED);

    // Initialize cached MQTT settings
    _updateMqttSettingsCache();

    /* Republish MQTT when alarm lines change */
    this->addUpdateHandler([this](const String &originId)
                          {
                              _mqttPublishAllAlarmLines();
                          },
                          false);

    /* Update cache and republish MQTT when MQTT settings change */
    if (_mqttSettingsService != nullptr)
    {
        _mqttSettingsService->addUpdateHandler([this](const String &originId)
                                               {
                                                   this->_updateMqttSettingsCache();
                                                   this->mqttRegisterTopicsAndPublishAlarmLines();
                                               },
                                               false);
    }
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

    // Publish to MQTT if connected
    if (_mqttClient != nullptr && _mqttSettingsService != nullptr)
    {
        if (_cachedMqttSettings.HAIntegrationEnabled && !_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        {
            // Trigger update handler which will handle full republish
        }
    }

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
    String lineName = "Unknown";
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

    ESP_LOGI(TAG, "MQTT Command received: Action='%s' for Alarm Line ID=%lu Name='%s'", 
             action.c_str(), lineIdHostOrder, lineName.c_str());

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

    // Notify the pending TX task to start the transmission
    if (xSemaphoreGive(_txSemaphore) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to give semaphore.");
        return ESP_FAIL;
    }

    // Publish running state over MQTT if available
    if (_mqttClient != nullptr && _mqttSettingsService != nullptr)
    {
        if (_cachedMqttSettings.HAIntegrationEnabled && !_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        {
            // Map action to human-readable transmission state value
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
        }
    }

    ESP_LOGI(TAG, "Action '%s' triggered successfully for line ID %lu ('%s').", action.c_str(), lineIdHostOrder, lineName.c_str());
    return ESP_OK;
}

// ============================================================================
// Public Methods - MQTT
// ============================================================================

/**
 * @brief Registers MQTT command topics and publishes discovery config for all alarm lines
 * 
 * This is the main entry point for MQTT integration. It:
 * 1. Subscribes to MQTT command topics for line test and fire alarm actions
 * 2. Handles topic prefix changes by unsubscribing from old topics
 * 3. Publishes Home Assistant discovery configuration for all alarm lines
 * 4. Publishes initial state for all alarm lines
 * 
 * Must be called after MQTT connection is established. Safe to call multiple times
 * (e.g., on reconnect or settings change) - will handle cleanup of old subscriptions.
 * 
 * Command topics use wildcards to handle multiple alarm lines:
 * - homeassistant/genius-alarmline/+/linetest/command
 * - homeassistant/genius-alarmline/+/firealarm/command
 * 
 * Payloads are JSON: {"action": "start"} or {"action": "stop"}
 */
void AlarmLinesService::mqttRegisterTopicsAndPublishAlarmLines()
{
    if (_mqttClient == nullptr || _mqttSettingsService == nullptr)
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled || _cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        return;

    // Get discovery prefix for topic subscriptions
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;

    // Unsubscribe from old topics if prefix has changed
    if (!_lastMqttPrefix.isEmpty() && _lastMqttPrefix != discoveryPrefix)
    {
        String oldLinetestTopic = _lastMqttPrefix + ALARMLINES_MQTT_TOPIC_LINE_TEST;
        String oldFirealarmTopic = _lastMqttPrefix + ALARMLINES_MQTT_TOPIC_FIRE_ALARM;
        _mqttClient->unsubscribe(oldLinetestTopic.c_str());
        _mqttClient->unsubscribe(oldFirealarmTopic.c_str());
        ESP_LOGI(TAG, "Unsubscribed from old MQTT command topics with prefix: %s", _lastMqttPrefix.c_str());
    }

    // Subscribe to command topics (wildcard) for Line Test actions
    String linetestCmdTopic = discoveryPrefix + ALARMLINES_MQTT_TOPIC_LINE_TEST;
    _mqttClient->onTopic(linetestCmdTopic.c_str(), 0, [this](char *topic, char *payload, int retain, int qos, bool dup)
                        {
                           String t = String(topic);
                           // Extract id from topic: .../genius-alarmline/<id>/linetest/command
                           int idx = t.indexOf(ALARMLINES_MQTT_TOPIC_PREFIX);
                           if (idx < 0)
                               return;
                           String remainder = t.substring(idx + strlen(ALARMLINES_MQTT_TOPIC_PREFIX));
                           int slash = remainder.indexOf('/');
                           if (slash < 0)
                               return;
                           String idStr = remainder.substring(0, slash);
                           uint32_t id = (uint32_t)atol(idStr.c_str());

                           // Parse payload as JSON and map to action
                           String pl = payload ? String(payload) : String();
                           String action = "line-test-start"; // default
                           if (pl.length() > 0)
                           {
                               JsonDocument doc;
                               DeserializationError err = deserializeJson(doc, pl);
                               if (!err && doc["action"].is<String>())
                               {
                                   String a = doc["action"].as<String>();
                                   if (a == "start")
                                       action = "line-test-start";
                                   else if (a == "stop")
                                       action = "line-test-stop";
                               }
                           }

                           _triggerAction(id, action); });

    // Subscribe to command topics (wildcard) for Fire Alarm actions
    String firealarmCmdTopic = discoveryPrefix + ALARMLINES_MQTT_TOPIC_FIRE_ALARM;
    _mqttClient->onTopic(firealarmCmdTopic.c_str(), 0, [this](char *topic, char *payload, int retain, int qos, bool dup)
                        {
                           String t = String(topic);
                           int idx = t.indexOf(ALARMLINES_MQTT_TOPIC_PREFIX);
                           if (idx < 0)
                               return;
                           String remainder = t.substring(idx + strlen(ALARMLINES_MQTT_TOPIC_PREFIX));
                           int slash = remainder.indexOf('/');
                           if (slash < 0)
                               return;
                           String idStr = remainder.substring(0, slash);
                           uint32_t id = (uint32_t)atol(idStr.c_str());

                           String pl = payload ? String(payload) : String();
                           String action = "fire-alarm-start";
                           if (pl.length() > 0)
                           {
                                JsonDocument doc;
                               DeserializationError err = deserializeJson(doc, pl);
                               if (!err && doc["action"].is<String>())
                               {
                                   String a = doc["action"].as<String>();
                                   if (a == "start")
                                       action = "fire-alarm-start";
                                   else if (a == "stop")
                                       action = "fire-alarm-stop";
                               }
                           }

                           _triggerAction(id, action); });

    // Store current prefix for future cleanup
    _lastMqttPrefix = discoveryPrefix;

    // Publish discovery and state for all lines
    _mqttPublishAllAlarmLines();
}

// ============================================================================
// Private Methods - MQTT Publishing
// ============================================================================

/**
 * @brief Publishes all alarm lines to MQTT/Home Assistant
 * 
 * First processes any pending deleted alarm lines (unpublishing them), then
 * iterates over all current alarm lines and publishes both configuration and state.
 * Only marks lines as published if both operations succeed (atomic semantics).
 * Uses a single transaction for the entire operation for efficiency.
 * 
 * This function synchronizes MQTT state with the current alarm line list.
 * 
 * This is an internal helper called by mqttRegisterTopicsAndPublishAlarmLines()
 * and the update handler when alarm lines change.
 * 
 * No transaction management - creates its own transaction internally.
 * 
 * @param onlyUnpublished If true, only publishes lines that haven't been published yet
 */
void AlarmLinesService::_mqttPublishAllAlarmLines(bool onlyUnpublished)
{
    beginTransaction();

    // First, unpublish any deleted alarm lines
    if (!_state.deletedLineIds.empty())
    {
        ESP_LOGI(TAG, "Processing %d deleted alarm line(s) for MQTT unpublishing.", _state.deletedLineIds.size());

        for (uint32_t id : _state.deletedLineIds)
        {
            _mqttUnpublishAlarmLine(id);
        }

        // Clear the list after processing
        _state.deletedLineIds.clear();
    }

    // Then publish config and state for current lines
    for (auto &line : _state.lines)
    {
        if (!onlyUnpublished || !line.published)
        {
            // IMPORTANT: Publish state BEFORE config!
            // Buttons reference the transmission state in their availability condition.
            // HA evaluates availability immediately when receiving config, so the state
            // message must already exist on the broker (retained) to avoid marking entities as unavailable.
            esp_err_t resState = _publishAlarmLineTransmissionState(line.id);
            
            // Pass useTransaction=false since we're already holding the lock
            esp_err_t resConfig = _mqttPublishAlarmLineConfig(line, false);
            
            // Safe approach: only mark as published if both config and state were successfully published
            if (resConfig == ESP_OK && resState == ESP_OK)
            {
                line.published = true;
            }
        }
    }

    endTransaction();
}

/**
 * @brief Publishes MQTT discovery configuration for a single alarm line
 * 
 * Creates and publishes Home Assistant MQTT discovery messages for all entities
 * associated with an alarm line (buttons, sensors). This only needs to be called
 * once or when the configuration changes.
 * 
 * @param line Reference to the alarm line
 * @param useTransaction If true, wraps access in transaction. Set false if caller holds lock.
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if MQTT not ready, ESP_ERR_INVALID_ARG for missing parameters, ESP_FAIL on publish error
 */
esp_err_t AlarmLinesService::_mqttPublishAlarmLineConfig(const genius_alarm_line_t &line, bool useTransaction)
{
    if (_mqttClient == nullptr || _mqttSettingsService == nullptr)
        return ESP_ERR_INVALID_STATE;

    if (!_cachedMqttSettings.HAIntegrationEnabled)
        return ESP_ERR_INVALID_STATE;

    if (_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
    {
        ESP_LOGW(TAG, "Home Assistant MQTT discovery prefix is empty. Cannot publish alarm line topics.");
        return ESP_ERR_INVALID_ARG;
    }

    if (useTransaction)
        beginTransaction();

    // Alle Publishing-Funktionen in saubere Module aufgeteilt
    esp_err_t result = ESP_OK;
    result |= _publishAlarmLineButtons(line);   // Alle 4 Buttons
    result |= _publishTransmissionSensor(line); // Transmission State Sensor

    if (useTransaction)
        endTransaction();

    ESP_LOGV(TAG, "Published MQTT discovery config for alarm line ID %lu.", line.id);
    return result == ESP_OK ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Unpublishes (removes) an alarm line from Home Assistant MQTT
 * 
 * Sends empty retained messages to all discovery config topics for the alarm line,
 * which signals Home Assistant to remove the entities. This is called when an alarm
 * line is deleted from the system.
 * 
 * Unpublishes all 5 entities:
 * - 2 Line Test buttons (start/stop)
 * - 2 Fire Alarm buttons (start/stop)  
 * - 1 Transmission state sensor
 * 
 * No transaction management - caller is responsible.
 * 
 * @param lineId Alarm line ID to unpublish
 */
void AlarmLinesService::_mqttUnpublishAlarmLine(uint32_t lineId)
{
    if (_mqttClient == nullptr || _mqttSettingsService == nullptr)
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled || _cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        return;

    String idStr = String(lineId);
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;

    // Unpublish all 5 entities by sending empty retained payloads
    String cfgTopics[] = {
        discoveryPrefix + "button/genius-alarmline-" + idStr + "-linetest/config",
        discoveryPrefix + "button/genius-alarmline-" + idStr + "-linetest-stop/config",
        discoveryPrefix + "sensor/genius-alarmline-" + idStr + "-transmission/config",
        discoveryPrefix + "button/genius-alarmline-" + idStr + "-firealarm/config",
        discoveryPrefix + "button/genius-alarmline-" + idStr + "-firealarm-stop/config"};

    for (const auto &topic : cfgTopics)
    {
        _mqttClient->publish(topic.c_str(), 0, true, "");
    }

    ESP_LOGI(TAG, "Unpublished MQTT entities for alarm line ID %lu.", lineId);
}

/**
 * @brief Publishes the completion state for the last triggered action
 * 
 * Called when an action (line test or fire alarm) completes or times out.
 * Resets the transmission state sensor to "Nothing" to indicate the alarm line
 * is idle and buttons are available again.
 * 
 * This is an internal helper called by _emitActionFinishedEvent().
 * Uses the cached _lastActionLineId and _lastActionType context.
 * 
 * No transaction management - caller is responsible.
 * 
 * @param timedOut True if the action timed out, false if completed normally
 */
void AlarmLinesService::_publishLastActionState(bool timedOut)
{
    if (_mqttClient == nullptr || _mqttSettingsService == nullptr)
        return;

    if (_lastActionLineId == 0 || _lastActionType.length() == 0)
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled || _cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        return;

    // Reset transmission status to Nothing
    _publishAlarmLineTransmissionState(_lastActionLineId, "Nothing");

    ESP_LOGV(TAG, "Published completion state (Nothing) for line ID '%lu', action '%s', timedOut=%d", _lastActionLineId, _lastActionType.c_str(), timedOut);
}

/**
 * @brief Internal function for publishing transmission state for a specific alarm line
 * 
 * Sends the current transmission state to the MQTT state topic.
 * No transaction management - caller is responsible.
 * 
 * @param lineId Alarm line ID
 * @param state State string (defaults to "Nothing"). Can be "Line Test", "Fire Alarm", etc.
 * @return ESP_OK on successful publish, ESP_ERR_INVALID_STATE if MQTT not ready, ESP_FAIL on publish error
 */
esp_err_t AlarmLinesService::_publishAlarmLineTransmissionState(uint32_t lineId, const String &state)
{
    if (_mqttClient == nullptr || _mqttSettingsService == nullptr)
        return ESP_ERR_INVALID_STATE;

    if (!_cachedMqttSettings.HAIntegrationEnabled || _cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        return ESP_ERR_INVALID_STATE;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String transmissionTopic = discoveryPrefix + "genius-alarmline/" + String(lineId) + "/transmission/state";

    JsonDocument stateDoc;
    stateDoc["state"] = state;
    String payload;
    serializeJson(stateDoc, payload);
    
    int result = _mqttClient->publish(transmissionTopic.c_str(), 0, true, payload.c_str());
    if (result == -1)
    {
        ESP_LOGE(TAG, "Failed to publish transmission state for line ID %lu", lineId);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// ============================================================================
// Private Methods - MQTT Publishing Helpers
// ============================================================================

/**
 * @brief Internal function for publishing all button configurations
 * 
 * Creates and publishes all 4 button entities for an alarm line:
 * - Start/Stop Line Test
 * - Start/Stop Fire Alarm
 * No transaction management - caller is responsible.
 * 
 * @param line Const reference to the alarm line
 * @return ESP_OK if all buttons published successfully, ESP_FAIL otherwise
 */
esp_err_t AlarmLinesService::_publishAlarmLineButtons(const genius_alarm_line_t &line)
{
    esp_err_t result = ESP_OK;
    
    // Line Test Start Button
    esp_err_t res1 = _publishButton(line, {
        .idSuffix = "linetest",
        .name = "Start Line Test",
        .uniqueIdSuffix = "linetest_start",
        .action = "start",
        .icon = "mdi:map-marker",
        .commandTopicSuffix = "linetest"
    });
    if (res1 != ESP_OK) result = ESP_FAIL;
    
    // Line Test Stop Button
    esp_err_t res2 = _publishButton(line, {
        .idSuffix = "linetest-stop",
        .name = "Stop Line Test",
        .uniqueIdSuffix = "linetest_stop",
        .action = "stop",
        .icon = "mdi:map-marker-off",
        .commandTopicSuffix = "linetest"
    });
    if (res2 != ESP_OK) result = ESP_FAIL;
    
    // Fire Alarm Start Button
    esp_err_t res3 = _publishButton(line, {
        .idSuffix = "firealarm",
        .name = "Start Fire Alarm",
        .uniqueIdSuffix = "firealarm",
        .action = "start",
        .icon = "mdi:fire",
        .commandTopicSuffix = "firealarm"
    });
    if (res3 != ESP_OK) result = ESP_FAIL;
    
    // Fire Alarm Stop Button
    esp_err_t res4 = _publishButton(line, {
        .idSuffix = "firealarm-stop",
        .name = "Stop Fire Alarm",
        .uniqueIdSuffix = "firealarm_stop",
        .action = "stop",
        .icon = "mdi:fire-off",
        .commandTopicSuffix = "firealarm"
    });
    if (res4 != ESP_OK) result = ESP_FAIL;
    
    return result;
}

/**
 * @brief Internal function for publishing transmission sensor configuration
 * 
 * Creates and publishes the transmission status sensor for an alarm line.
 * No transaction management - caller is responsible.
 * 
 * @param line Const reference to the alarm line
 * @return ESP_OK on successful publish, ESP_FAIL on error
 */
esp_err_t AlarmLinesService::_publishTransmissionSensor(const genius_alarm_line_t &line)
{
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String idStr = String(line.id);
    String baseTopic = discoveryPrefix + "genius-alarmline/" + idStr;

    String stateCfgTopic = discoveryPrefix + "sensor/genius-alarmline-" + idStr + "-transmission/config";
    JsonDocument statecfg;
    statecfg["~"] = baseTopic;
    statecfg["name"] = "Current Transmission";
    statecfg["unique_id"] = "genius-alarmline_" + idStr + "_transmission";
    statecfg["state_topic"] = "~/transmission/state";
    statecfg["value_template"] = "{{value_json.state}}";
    statecfg["icon"] = "mdi:radio-tower";
    statecfg["entity_category"] = "diagnostic";

    _addDeviceInfo(statecfg, line);

    String statePayload;
    serializeJson(statecfg, statePayload);
    
    int result = _mqttClient->publish(stateCfgTopic.c_str(), 0, true, statePayload.c_str());
    if (result == -1)
    {
        ESP_LOGE(TAG, "Failed to publish transmission sensor for line ID %lu", line.id);
        return ESP_FAIL;
    }
    
    ESP_LOGV(TAG, "Published transmission sensor for line ID %lu", line.id);
    return ESP_OK;
}

/**
 * @brief Internal function for publishing a single button entity
 * 
 * Generic helper that creates and publishes Home Assistant button discovery messages.
 * Adds availability and device info automatically.
 * No transaction management - caller is responsible.
 * 
 * @param line Const reference to the alarm line
 * @param config Button configuration (name, icon, action, etc.)
 * @return ESP_OK on successful publish, ESP_FAIL on error
 */
esp_err_t AlarmLinesService::_publishButton(const genius_alarm_line_t &line, const AlarmLineButtonConfig &config)
{
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String idStr = String(line.id);
    String baseTopic = discoveryPrefix + "genius-alarmline/" + idStr;
    
    String cfgTopic = discoveryPrefix + "button/genius-alarmline-" + idStr + "-" + config.idSuffix + "/config";
    JsonDocument cfg;
    cfg["~"] = baseTopic + "/" + config.commandTopicSuffix;
    cfg["name"] = config.name;
    cfg["unique_id"] = "genius-alarmline_" + idStr + "_" + config.uniqueIdSuffix;
    cfg["command_topic"] = baseTopic + "/" + config.commandTopicSuffix + "/command";
    cfg["payload_press"] = "{\"action\":\"" + String(config.action) + "\"}";
    cfg["icon"] = config.icon;
    
    _addAvailabilityAndDevice(cfg, line);
    
    String payload;
    serializeJson(cfg, payload);
    
    int result = _mqttClient->publish(cfgTopic.c_str(), 0, true, payload.c_str());
    if (result == -1)
    {
        ESP_LOGE(TAG, "Failed to publish button '%s' for line ID %lu", config.name, line.id);
        return ESP_FAIL;
    }
    
    ESP_LOGV(TAG, "Published button '%s' for line ID %lu", config.name, line.id);
    return ESP_OK;
}

/**
 * @brief Helper to add availability and device info to config document
 * 
 * Adds availability condition (based on transmission state) and device info
 * to a Home Assistant discovery configuration document.
 * 
 * @param doc JsonDocument to modify
 * @param line Const reference to the alarm line
 */
void AlarmLinesService::_addAvailabilityAndDevice(JsonDocument &doc, const genius_alarm_line_t &line)
{
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String idStr = String(line.id);
    String baseTopic = discoveryPrefix + "genius-alarmline/" + idStr;

    JsonArray avail = doc["availability"].to<JsonArray>();
    JsonObject availObj = avail.add<JsonObject>();
    availObj["topic"] = baseTopic + "/transmission/state";
    availObj["value_template"] = "{% if value_json.state == 'Nothing' %}online{% else %}offline{% endif %}";
    doc["availability_mode"] = "all";

    _addDeviceInfo(doc, line);
}

/**
 * @brief Helper to add device info to config document
 * 
 * Adds Home Assistant device information to a discovery configuration document.
 * Includes configuration URL if a valid IP address is available.
 * 
 * @param doc JsonDocument to modify
 * @param line Const reference to the alarm line
 */
void AlarmLinesService::_addDeviceInfo(JsonDocument &doc, const genius_alarm_line_t &line)
{
    String idStr = String(line.id);
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = "genius-alarmline-" + idStr;
    device["name"] = "Alarm Line '" + line.name + "'";
    device["manufacturer"] = "Genius Gateway Project";
    device["model"] = "Genius Plus X Alarm Line";
    device["via_device"] = "genius-gateway-" + SettingValue::getUniqueId();
    
    // Get the current IP address and only add configuration_url if we have a valid IP
    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
    {
        device["configuration_url"] = "http://" + localIP.toString() + "/gateway/alarm-lines";
    }
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

    // Unpublish Home Assistant MQTT entities for this alarm line
    if (_mqttClient != nullptr && _mqttSettingsService != nullptr)
    {
        if (_cachedMqttSettings.HAIntegrationEnabled && !_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        {
            _mqttUnpublishAlarmLine(id);
        }
    }

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

/**
 * @brief Updates the local cache of MQTT settings
 *
 * Loads current MQTT settings from GatewayMqttSettingsService and stores them
 * in cache for faster access without repeated service calls. Called on initialization
 * and when MQTT settings change.
 */
void AlarmLinesService::_updateMqttSettingsCache()
{
    if (_mqttSettingsService != nullptr)
    {
        _cachedMqttSettings = _mqttSettingsService->getSettingsCopy();
        ESP_LOGV(TAG, "Updated cached MQTT settings (enabled: %d, prefix: %s)", 
                 _cachedMqttSettings.HAIntegrationEnabled, 
                 _cachedMqttSettings.HAMQTTDiscoveryPrefix.c_str());
    }
}
