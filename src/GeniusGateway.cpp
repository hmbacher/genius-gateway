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
    /* Create packet handling task — stack forced to internal DRAM so ISR-driven
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
     * a task notification. onConnect simply wakes it — the MQTT event task is
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
