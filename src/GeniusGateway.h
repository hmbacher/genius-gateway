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
#include <GeniusDevicesService.h>
#include <AlarmLinesService.h>
#include <GatewaySettingsService.h>
#include <AlarmPublishingSettingsService.h>
#include <ReportSettingsService.h>
#include <GatewayDeviceMqttService.h>
#include <CC1101Controller.h>
#include <CC1101PinsService.h>
#include <cc1101.h>
#include <AlarmBlocker.h>
#include <vector>

#define RX_TASK_STACK_SIZE 4096  ///< Stack size for RX task in bytes
#define RX_TASK_PRIORITY 20      ///< Priority level for RX task
#define RX_TASK_CORE_AFFINITY 1  ///< CPU core affinity for RX task (0 or 1)
#define RX_TASK_NAME "genius-rx" ///< Name identifier for RX task

#define HOPS_FIRST 0xF ///< Initial hops value for packet routing
#define HOPS_LAST 0x0  ///< Final hops value for packet routing

#define MIN_GENIUS_PACKET_LENGTH LEN_UNKNOWN_PURPOSE_1_PACKET ///< Minimum valid packet length

#define LEN_COMMISSIONING_PACKET 37      ///< Commissioning packet length
#define LEN_COMMISSIONING_PROBE_REQUEST_PACKET 28  ///< CommissioningProbe request packet length
#define LEN_COMMISSIONING_PROBE_RESPONSE_PACKET 32 ///< CommissioningProbe response packet length
#define LEN_ALARM_PACKET 36              ///< Alarm packet length
#define LEN_LINE_TEST_PACKET 29          ///< Line test packet length
#define LEN_CONFIG_CHECK_PROBE_REQUEST_PACKET 28 ///< ConfigCheckProbe request length (same wire length as CommissioningProbe Request; distinguished by the message-type byte)
#define LEN_CONFIG_CHECK_PROBE_RESPONSE_PACKET 36 ///< ConfigCheckProbe response length (same wire length as Alarming; distinguished by the message-type byte)

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
#define DATAPOS_MSG_TYPE 27                       ///< Message-type discriminator byte (constant per packet type - how the radio module routes a received frame)
#define DATAPOS_CONFIG_CHECK_PROBE_STATUS 29              ///< ConfigCheckProbe response: per-device status/capability flag
#define DATAPOS_CONFIG_CHECK_PROBE_GROUPLINE 30           ///< ConfigCheckProbe response: detector group/line (hi nibble = group A-H, lo nibble = line 0-9)

/* Message-type byte values (data[DATAPOS_MSG_TYPE]) - the on-air type discriminator the
 * genuine radio module routes on. Length disambiguates the single shared value (0x00). */
#define MSGTYPE_ALARM_OR_COMMISSIONING_PROBE_REQ 0x00 ///< Alarming (36 B) OR CommissioningProbe Request (28 B)
#define MSGTYPE_COMMISSIONING_PROBE_RESPONSE 0x01     ///< CommissioningProbe Response (32 B, carries Req-SN)
#define MSGTYPE_COMMISSIONING 0x03          ///< Alarm-line commissioning (37 B)
#define MSGTYPE_LINE_TEST 0x04              ///< Line test start/stop (29 B)
#define MSGTYPE_CONFIG_CHECK_PROBE_05 0x05       ///< ConfigCheckProbe probe (29 B) - firmware-derived, provisional (not yet capture-confirmed)
#define MSGTYPE_CONFIG_CHECK_PROBE_REQUEST 0x06     ///< ConfigCheckProbe request (28 B)
#define MSGTYPE_CONFIG_CHECK_PROBE_RESPONSE 0x08    ///< ConfigCheckProbe response (36 B)

#define EXTRACT32(buffer, pos) (__ntohl(*(uint32_t *)&buffer[pos])) ///< Extract 32-bit value with network byte order conversion
#define EXTRACT32_REV(buffer, pos) (*(uint32_t *)&buffer[pos])      ///< Extract 32-bit value without byte order conversion

#define GATEWAY_ID 0xFFFFFFFE ///< Gateway identifier for genius protocol

#define RX_TASK_NOTIFICATION_INDEX 0            ///< Task notification array index (must be < CONFIG_FREERTOS_TASK_NOTIFICATION_ARRAY_ENTRIES)
#define RX_TASK_MAX_WAITING_TICKS portMAX_DELAY ///< Maximum ticks to wait for packet reception

#define GATEWAY_EVENT_ALARM "alarm" ///< WebSocket event name for alarm notifications
#define GATEWAY_EVENT_CONFIG_CHECK_PROBE "config-check-probe" ///< WebSocket event for ConfigCheckProbe survey progress/result

#define GATEWAY_ALARM_STATE_EMIT_INTERVAL_MS 1000 ///< Alarm state emission interval (1 second)

#define GATEWAY_SERVICE_PATH_END_ALARMS "/rest/end-alarms"               ///< REST endpoint for ending alarms
#define GATEWAY_SERVICE_PATH_END_ALARMBLOCKING "/rest/end-alarmblocking" ///< REST endpoint for ending alarm blocking
#define GATEWAY_MAX_ALARM_BLOCKING_TIME_S 3600UL                         ///< Maximum alarm blocking time (1 hour)

#define GATEWAY_SERVICE_PATH_PROBE "/rest/config-check-probe"                       ///< POST: start a ConfigCheckProbe RSSI survey
#define GATEWAY_SERVICE_PATH_PROBE_STOP "/rest/config-check-probe/stop"             ///< POST: finalize the running survey now (keep + persist responders)
#define GATEWAY_SERVICE_PATH_PROBE_CANCEL "/rest/config-check-probe/cancel"         ///< POST: abort the running ConfigCheckProbe survey (discard)
#define GATEWAY_SERVICE_PATH_PROBE_DISCOVERED "/rest/config-check-probe/discovered" ///< GET: last survey's unknown (discovered) responders
#define CONFIG_CHECK_PROBE_SWEEPS 2            ///< Broadcast sweeps per survey (mirrors the Genius-Port two-sweep "Bahlinger" pattern)
#define CONFIG_CHECK_PROBE_COLLECT_MS 30000    ///< Active-listening window after each sweep (ms) - matches Genius-Port silent_ping_timeout
#define CONFIG_CHECK_PROBE_MAX_RESPONDERS 64   ///< Cap on distinct responders collected per survey

typedef enum genius_packet_type
{
  HPT_UNKNOWN = -1,       ///< Unknown packet type
  HPT_COMMISSIONING = 0,  ///< Commissioning packet (smoke detector assignment to alarm line)
  HPT_COMMISSIONING_PROBE_REQUEST,  ///< CommissioningProbe request packet (request for smoke detectors to identify)
  HPT_COMMISSIONING_PROBE_RESPONSE, ///< CommissioningProbe response packet (smoke detector identification response)
  HPT_ALARM_START,        ///< Alarm start packet (smoke detection notification)
  HPT_ALARM_STOP,         ///< Alarm stop packet (smoke cleared or alarm silenced)
  HPT_LINE_TEST_START,    ///< Line test start packet (line test initiation)
  HPT_LINE_TEST_STOP,     ///< Line test stop packet (line test completion)
  HPT_CONFIG_CHECK_PROBE_REQUEST, ///< ConfigCheckProbe request (radio module 0x55-family, subtype 0x06; direct-range probe)
  HPT_CONFIG_CHECK_PROBE_RESPONSE ///< ConfigCheckProbe response (radio module 0x55-family, subtype 0x08; carries responder group/line + status)
} genius_packet_type_t;

typedef struct genius_packet_t
{
  genius_packet_type_t type; ///< Packet type classification
  uint32_t origin_id;        ///< Original sender radio module ID
  uint32_t sender_id;        ///< Current sender radio module ID
  uint32_t line_id;          ///< Associated alarm line ID
  uint8_t hops;              ///< Hop count for packet routing
} genius_packet_t;

/// One radio module that answered a ConfigCheckProbe survey (heard directly, hops = 0).
typedef struct config_check_responder_t
{
  uint32_t radioModuleSN; ///< Responder's own radio-module serial (packet Org-SN)
  uint32_t lineId;        ///< Responder's own Line-ID
  int8_t rssi;            ///< Best measured RSSI this sweep, in dBm
  uint8_t status;         ///< Response status/capability byte (offset 29)
  uint8_t groupLine;      ///< Group/line nibble byte (offset 30)
} config_check_responder_t;

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
  ESP32SvelteKit *_sveltekit;                             ///< Framework instance (for HAService access)
  AlarmPublishingSettingsService _alarmPublishingSettingsService; ///< Simple alarm-publishing settings service
  ReportSettingsService _reportSettingsService;                   ///< PDF report header settings service
  GatewaySettingsService _gatewaySettings;                ///< Gateway settings service
  GatewayDeviceMqttService _gatewayDeviceMqttService;     ///< Gateway device MQTT service
  GeniusDevicesService _geniusDevices;                  ///< Genius devices service
  AlarmLinesService _alarmLines;                          ///< Alarm lines service
  WSLogger _wsLogger;                                     ///< WebSocket logger service
  VisualizerSettingsService _visualizerSettingsService;   ///< Visualizer settings service
  CC1101Controller _cc1101Controller;                     ///< CC1101 radio controller
  CC1101PinsService _cc1101PinsService;                   ///< CC1101 runtime pin configuration service
  AlarmBlocker _alarmBlocker;                             ///< Alarm blocker service

  uint32_t _lastPacketHash; ///< Hash of last received packet for duplicate detection
  bool _hasLastPacketHash;  ///< Flag indicating if last packet hash is valid

  TaskHandle_t _mqttPublishTaskHandle = nullptr; ///< Handle for persistent HA publish task

  // --- ConfigCheckProbe RSSI survey session state (guarded by _probeMutex) ---
  SemaphoreHandle_t _probeMutex = nullptr;                  ///< Guards the probe-session fields below
  TaskHandle_t _probeFinalizeTaskHandle = nullptr;         ///< Persistent task: waits out the response window, then finalizes
  volatile bool _probeActive = false;                      ///< True while a survey is collecting responses
  volatile bool _probeStopRequested = false;               ///< User asked to finalize early: keep responders, skip unreached-marking
  uint32_t _probeSweepId = 0;                              ///< Increments each survey (stale-timer guard, UI correlation)
  std::vector<config_check_responder_t> _probeResponders;  ///< Responders collected in the active sweep
  std::vector<config_check_responder_t> _probeDiscovered;  ///< Last survey's unknown responders (for the discovery UI)

  /// Handle REST request to end alarming for devices
  esp_err_t _handleEndAlarming(PsychicRequest *request, JsonVariant &json);

  /// Handle REST request to end alarm blocking
  esp_err_t _handleEndBlocking(PsychicRequest *request);

  /// Handle REST request to start a ConfigCheckProbe RSSI survey
  esp_err_t _handleStartProbe(PsychicRequest *request);

  /// Handle REST request to finalize the running survey now (keep + persist responders, skip
  /// marking non-responders unreached since the survey was cut short)
  esp_err_t _handleStopProbe(PsychicRequest *request);

  /// Handle REST request to abort the running ConfigCheckProbe survey (dialog closed by the user)
  esp_err_t _handleCancelProbe(PsychicRequest *request);

  /// Handle REST request for the last survey's discovered (unknown) responders
  esp_err_t _handleGetDiscovered(PsychicRequest *request);

  /// Record one ConfigCheckProbe response into the active survey (dedup by SN, keep best RSSI)
  void _recordProbeResponse(const genius_packet_t *details, const cc1101_packet_t *packet);

  /// Close the survey: update known devices' RSSI, split out unknowns, persist, emit result. A no-op
  /// if the survey was cancelled (discarded). On an early stop, keeps/persists the responders heard
  /// so far but skips the "unreached" marking (the survey didn't run to completion).
  void _finalizeProbe();

  /// Persistent worker: woken when a survey starts, waits out the response-collection window,
  /// then runs _finalizeProbe - keeping the flash write + WS emit on a task that may block (not
  /// the HTTP handler thread and not an esp_timer callback).
  void _probeFinalizeTask();

  /// Interruptible listen used by the finalize worker: sleeps up to @p total ticks but returns
  /// early (false) as soon as the survey is cancelled, so a Cancel takes effect within ~250 ms.
  bool _probeListen(TickType_t total);

  /// Static wrapper for the finalize worker task
  static void _probeFinalizeTaskImpl(void *_this) { static_cast<GeniusGateway *>(_this)->_probeFinalizeTask(); }

  /// Serialize a single responder into a JSON object (shared by the live event + list serializer)
  static void _responderToJson(const config_check_responder_t &r, JsonObject o);

  /// Serialize a responder list into a JSON array (shared by the WS result + discovered GET)
  static void _respondersToJson(const std::vector<config_check_responder_t> &list, JsonArray arr);

  /// Main packet reception loop
  void _rx_packets();

  /// Static wrapper for packet reception task
  static void _rx_packetsImpl(void *_this) { static_cast<GeniusGateway *>(_this)->_rx_packets(); }

  /// HA commissioningprobe publish loop (persistent task, woken by onConnect notification)
  void _mqttPublishTask();

  /// Static wrapper for HA publish task
  static void _mqttPublishTaskImpl(void *_this) { static_cast<GeniusGateway *>(_this)->_mqttPublishTask(); }

  /// Analyze received packet data and extract packet information
  esp_err_t _genius_analyze_packet_data(uint8_t *packet_data, size_t data_length, genius_packet_t *analyzed_packet);

  /// Emit current alarm state via WebSocket
  void _emitAlarmState();
};
