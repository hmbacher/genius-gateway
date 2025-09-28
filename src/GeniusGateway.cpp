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
                                                          _gatewayDevices(sveltekit),
                                                          _alarmLines(sveltekit, &this->_cc1101Controller),
                                                          _gatewaySettings(sveltekit),
                                                          _gatewayMqttSettingsService(sveltekit),
                                                          _mqttClient(sveltekit->getMqttClient()),
                                                          _wsLogger(sveltekit),
                                                          _visualizerSettingsService(sveltekit),
                                                          _cc1101Controller(sveltekit),
                                                          _alarmBlocker(sveltekit),
                                                          _eventSocket(sveltekit->getSocket()),
                                                          _featureService(sveltekit->getFeatureService()),
                                                          _lastPacketHash(0),
                                                          _hasLastPacketHash(false)
{
}

void GeniusGateway::begin()
{
    /* TEMPORARY: Configure helper GPIO to measure packet handlig time
     * from GDO0 interrupt to full packet read */
    gpio_config_t io_conf1 = {
        .pin_bit_mask = (1ULL << GPIO_TEST1 | 1ULL << GPIO_TEST2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf1);
    gpio_set_level(static_cast<gpio_num_t>(GPIO_TEST1), 0);
    gpio_set_level(static_cast<gpio_num_t>(GPIO_TEST2), 0);
    /* END TEMPORARY */

    /* Create packet handling task */
    BaseType_t xReturned;
    xReturned = xTaskCreatePinnedToCore(
        this->_rx_packetsImpl,
        RX_TASK_NAME,
        RX_TASK_STACK_SIZE,
        this,
        RX_TASK_PRIORITY,
        &GeniusGateway::xRxTaskHandle,
        RX_TASK_CORE_AFFINITY);

    if (xReturned == pdPASS)
    {
        ESP_LOGI(TAG, "RX task created (%p).", GeniusGateway::xRxTaskHandle);

        // Initialize CC1101
        esp_err_t ret = cc1101_init(nofifyReceivedPacket);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "CC1101 could not be set up.");
        else
            ESP_LOGI(TAG, "CC1101 set up successfully.");
    }
    else
    {
        ESP_LOGE(TAG, "RX task creation failed.");
    }

    /* Initialize Gateway Devices Service */
    _gatewayDevices.begin();
    /* Initialize Alarm Lines Service */
    _alarmLines.begin();
    /* Initialize Gateway Settings Service */
    _gatewaySettings.begin();
    /* Initialize Gateway MQTT Settings Service */
    _gatewayMqttSettingsService.begin();
    /* Initialize WS Logger */
    _wsLogger.begin();
    /* Initialize Packet Vizualizer Settings */
    _visualizerSettingsService.begin();

#if FT_ENABLED(FT_CC1101_CONTROLLER)
    _featureService->addFeature("cc1101_controller", true);
    /* Initialize CC1101Controller */
    _cc1101Controller.begin();
    _cc1101Controller.enableRXMonitoring();
#else
    _featureService->addFeature("cc1101_controller", false);
#endif

    /* Perform a full publish (all devices and states), if MQTT client connects. */
    _mqttClient->onConnect([this](bool /*sessionPresent*/)
                           { this->_mqttPublishDevices(false, true); });

    /* Configure update handler for when the smoke detector devices change.
     * Only updates the MQTT state if the change did not originate from a
     * device addition over received alarm packets or alarm state change. */
    _gatewayDevices.addUpdateHandler([&](const String &originId)
                                     { if (originId != GENIUS_DEVICE_ADDED_FROM_PACKET &&
                                           originId != ALARM_STATE_CHANGE)
                                        _mqttPublishDevices(); },
                                     false);

    /* Configure update handler for when the MQTT settings change:
     * Perform a full publish (all devices and states), if settings change. */
    _gatewayMqttSettingsService.addUpdateHandler([&](const String &originId)
                                                 { _mqttPublishDevices(); },
                                                 false);

    _eventSocket->registerEvent(GATEWAY_EVENT_ALARM);

    /* Initialize Alarm Blocking Service */
    _alarmBlocker.begin();

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

    if (_gatewayDevices.resetAllAlarms())
    {
        _mqttPublishDevices(true); // Re-Publish all silenced devices' state
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
    root["isAlarming"] = _gatewayDevices.isAlarming();

    /* Emit event */
    _eventSocket->emitEvent(GATEWAY_EVENT_ALARM, root);
}

void GeniusGateway::_mqttPublishDevices(bool onlyState, bool forceAll)
{
    if (!_mqttClient->connected())
    {
        return;
    }

    GatewayMqttSettings mqttSettings = _gatewayMqttSettingsService.getSettingsCopy(); // Explicit deep copy for thread safety

    // Get optimized MQTT data - only the minimal properties needed for publishing,
    // thread-safe and performance optimized
    std::vector<DeviceMqttData> devicesMqttData = _gatewayDevices.getDevicesMqttData();
    if (devicesMqttData.empty())
    {
        ESP_LOGV(TAG, "No pending devices, skipping MQTT publish.");
        return;
    }

    /* Publish Home Assistant compatible topics */
    if (mqttSettings.haMQTTEnabled)
    {
        if (mqttSettings.haMQTTTopicPrefix.isEmpty())
        {
            ESP_LOGW(TAG, "Home Assistant MQTT topic prefix is empty. Cannot publish config topic.");
        }
        else
        {
            for (const auto &deviceData : devicesMqttData) // Now thread safe and lightweight
            {
                /* Publish config topic for device discovery */
                if (!onlyState)
                {
                    String configTopic = mqttSettings.haMQTTTopicPrefix + deviceData.smokeDetectorSN + "/config";

                    JsonDocument config_jsonDoc;
                    config_jsonDoc["~"] = mqttSettings.haMQTTTopicPrefix + deviceData.smokeDetectorSN;
                    config_jsonDoc["name"] = "Genius Plus X";
                    config_jsonDoc["unique_id"] = deviceData.smokeDetectorSN;
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
                    dev_jsonObj["identifiers"] = deviceData.smokeDetectorSN;
                    dev_jsonObj["manufacturer"] = "Hekatron Vertriebs GmbH";
                    dev_jsonObj["model"] = "Genius Plus X";
                    dev_jsonObj["name"] = "Rauchmelder";

                    dev_jsonObj["serial_number"] = deviceData.smokeDetectorSN;
                    dev_jsonObj["suggested_area"] = deviceData.location;

                    String config_payload;
                    serializeJson(config_jsonDoc, config_payload);
                    _mqttClient->publish(configTopic.c_str(), 0, true, config_payload.c_str());
                }

                /* Pubish state topic */
                String stateTopic = mqttSettings.haMQTTTopicPrefix + deviceData.smokeDetectorSN + "/state";

                JsonDocument state_jsonDoc;
                state_jsonDoc["state"] = deviceData.isAlarming ? "ON" : "OFF";

                String payload;
                serializeJson(state_jsonDoc, payload);
                _mqttClient->publish(stateTopic.c_str(), 0, true, payload.c_str());

                // Set device as published
                _gatewayDevices.setPublished(deviceData.smokeDetectorSN);
            }
        }
    }

    /* Publish generic alarming topic */
    if (mqttSettings.alarmEnabled)
    {
        if (mqttSettings.alarmTopic.isEmpty())
        {
            ESP_LOGW(TAG, "Alarm MQTT topic is empty. Cannot publish alarming state.");
        }
        else
        {
            JsonDocument alarming_jsonDoc;
            bool isAlarming = _gatewayDevices.isAlarming();
            alarming_jsonDoc["isAlarming"] = isAlarming;
            alarming_jsonDoc["numAlarmingDevices"] = _gatewayDevices.numAlarmingDevices();

            String payload;
            serializeJson(alarming_jsonDoc, payload);
            _mqttClient->publish(mqttSettings.alarmTopic.c_str(), 0, true, payload.c_str());
        }
    }
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

    /* Determine type of Genius packet */
    switch (data_length)
    {

    case LEN_COMMISSIONING_PACKET:
        analyzed_packet->type = HPT_COMMISSIONING;
        break;
    case LEN_DISCOVERY_REQUEST_PACKET:
        analyzed_packet->type = HPT_DISCOVERY_REQUEST;
        break;
    case LEN_DISCOVERY_RESPONSE_PACKET:
        analyzed_packet->type = HPT_DISCOVERY_RESPONSE;
        break;
    case LEN_ALARM_PACKET:
        if (packet_data[DATAPOS_ALARM_ACTIVE_FLAG] == 1)
            analyzed_packet->type = HPT_ALARM_START;
        else if (packet_data[DATAPOS_ALARM_SILENCE_FLAG] == 1)
            analyzed_packet->type = HPT_ALARM_STOP;
        else
            analyzed_packet->type = HPT_UNKNOWN;
        break;
    case LEN_LINE_TEST_PACKET:
        if (packet_data[DATAPOS_LINE_TEST_START_STOP_FLAG] == 0) // 0 indicates END/STOP of line test
            analyzed_packet->type = HPT_LINE_TEST_STOP;
        else if ((packet_data[DATAPOS_LINE_TEST_START_STOP_FLAG] & 0x04) > 0) // 0x04 and 0x06 are known indicators for START of line test (0x04 includes 0x06)
            analyzed_packet->type = HPT_LINE_TEST_START;
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
            gpio_set_level((gpio_num_t)GPIO_TEST1, 1); // Temporary: Measuring execution time

            // Temprarily disable RX Monitoring
            _cc1101Controller.disableRXMonitoring();

            // Fetch the packet
            if (cc1101_receive_data(&packet) == ESP_OK)
            {
                // Duplicate detection using optimized XOR hash
                bool isDuplicate = false;
                if (packet.length > 3)
                {                    
                    // Skip first 3 bytes of packet data as first byte is always 0x02 and
                    // bytes 2-3 are some kind of a varying packet counter
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
                                    bool isDetectorKnown = _gatewayDevices.isSmokeDetectorKnown(source_id);

                                    bool deviceAdded = false;
                                    if (!isDetectorKnown && _gatewaySettings.isAlertOnUnknownDetectorsEnabled())
                                    {
                                        uint32_t snRM = EXTRACT32(packet.data, DATAPOS_GENERAL_ORIGIN_RADIO_MODULE_ID);
                                        deviceAdded = _gatewayDevices.AddGeniusDevice(snRM, source_id);
                                        isDetectorKnown = true; // Now we know the detector, as it was intentionally added
                                    }

                                    /* Set/Reset alarm */
                                    if (isDetectorKnown)
                                    {
                                        const GeniusDevice *dev = _gatewayDevices.setAlarm(source_id);
                                        if (dev)
                                            _mqttPublishDevices(!deviceAdded);
                                    }
                                }
                                else // packet_details.type == HPT_ALARM_SILENCING
                                {
                                    const GeniusDevice *dev = _gatewayDevices.resetAlarm(source_id, GAE_BY_SMOKE_DETECTOR);
                                    if (dev)
                                        _mqttPublishDevices(true);
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
                    }
                } // End of !isDuplicate check

                /* Send data to WebSocket logger - log ALL packets including duplicates */
                gpio_set_level(static_cast<gpio_num_t>(GPIO_TEST2), 1); // Temporary: Measuring execution time
                _wsLogger.logPacket(&packet);
                gpio_set_level(static_cast<gpio_num_t>(GPIO_TEST2), 0); // Temporary: Measuring execution time
            }

            // Re-enable RX Monitoring
            _cc1101Controller.enableRXMonitoring();

            gpio_set_level(static_cast<gpio_num_t>(GPIO_TEST1), 0); // Temporary: Measuring execution time
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
