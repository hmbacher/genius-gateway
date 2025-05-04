#include <GeniusGateway.h>

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Just for temporary testing purposes
    gpio_set_level((gpio_num_t)GPIO_TEST1, 1);

    // Notify the waiting (blocked) RX task that a packet is ready to be read from RX FIFO
    vTaskNotifyGiveIndexedFromISR(GeniusGateway::xRxTaskHandle,
                                  RX_TASK_NOTIFICATION_INDEX,
                                  &xHigherPriorityTaskWoken);

    /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch should be
       performed to ensure the interrupt returns directly to the highest priority task. */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

TaskHandle_t GeniusGateway::xRxTaskHandle = NULL;

GeniusGateway::GeniusGateway(ESP32SvelteKit *sveltekit) : _gatewayDevices(sveltekit),
                                                          _gatewaySettings(sveltekit),
                                                          _gatewayMqttSettingsService(sveltekit),
                                                          _alarmState(sveltekit),
                                                          _mqttClient(sveltekit->getMqttClient()),
                                                          _webSocketLogger(sveltekit),
                                                          _visualizerSettingsService(sveltekit)
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
    gpio_set_level(GPIO_TEST1, 0);
    gpio_set_level(GPIO_TEST2, 0);
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
        esp_err_t ret = cc1101_init(gpio_isr_handler);
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
    /* Initialize Gateway Settings Service */
    _gatewaySettings.begin();
    /* Initialize Gateway MQTT Settings Service */
    _gatewayMqttSettingsService.begin();
    /* Initialize Alarm State Service */
    _alarmState.begin();
    /* Initialize WS Logger */
    _webSocketLogger.begin();
    /* Initialize Packet Vizualizer Settings */
    _visualizerSettingsService.begin();

    /* Configure MQTT callback */
    _mqttClient->onConnect(std::bind(&GeniusGateway::mqttConfig, this));

    /* Configure update handler for when the smoke detector devices change */
    _gatewayDevices.addUpdateHandler([&](const String &originId)
                                     { mqttConfig(); },
                                     false);

    /* Configure update handler for when the MQTT settings change */
    _gatewayMqttSettingsService.addUpdateHandler([&](const String &originId)
                                                 { mqttConfig(); },
                                                 false);

    /* Configure update handler for when the alarm state changes */
    _alarmState.addUpdateHandler([&](const String &originId)
                                 { mqttConfig(); },
                                 false);
}

void GeniusGateway::mqttConfig()
{
    if (!_mqttClient->connected())
    {
        return;
    }

    GatewayMqttSettings &mqttSettings = _gatewayMqttSettingsService.getSettings();

    for (auto &device : _gatewayDevices.getDevices())
    {
        /* Publish config topic */
        String configTopic = mqttSettings.mqttPath + device.smokeDetector.sn + "/config";

        JsonDocument config_jsonDoc;
        config_jsonDoc["~"] = mqttSettings.mqttPath + device.smokeDetector.sn;
        config_jsonDoc["name"] = "Genius Plus X";
        config_jsonDoc["unique_id"] = device.smokeDetector.sn;
        config_jsonDoc["device_class"] = "smoke";
        config_jsonDoc["state_topic"] = "~/state";
        config_jsonDoc["schema"] = "json";
        config_jsonDoc["value_template"] = "{{value_json.state}}";
        config_jsonDoc["entity_picture"] = "http://genius-gateway/hekatron-genius-plus-x.png";
        JsonObject dev_jsonObj = config_jsonDoc["device"].to<JsonObject>();
        dev_jsonObj["identifiers"] = device.smokeDetector.sn;
        dev_jsonObj["manufacturer"] = "Hekatron Vertriebs GmbH";
        dev_jsonObj["model"] = "Genius Plus X";
        dev_jsonObj["name"] = "Rauchmelder";
        dev_jsonObj["serial_number"] = device.smokeDetector.sn;
        dev_jsonObj["suggested_area"] = device.location;
                
        String config_payload;
        serializeJson(config_jsonDoc, config_payload);
        _mqttClient->publish(configTopic.c_str(), 0, true, config_payload.c_str());

        /* Pubish state topic */
        String stateTopic = mqttSettings.mqttPath + device.smokeDetector.sn + "/state";

        JsonDocument state_jsonDoc;
        state_jsonDoc["state"] = _alarmState.isAlarming(device.smokeDetector.sn) ? "ON" : "OFF";

        String payload;
        serializeJson(state_jsonDoc, payload);
        _mqttClient->publish(stateTopic.c_str(), 0, true, payload.c_str());
    }
}

void GeniusGateway::mqttPublishAlarmState()
{
    if (!_mqttClient->connected())
    {
        return;
    }

    GatewayMqttSettings &mqttSettings = _gatewayMqttSettingsService.getSettings();

    for (auto &device : _gatewayDevices.getDevices())
    {
        String pubTopic = mqttSettings.mqttPath + device.smokeDetector.sn + "/state";

        /* Pubish state topic */
        JsonDocument state_jsonDoc;
        state_jsonDoc["state"] = _alarmState.isAlarming(device.smokeDetector.sn) ? "ON" : "OFF";

        String payload;
        serializeJson(state_jsonDoc, payload);
        _mqttClient->publish(pubTopic.c_str(), 0, true, payload.c_str());
    }
}



esp_err_t GeniusGateway::_hekatron_analyze_packet_data(uint8_t *packet_data, size_t data_length, hekatron_packet_t *analyzed_packet)
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
    memset(analyzed_packet, 0, sizeof(hekatron_packet_t));

    /* Determine type of Hekatron packet */
    switch (data_length)
    {

    case LEN_COMMISSIONING_PACKET:
        analyzed_packet->type = HPT_COMMISSIONING;
        break;
    case LEN_UNKNOWN_PURPOSE_1_PACKET:
        analyzed_packet->type = HPT_UKNOWN_PURPOSE_1;
        break;
    case LEN_UNKNOWN_PURPOSE_2_PACKET:
        analyzed_packet->type = HPT_UKNOWN_PURPOSE_2;
        break;
    case LEN_ALARM_PACKET:
        if (packet_data[DATAPOS_ALARM_ACTIVE_FLAG] == 1)
        {
            analyzed_packet->type = HPT_ALARMING;
        }
        else if (packet_data[DATAPOS_ALARM_SILENCE_FLAG] == 1)
        {
            analyzed_packet->type = HPT_ALARM_SILENCING;
        }
        else
        {
            analyzed_packet->type = HPT_UNKNOWN;
        }
        break;
    case LEN_LINE_TEST_PACKET:
        analyzed_packet->type = HPT_LINE_TEST;
        break;
    default:
        analyzed_packet->type = HPT_UNKNOWN;
        break;
    }

    if (analyzed_packet->type != HPT_UNKNOWN)
    {
        analyzed_packet->origin_id = EXTRACT(packet_data, DATAPOS_GENERAL_ORIGIN_RADIO_MODULE_ID);
        analyzed_packet->sender_id = EXTRACT(packet_data, DATAPOS_GENERAL_SENDER_RADIO_MODULE_ID);
        analyzed_packet->line_id = EXTRACT(packet_data, DATAPOS_GENERAL_LINE_ID);
        analyzed_packet->hops = HOPS_FIRST - packet_data[DATAPOS_GENERAL_HOPS];
    }

    return ESP_OK;
}

void GeniusGateway::_rx_packets()
{
    cc1101_packet_t packet;

    ESP_LOGI(pcTaskGetName(0), "Started.");

    while (1)
    {
        /* Wait (blocking) until being notified (by ISR), that a packet has been received.
           Note: the parameter 'xClearCountOnExit' is pdTRUE, which has the effect of
           clearing the task's notification value back to 0, making the notification
           value act like a binary semaphore. */
        if (ulTaskNotifyTakeIndexed(RX_TASK_NOTIFICATION_INDEX, pdTRUE, RX_TASK_MAX_WAITING_TICKS) == 1)
        {
            // Fetch the packet
            if (cc1101_receive_data(&packet) == ESP_OK)
            {
                // ESP_LOGD(pcTaskGetName(0), "Packet received: packet.length: %d", packet.length);

                hekatron_packet_t packet_details;
                if (_hekatron_analyze_packet_data(packet.data, packet.length, &packet_details) == ESP_OK)
                {
                    if (packet_details.type == HPT_ALARMING ||
                        packet_details.type == HPT_ALARM_SILENCING)
                    {
                        /* Determine source smoke alarm */
                        uint32_t source_id = EXTRACT(packet.data, DATAPOS_ALARM_SOURCE_SMOKE_ALARM_ID);

                        // TODO
                    }
                }

                /* Send data to WebSocket logger */
                gpio_set_level(GPIO_TEST2, 1); // Temporary: Measuring execution time
                _webSocketLogger.logPacket(&packet);
                gpio_set_level(GPIO_TEST2, 0); // Temporary: Measuring execution time
            }

            // Temporary GPIO toggle to measure packet handling time
            gpio_set_level(GPIO_TEST1, 0);
        }
        else
        {
            /* The call to ulTaskNotifyTake() timed out, if ulTaskNotifyTakeIndexed()
             * was called with xTicksToWait set to a value < portMAX_DELAY. */
            vTaskDelay(1);
        }
    }

    // never reach here
    vTaskDelete(NULL);
}