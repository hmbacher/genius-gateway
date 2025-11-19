/**
 * @file GeniusGateway.h
 * @brief Main gateway service for managing genius protocol communication
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

#pragma once

#include <ESP32SvelteKit.h>
#include <WSLogger.h>
#include <VisualizerSettingsService.h>
#include <GatewayDevicesService.h>
#include <AlarmLinesService.h>
#include <GatewaySettingsService.h>
#include <GatewayMqttSettingsService.h>
#include <CC1101Controller.h>
#include <cc1101.h>
#include <AlarmBlocker.h>

#define RX_TASK_STACK_SIZE 4096  ///< Stack size for RX task in bytes
#define RX_TASK_PRIORITY 20      ///< Priority level for RX task
#define RX_TASK_CORE_AFFINITY 1  ///< CPU core affinity for RX task (0 or 1)
#define RX_TASK_NAME "genius-rx" ///< Name identifier for RX task

#define HOPS_FIRST 0xF ///< Initial hops value for packet routing
#define HOPS_LAST 0x0  ///< Final hops value for packet routing

#define MIN_GENIUS_PACKET_LENGTH LEN_UNKNOWN_PURPOSE_1_PACKET ///< Minimum valid packet length

#define LEN_COMMISSIONING_PACKET 37      ///< Commissioning packet length
#define LEN_DISCOVERY_REQUEST_PACKET 28  ///< Discovery request packet length
#define LEN_DISCOVERY_RESPONSE_PACKET 32 ///< Discovery response packet length
#define LEN_ALARM_PACKET 36              ///< Alarm packet length
#define LEN_LINE_TEST_PACKET 29          ///< Line test packet length

#define DATAPOS_GENERAL_ORIGIN_RADIO_MODULE_ID 9  ///< Origin radio module ID position
#define DATAPOS_GENERAL_SENDER_RADIO_MODULE_ID 14 ///< Sender radio module ID position
#define DATAPOS_GENERAL_LINE_ID 18                ///< Line ID position
#define DATAPOS_GENERAL_HOPS 22                   ///< Hops counter position
#define DATAPOS_COMISSIONING_NEW_LINE_ID 28       ///< New line ID position in commissioning packets
#define DATAPOS_COMOSSIONING_TIME_HOUR 32         ///< Hour position in commissioning packets
#define DATAPOS_COMOSSIONING_TIME_MINUTE 33       ///< Minute position in commissioning packets
#define DATAPOS_COMOSSIONING_TIME_SECOND 34       ///< Second position in commissioning packets
#define DATAPOS_ALARM_ACTIVE_FLAG 28              ///< Alarm active flag position
#define DATAPOS_ALARM_SILENCE_FLAG 30             ///< Alarm silence flag position
#define DATAPOS_ALARM_SOURCE_SMOKE_ALARM_ID 32    ///< Source smoke alarm ID position
#define DATAPOS_LINE_TEST_START_STOP_FLAG 28      ///< Line test start/stop flag position

#define EXTRACT32(buffer, pos) (__ntohl(*(uint32_t *)&buffer[pos])) ///< Extract 32-bit value with network byte order conversion
#define EXTRACT32_REV(buffer, pos) (*(uint32_t *)&buffer[pos])      ///< Extract 32-bit value without byte order conversion

#define GATEWAY_ID 0xFFFFFFFE ///< Gateway identifier for genius protocol

#define RX_TASK_NOTIFICATION_INDEX 0            ///< Task notification array index (must be < CONFIG_FREERTOS_TASK_NOTIFICATION_ARRAY_ENTRIES)
#define RX_TASK_MAX_WAITING_TICKS portMAX_DELAY ///< Maximum ticks to wait for packet reception

#define GATEWAY_EVENT_ALARM "alarm" ///< WebSocket event name for alarm notifications

#define GATEWAY_ALARM_STATE_EMIT_INTERVAL_MS 1000 ///< Alarm state emission interval (1 second)

#define GATEWAY_SERVICE_PATH_END_ALARMS "/rest/end-alarms"               ///< REST endpoint for ending alarms
#define GATEWAY_SERVICE_PATH_END_ALARMBLOCKING "/rest/end-alarmblocking" ///< REST endpoint for ending alarm blocking
#define GATEWAY_MAX_ALARM_BLOCKING_TIME_S 3600UL                         ///< Maximum alarm blocking time (1 hour)

typedef enum genius_packet_type
{
  HPT_UNKNOWN = -1,       ///< Unknown packet type
  HPT_COMMISSIONING = 0,  ///< Commissioning packet (smoke detector assignment to alarm line)
  HPT_DISCOVERY_REQUEST,  ///< Discovery request packet (request for smoke detectors to identify)
  HPT_DISCOVERY_RESPONSE, ///< Discovery response packet (smoke detector identification response)
  HPT_ALARM_START,        ///< Alarm start packet (smoke detection notification)
  HPT_ALARM_STOP,         ///< Alarm stop packet (smoke cleared or alarm silenced)
  HPT_LINE_TEST_START,    ///< Line test start packet (line test initiation)
  HPT_LINE_TEST_STOP      ///< Line test stop packet (line test completion)
} genius_packet_type_t;

typedef struct genius_packet_t
{
  genius_packet_type_t type; ///< Packet type classification
  uint32_t origin_id;        ///< Original sender radio module ID
  uint32_t sender_id;        ///< Current sender radio module ID
  uint32_t line_id;          ///< Associated alarm line ID
  uint8_t hops;              ///< Hop count for packet routing
} genius_packet_t;

/// Main gateway service for managing genius protocol communication
class GeniusGateway
{
public:
  GeniusGateway(ESP32SvelteKit *sveltekit);

  static TaskHandle_t xRxTaskHandle;

  /// Initialize the genius gateway service
  void begin();

private:
  static constexpr const char *TAG = "GeniusGateway"; ///< Logging tag

  PsychicHttpServer *_server;                             ///< HTTP server instance
  SecurityManager *_securityManager;                      ///< Security manager instance
  EventSocket *_eventSocket;                              ///< WebSocket event manager
  PsychicMqttClient *_mqttClient;                         ///< MQTT client instance
  FeaturesService *_featureService;                       ///< Feature flags service
  GatewayDevicesService _gatewayDevices;                  ///< Gateway devices service
  AlarmLinesService _alarmLines;                          ///< Alarm lines service
  GatewaySettingsService _gatewaySettings;                ///< Gateway settings service
  GatewayMqttSettingsService _gatewayMqttSettingsService; ///< MQTT settings service
  WSLogger _wsLogger;                                     ///< WebSocket logger service
  VisualizerSettingsService _visualizerSettingsService;   ///< Visualizer settings service
  CC1101Controller _cc1101Controller;                     ///< CC1101 radio controller
  AlarmBlocker _alarmBlocker;                             ///< Alarm blocker service

  uint32_t _lastPacketHash; ///< Hash of last received packet for duplicate detection
  bool _hasLastPacketHash;  ///< Flag indicating if last packet hash is valid

  /// Handle REST request to end alarming for devices
  esp_err_t _handleEndAlarming(PsychicRequest *request, JsonVariant &json);

  /// Handle REST request to end alarm blocking
  esp_err_t _handleEndBlocking(PsychicRequest *request);

  /// Publish device states to MQTT
  void _mqttPublishDevices(bool onlyState = false, bool forceAll = false);

  /// Main packet reception loop
  void _rx_packets();

  /// Static wrapper for packet reception task
  static void _rx_packetsImpl(void *_this) { static_cast<GeniusGateway *>(_this)->_rx_packets(); }

  /// Analyze received packet data and extract packet information
  esp_err_t _genius_analyze_packet_data(uint8_t *packet_data, size_t data_length, genius_packet_t *analyzed_packet);

  /// Emit current alarm state via WebSocket
  void _emitAlarmState();
};
