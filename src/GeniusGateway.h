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
#include <GeniusPacket.h>
#include <vector>

#define RX_TASK_STACK_SIZE 4096  ///< Stack size for RX task in bytes
#define RX_TASK_PRIORITY 20      ///< Priority level for RX task
#define RX_TASK_CORE_AFFINITY 1  ///< CPU core affinity for RX task (0 or 1)
#define RX_TASK_NAME "genius-rx" ///< Name identifier for RX task

/* Frame layout, packet types and classification: see GeniusPacket.h */

#define EXTRACT32(buffer, pos) (__ntohl(*(uint32_t *)&buffer[pos])) ///< Extract 32-bit value with network byte order conversion
#define EXTRACT32_REV(buffer, pos) (*(uint32_t *)&buffer[pos])      ///< Extract 32-bit value without byte order conversion

#define RX_TASK_NOTIFICATION_INDEX 0            ///< Task notification array index (must be < CONFIG_FREERTOS_TASK_NOTIFICATION_ARRAY_ENTRIES)
#define RX_TASK_MAX_WAITING_TICKS portMAX_DELAY ///< Maximum ticks to wait for packet reception

#define GATEWAY_EVENT_ALARM "alarm" ///< WebSocket event name for alarm notifications
#define GATEWAY_EVENT_DEVICE_STATUS "device-status" ///< WebSocket event for smoke detector status changes (battery low, faults)
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

/// One radio module that answered a ConfigCheckProbe survey (heard directly, hops = 0).
typedef struct config_check_responder_t
{
  uint32_t radioModuleSN; ///< Responder's own radio-module serial (packet Org-SN)
  uint32_t lineId;        ///< Responder's own Line-ID
  int8_t rssi;            ///< Best measured RSSI this sweep, in dBm
  uint8_t status;         ///< Response status/capability byte (offset 29)
  uint8_t groupLine;      ///< Line code of the responder: position of its rotary switches, letter << 4 | number (offset 30)
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
  TaskHandle_t _alarmEventTaskHandle = nullptr;  ///< Handle for the task that sends the alarm state to the web clients

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

  /// Handle a received status change: alarm start/stop (alarm bit) plus all other status flags
  void _handleStatusChange(const genius_packet_t &details);

  /// Emit current alarm state via WebSocket
  void _emitAlarmState();

  /**
   * @brief Have the alarm state sent to the web clients without waiting for it
   *
   * For the packet reception task: sending to the web clients can block on a client that
   * does not take the data, and reception must not stand still for that. The state is sent
   * by a task of its own; several requests in a row may result in one event with the
   * latest state.
   */
  void _requestAlarmStateEmit();

  /**
   * @brief Have the device status summary sent to the web clients without waiting for it
   *
   * Same reason and same task as for the alarm state: a status change is applied by the
   * packet reception task, which must not wait for a web client.
   */
  void _requestDeviceStatusEmit();

  /// Sends the alarm state and / or the device status summary when woken by one of the
  /// _request...Emit() functions (persistent task)
  void _alarmEventTask();

  static constexpr uint32_t EVENT_TASK_ALARM_STATE = 0x01;   ///< Notification bit: send the alarm state
  static constexpr uint32_t EVENT_TASK_DEVICE_STATUS = 0x02; ///< Notification bit: send the device status summary
  static constexpr uint32_t EVENT_TASK_DEVICE_RUNTIME = 0x04; ///< Notification bit: send the changed last seen / signal values

  /// Static wrapper for the alarm event task
  static void _alarmEventTaskImpl(void *_this) { static_cast<GeniusGateway *>(_this)->_alarmEventTask(); }

  /// Emit the smoke detectors' status summary (faulty / battery-low counts) via WebSocket
  void _emitDeviceStatus();
};
