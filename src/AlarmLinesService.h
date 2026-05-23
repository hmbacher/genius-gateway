/**
 * @file AlarmLinesService.h
 * @brief Alarm Lines service for managing genius alarm line communication
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
#include <EventSocket.h>
#include <FSPersistence.h>
#include <HttpEndpoint.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <CC1101Controller.h>
#include <Utils.hpp>
#include <cc1101.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <map>
#include <AlarmPublishingSettingsService.h>
#include <HomeAssistant/HAService.h>
#include <HomeAssistant/HADevice.h>
#include <HomeAssistant/HAButton.h>
#include <HomeAssistant/HASensor.h>
#include <PsychicMqttClient.h>
#include <SettingValue.h>


#define ALARMLINES_FILE "/config/alarm-lines.json"            ///< Configuration file path
#define ALARMLINES_SERVICE_PATH "/rest/alarm-lines"           ///< HTTP REST API service endpoint
#define ALARMLINES_PATH_ACTIONS ALARMLINES_SERVICE_PATH "/do" ///< HTTP endpoint for actions
#define ALARMLINES_ID_BROADCAST 0xFFFFFFFF                    ///< Broadcast alarm line ID (all lines)
#define ALARMLINES_ID_NONE 0x00000000                         ///< No alarm line ID (unassigned)
#define ALARMLINES_MAX_NUM 100                                ///< Maximum number of alarm lines supported

#define ALARMLINES_NAME_MAX_LENGTH 100                        ///< Maximum length for alarm line names
#define ALARMLINES_ORIGIN_ID "alarm-lines"                    ///< Origin ID for update handler tracking
#define ALARMLINES_TX_TASK_STACK_SIZE 4096                    ///< Stack size for transmission task in bytes
#define ALARMLINES_TX_TASK_PRIORITY 20                        ///< Priority level for transmission task
#define ALARMLINES_TX_TASK_NAME "alarmlines-tx"               ///< Name identifier for transmission task
#define ALARMLINES_TX_TASK_CORE_AFFINITY 1                    ///< CPU core affinity for transmission task (0 or 1)
#define ALARMLINES_TX_PERIOD_TIMER_NAME "alarmlines-tx-timer" ///< Timer name for transmission period timing
#define ALARMLINES_TX_TIMEOUT_MS 10000LU                      ///< Transmission timeout in milliseconds (10 seconds)

/// Task notification array index for transmission task (must be < CONFIG_FREERTOS_TASK_NOTIFICATION_ARRAY_ENTRIES)
#define ALARMLINES_TX_TASK_NOTIFICATION_INDEX 0

/// Maximum ticks to wait between packet transmission iterations
#define ALARMLINES_TX_TASK_ITERATION_MAX_WAITING_TICKS pdMS_TO_TICKS(1000) // 1 second

#define ALARMLINES_TX_PERIOD_LINETEST_US 8395    ///< Transmission period for line test packets in microseconds (8.395 ms)
#define ALARMLINES_TX_NUM_REPEAT_LINETEST 370    ///< Number of line test packet repetitions
#define ALARMLINES_LINETEST_FIRST_PCKTCNT 0x18CC ///< First packet count value for line test sequence
#define ALARMLINES_LINETEST_LAST_PCKTCNT 0x0002  ///< Last packet count value for line test sequence

#define ALARMLINES_LINETEST_PCKTCNT_STEP (float)(ALARMLINES_LINETEST_FIRST_PCKTCNT - ALARMLINES_LINETEST_LAST_PCKTCNT) / (float)(ALARMLINES_TX_NUM_REPEAT_LINETEST - 1) ///< Packet count step size calculation for line test

#define ALARMLINES_TX_PERIOD_FIREALARM_US 9855                                                                                                                              ///< Transmission period for fire alarm packets in microseconds (9.855 ms)
#define ALARMLINES_TX_NUM_REPEAT_FIREALARM 315                                                                                                                              ///< Number of fire alarm packet repetitions
#define ALARMLINES_FIREALARM_FIRST_PCKTCNT 0x18CC                                                                                                                           ///< First packet count value for fire alarm sequence
#define ALARMLINES_FIREALARM_LAST_PCKTCNT 0x000A                                                                                                                            ///< Last packet count value for fire alarm sequence
#define ALARMLINES_FIREALARM_PCKTCNT_STEP (float)(ALARMLINES_FIREALARM_FIRST_PCKTCNT - ALARMLINES_FIREALARM_LAST_PCKTCNT) / (float)(ALARMLINES_TX_NUM_REPEAT_FIREALARM - 1) ///< Packet count step size calculation for fire alarm

#define ALARMLINES_EVENT_NEW_LINE "new-alarm-line"                    ///< WebSocket event for new alarm line discovery
#define ALARMLINES_EVENT_ACTION_STARTED "alarm-line-action-started"   ///< WebSocket event for action start notification
#define ALARMLINES_EVENT_ACTION_FINISHED "alarm-line-action-finished" ///< WebSocket event for action completion notification

#define ALARMLINES_NVS_NAMESPACE "gg-alarmlines" ///< NVS namespace for alarm lines data storage
#define ALARMLINES_NVS_SEQ_KEY "pkt_seq_num"     ///< NVS key for packet sequence number storage
#define ALARMLINES_NVS_SEQ_DEFAULT 0             ///< Default packet sequence number value

/// Enumeration for alarm line acquisition methods
typedef enum alarm_line_acquisition
{
    ALA_MIN = -1,      ///< Boundary check minimum value
    ALA_BUILT_IN = 0,  ///< Built-in alarm line (e.g. broadcast line)
    ALA_GENIUS_PACKET, ///< Discovered via received genius packet
    ALA_MANUAL,        ///< Manually added via web interface
    ALA_ACOUSTIC,      ///< Discovered via acoustic device readout
    ALA_MAX            ///< Boundary check maximum value
} alarm_line_acquisition_t;

/// Structure representing a genius alarm line
typedef struct genius_alarm_line
{
    uint32_t id;                          ///< Unique alarm line ID (0xFFFFFFFF = broadcast, 0x00000000 = none)
    String name;                          ///< Human-readable alarm line name
    time_t created;                       ///< Creation timestamp (Unix epoch)
    alarm_line_acquisition_t acquisition; ///< How this line was discovered/added
    bool published;                       ///< Whether the current line configuration has been published via MQTT (runtime state, not persisted)
} genius_alarm_line_t;

/// Data model class for managing alarm line collections
class AlarmLines
{
public:
    static constexpr const char *TAG = "AlarmLines"; ///< Logging tag
    std::vector<genius_alarm_line_t> lines;          ///< Vector containing all managed alarm lines
    std::vector<uint32_t> deletedLineIds;            ///< Temporary storage for deleted line IDs (populated during update)

    /// Deserialize alarm lines from JSON object
    static void read(AlarmLines &alarmLines, JsonObject &root)
    {
        JsonArray jsonDevices = root["lines"].to<JsonArray>();
        for (auto &line : alarmLines.lines)
        {
            JsonObject jsonLine = jsonDevices.add<JsonObject>();
            jsonLine["id"] = line.id;
            jsonLine["name"] = line.name;
            jsonLine["created"] = Utils::time_t_to_iso8601(line.created);
            jsonLine["acquisition"] = line.acquisition;
            // Note: 'published' flag is not serialized (runtime state only)
        }

        ESP_LOGV(AlarmLines::TAG, "Alarm lines configurations read.");
    }

    /// Update alarm lines from JSON object
    static StateUpdateResult update(JsonObject &root, AlarmLines &alarmLines, const String &originId)
    {
        if (root["lines"].is<JsonArray>())
        {
            // Track current line IDs before clearing
            std::vector<uint32_t> oldLineIds;
            oldLineIds.reserve(alarmLines.lines.size());
            for (const auto &line : alarmLines.lines)
            {
                oldLineIds.push_back(line.id);
            }

            // Parse new lines from JSON
            std::vector<uint32_t> newLineIds;
            std::vector<genius_alarm_line_t> newLines;

            int i = 0;
            for (JsonVariant jsonLineArrItem : root["lines"].as<JsonArray>())
            {
                if (i++ >= ALARMLINES_MAX_NUM)
                {
                    ESP_LOGE(AlarmLines::TAG, "Too many alarm lines. Maximum allowed is %d.", ALARMLINES_MAX_NUM);
                    break;
                }

                JsonObject jsonLine = jsonLineArrItem.as<JsonObject>();
                if (!jsonLine["id"].is<uint32_t>() ||
                    !jsonLine["name"].is<String>() ||
                    !jsonLine["created"].is<String>() ||
                    !jsonLine["acquisition"].is<alarm_line_acquisition_t>())
                {
                    ESP_LOGE(AlarmLines::TAG, "Invalid alarm line configuration.");
                    return StateUpdateResult::ERROR;
                }

                genius_alarm_line_t newLine;
                newLine.id = jsonLine["id"].as<uint32_t>();
                if (newLine.id == ALARMLINES_ID_NONE)
                {
                    ESP_LOGW(AlarmLines::TAG, "Skipping reserved alarm line ID: None (0 / 0x00000000).");
                    continue;
                }
                if (newLine.id == ALARMLINES_ID_BROADCAST)
                {
                    ESP_LOGW(AlarmLines::TAG, "Skipping reserved alarm line ID: Broadcast (4294967295 / 0xFFFFFFFF).");
                    continue;
                }
                newLine.name = jsonLine["name"].as<String>();
                newLine.created = Utils::iso8601_to_time_t(jsonLine["created"].as<String>());
                newLine.acquisition = jsonLine["acquisition"].as<alarm_line_acquisition_t>();

                // Check if this line existed before and preserve its published state if unchanged
                newLine.published = false;
                for (const auto &oldLine : alarmLines.lines)
                {
                    if (oldLine.id == newLine.id && oldLine.name == newLine.name)
                    {
                        // Line exists and name hasn't changed - preserve published state
                        newLine.published = oldLine.published;
                        break;
                    }
                }

                newLines.push_back(newLine);
                newLineIds.push_back(newLine.id);

                ESP_LOGV(AlarmLines::TAG, "Added alarm line: %s", newLine.name.c_str());
            }

            // Detect deleted lines (in old list but not in new list)
            alarmLines.deletedLineIds.clear();
            for (uint32_t oldId : oldLineIds)
            {
                if (std::find(newLineIds.begin(), newLineIds.end(), oldId) == newLineIds.end())
                {
                    // Line was deleted - store ID for unpublishing
                    alarmLines.deletedLineIds.push_back(oldId);
                    ESP_LOGI(AlarmLines::TAG, "Alarm line with ID %lu marked for deletion.", oldId);
                }
            }

            // Replace with new lines
            alarmLines.lines = std::move(newLines);
        }

        ESP_LOGV(AlarmLines::TAG, "AlarmLines configurations updated.");

        return StateUpdateResult::CHANGED;
    }

private:
};

/// Service class for managing alarm lines and RF transmission
class AlarmLinesService : public StatefulService<AlarmLines>
{
public:
    static constexpr const char *TAG = "AlarmLinesService"; ///< Logging tag

    AlarmLinesService(ESP32SvelteKit *sveltekit, PsychicMqttClient *mqttClient, CC1101Controller *cc1101Ctrl, AlarmPublishingSettingsService *alarmPublishingSettings);

    /// Initialize the alarm lines service
    void begin();

    /// Add a new alarm line to the system
    esp_err_t addAlarmLine(uint32_t id, String name, alarm_line_acquisition_t acquisition = ALA_GENIUS_PACKET, bool toFront = false);

    /// Increment packet sequence number and persist it
    uint8_t incPcktSeqNum();

    /// Save packet sequence number to NVS
    esp_err_t savePcktSeqNum();

    /// Load persisted packet sequence number from NVS
    esp_err_t loadPcktSeqNum();

private:
    // ========== Static Constants ==========
    static const uint8_t _packet_base_linetest[];  ///< Base packet template for line test transmissions
    static const uint8_t _packet_base_firealarm[]; ///< Base packet template for fire alarm transmissions
    uint8_t _packet_sequence_number;               ///< Current packet sequence number (persisted in NVS)

    // ========== Member Variables ==========
    // Framework and service instances
    ESP32SvelteKit *_sveltekit;               ///< Framework instance
    PsychicHttpServer *_server;               ///< HTTP server instance
    SecurityManager *_securityManager;        ///< Security management
    FeaturesService *_featureService;         ///< Feature flags service
    EventSocket *_eventSocket;                ///< WebSocket event system
    HttpEndpoint<AlarmLines> _httpEndpoint;   ///< REST API endpoint
    FSPersistence<AlarmLines> _fsPersistence; ///< File system persistence
    CC1101Controller *_cc1101Ctrl;            ///< RF controller instance

    // Task and synchronization
    TaskHandle_t _txTaskHandle;      ///< Transmission task handle
    SemaphoreHandle_t _txSemaphore;  ///< Transmission synchronization
    esp_timer_handle_t _timerHandle; ///< High precision timer handle

    // Transmission state
    volatile bool _isTransmitting;              ///< Current transmission status
    volatile uint32_t _transmissionTimeElapsed; ///< Elapsed transmission time
    volatile uint32_t _lastTXLoop;              ///< Last transmission loop timestamp

    // Transmission configuration
    uint32_t _txRepeat;                       ///< Number of transmission repetitions
    uint8_t _txBuffer[CC1101_MAX_PACKET_LEN]; ///< Transmission buffer
    size_t _txDataLength;                     ///< Current transmission data length
    float _packetCntStep;                     ///< Packet count increment step
    float _currentPacketCnt;                  ///< Current packet count value
    uint32_t _txPeriodUs;                     ///< Transmission period in microseconds

    // MQTT
    PsychicMqttClient *_mqttClient;                   ///< MQTT client instance (not owned)
    AlarmPublishingSettingsService *_alarmPublishingSettings; ///< Alarm-publishing settings service
    AlarmPublishingSettings _cachedAlarmPublishingSettings;   ///< Cached copy of alarm-publishing settings
    HAService *_haService;                            ///< HA service for sub-device management

    // Last action context
    uint32_t _lastActionLineId; ///< ID of the last triggered alarm line action
    String _lastActionType;     ///< Type of the last triggered action

    // ========== HA Sub-device tracking ==========
    /// Raw pointers into HADevice-owned entities (valid while sub-device is registered)
    struct AlarmLineHA
    {
        HADevice *device;   ///< Registered sub-device (owned by HAService)
        HASensor *txSensor; ///< Transmission state sensor (owned by device)
    };
    std::map<uint32_t, AlarmLineHA> _haDevices; ///< lineId → registered HA sub-device
    std::map<uint32_t, String> _txStates;       ///< lineId → last transmission state string

    // ========== Core Loops and Timing ==========
    /// Main monitoring loop for alarm line discovery
    void _monitorLoop();

    /// Transmission loop for RF packet sending
    void _txLoop();

    /// Static wrapper for transmission loop task
    static void _txLoopImpl(void *_this)
    {
        static_cast<AlarmLinesService *>(_this)->_txLoop();
    }

    /// Timer callback for transmission timing control
    void _onTimer();

    /// Static wrapper for timer callback
    static void _onTimerImpl(void *_this)
    {
        static_cast<AlarmLinesService *>(_this)->_onTimer();
    }

    // ========== Alarm Line Management ==========
    /// Check if alarm line with given ID already exists
    bool _alarmLineExists(uint32_t id);

    /// Remove alarm line from the system
    esp_err_t _removeAlarmLine(uint32_t id);

    // ========== Action Handling ==========
    /// Process HTTP action requests for alarm line operations
    esp_err_t _performAction(PsychicRequest *request, JsonVariant &json);

    /// Trigger an action programmatically (used by MQTT command handler)
    esp_err_t _triggerAction(uint32_t lineIdHostOrder, const String &action);

    // ========== Event Emission ==========
    /// Emit WebSocket event for new alarm line discovery
    void _emitNewAlarmLineEvent(uint32_t id);

    /// Emit WebSocket event for action start
    void _emitActionStartedEvent(uint32_t lineIdHostOrder, const String &action);

    /// Emit WebSocket event for action completion
    void _emitActionFinishedEvent(bool timedOut = false);

    // ========== MQTT / HA Publishing ==========

    /**
     * @brief Add a HA sub-device for a single alarm line (4 buttons + 1 sensor).
     *
     * Creates an HADevice with identity "genius-alarmline-{lineId}", registers 4
     * HAButton entities (line-test start/stop, fire-alarm start/stop) and one
     * HASensor for the current transmission state, then calls
     * HAService::addSubDevice(). Idempotent — no-op if the device is already
     * registered.
     *
     * @param lineId  Alarm line ID
     * @param lineName Human-readable line name (used as device name in HA)
     */
    void _addAlarmLineSubDevice(uint32_t lineId, const String &lineName);

    /**
     * @brief Remove the HA sub-device for a single alarm line.
     *
     * Calls HAService::removeSubDevice() which sends empty retained payloads so
     * HA removes the device from its registry, then erases the tracking entries.
     *
     * @param lineId  Alarm line ID
     */
    void _removeAlarmLineSubDevice(uint32_t lineId);

    /**
     * @brief Synchronize HA sub-devices with the current alarm line list.
     *
     * Processes pending deletions (from AlarmLines::deletedLineIds), then adds
     * sub-devices for any line not yet registered. Also handles line renames by
     * re-creating the sub-device.
     *
     * Called from the state update handler and once from begin().
     */
    void _syncAlarmLineSubDevices();

    /**
     * @brief Publish current transmission state for an alarm line.
     *
     * Updates _txStates and calls HASensor::publishState() if the sub-device is
     * registered and HA is ready.
     *
     * @param lineId  Alarm line ID
     * @param state   State string (default "Nothing")
     * @return ESP_OK on success, ESP_ERR_INVALID_STATE if HA not ready
     */
    esp_err_t _publishAlarmLineTransmissionState(uint32_t lineId, const String &state = "Nothing");

    /// Publish completion state for the last triggered action (resets to "Nothing")
    void _publishLastActionState(bool timedOut = false);

    // ========== Settings Management ==========
    /// Update cached alarm-publishing settings from AlarmPublishingSettingsService for faster access
    void _updateAlarmPublishingSettingsCache();
};
