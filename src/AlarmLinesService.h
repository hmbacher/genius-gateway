/**
 * @file AlarmLinesService.h
 * @brief Alarm Lines service for managing genius alarm line communication
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

#define ALARMLINES_LINETEST_PCKTCNT_STEP (float)(ALARMLINES_LINETEST_FIRST_PCKTCNT - ALARMLINES_LINETEST_LAST_PCKTCNT) / (float)(ALARMLINES_TX_NUM_REPEAT_LINETEST - 1)  ///< Packet count step size calculation for line test

#define ALARMLINES_TX_PERIOD_FIREALARM_US 9855  ///< Transmission period for fire alarm packets in microseconds (9.855 ms)
#define ALARMLINES_TX_NUM_REPEAT_FIREALARM 315  ///< Number of fire alarm packet repetitions
#define ALARMLINES_FIREALARM_FIRST_PCKTCNT 0x18CC  ///< First packet count value for fire alarm sequence
#define ALARMLINES_FIREALARM_LAST_PCKTCNT 0x000A  ///< Last packet count value for fire alarm sequence
#define ALARMLINES_FIREALARM_PCKTCNT_STEP (float)(ALARMLINES_FIREALARM_FIRST_PCKTCNT - ALARMLINES_FIREALARM_LAST_PCKTCNT) / (float)(ALARMLINES_TX_NUM_REPEAT_FIREALARM - 1)  ///< Packet count step size calculation for fire alarm

#define ALARMLINES_EVENT_NEW_LINE "new-alarm-line"  ///< WebSocket event for new alarm line discovery
#define ALARMLINES_EVENT_ACTION_FINISHED "alarm-line-action-finished"  ///< WebSocket event for action completion notification

#define ALARMLINES_NVS_NAMESPACE "gg-alarmlines"  ///< NVS namespace for alarm lines data storage
#define ALARMLINES_NVS_SEQ_KEY "pkt_seq_num"  ///< NVS key for packet sequence number storage
#define ALARMLINES_NVS_SEQ_DEFAULT 0  ///< Default packet sequence number value

/// Enumeration for alarm line acquisition methods
typedef enum alarm_line_acquisition
{
    ALA_MIN = -1,      ///< Boundary check minimum value
    ALA_BUILT_IN = 0,  ///< Built-in alarm line (e.g. broadcast line)
    ALA_GENIUS_PACKET, ///< Discovered via received genius packet
    ALA_MANUAL,        ///< Manually added via web interface
    ALA_MAX            ///< Boundary check maximum value
} alarm_line_acquisition_t;

/// Structure representing a genius alarm line
typedef struct genius_alarm_line
{
    uint32_t id;                          ///< Unique alarm line ID (0xFFFFFFFF = broadcast, 0x00000000 = none)
    String name;                          ///< Human-readable alarm line name
    time_t created;                       ///< Creation timestamp (Unix epoch)
    alarm_line_acquisition_t acquisition; ///< How this line was discovered/added
} genius_alarm_line_t;

/// Data model class for managing alarm line collections
class AlarmLines
{
public:
    static constexpr const char *TAG = "AlarmLines"; ///< Logging tag
    std::vector<genius_alarm_line_t> lines;          ///< Vector containing all managed alarm lines

    /// Deserialize alarm lines from JSON object
    static void read(AlarmLines &alarmLines, JsonObject &root);

    /// Serialize alarm lines to JSON object
    static JsonObject &write(JsonObject &root, AlarmLines &alarmLines)
    {
        JsonArray jsonDevices = root["lines"].to<JsonArray>();
        for (auto &line : alarmLines.lines)
        {
            JsonObject jsonLine = jsonDevices.add<JsonObject>();
            jsonLine["id"] = line.id;
            jsonLine["name"] = line.name;
            jsonLine["created"] = Utils::time_t_to_iso8601(line.created);
            jsonLine["acquisition"] = line.acquisition;
        }

        ESP_LOGV(AlarmLines::TAG, "Alarm lines configurations read.");
    }

    /// Update alarm lines from JSON object
    static StateUpdateResult update(JsonObject &root, AlarmLines &alarmLines)
    {
        if (root["lines"].is<JsonArray>())
        {
            alarmLines.lines.clear();

            // iterate over devices
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
                newLine.name = jsonLine["name"].as<String>();
                newLine.created = Utils::iso8601_to_time_t(jsonLine["created"].as<String>());
                newLine.acquisition = jsonLine["acquisition"].as<alarm_line_acquisition_t>();

                alarmLines.lines.push_back(newLine);

                ESP_LOGV(AlarmLines::TAG, "Added alarm line: %s", newLine.name.c_str());

                i++;
            }
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

    AlarmLinesService(ESP32SvelteKit *sveltekit, CC1101Controller *cc1101Ctrl);

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
    static const uint8_t _packet_base_linetest[];  ///< Base packet template for line test transmissions
    static const uint8_t _packet_base_firealarm[]; ///< Base packet template for fire alarm transmissions
    uint8_t _packet_sequence_number;               ///< Current packet sequence number (persisted in NVS)

    ESP32SvelteKit *_sveltekit;               ///< Framework instance
    PsychicHttpServer *_server;               ///< HTTP server instance
    SecurityManager *_securityManager;        ///< Security management
    FeaturesService *_featureService;         ///< Feature flags service
    EventSocket *_eventSocket;                ///< WebSocket event system
    HttpEndpoint<AlarmLines> _httpEndpoint;   ///< REST API endpoint
    FSPersistence<AlarmLines> _fsPersistence; ///< File system persistence
    CC1101Controller *_cc1101Ctrl;            ///< RF controller instance

    TaskHandle_t _txTaskHandle;      ///< Transmission task handle
    SemaphoreHandle_t _txSemaphore;  ///< Transmission synchronization
    esp_timer_handle_t _timerHandle; ///< High precision timer handle

    volatile bool _isTransmitting;              ///< Current transmission status
    volatile uint32_t _transmissionTimeElapsed; ///< Elapsed transmission time
    volatile uint32_t _lastTXLoop;              ///< Last transmission loop timestamp

    uint32_t _txRepeat;                       ///< Number of transmission repetitions
    uint8_t _txBuffer[CC1101_MAX_PACKET_LEN]; ///< Transmission buffer
    size_t _txDataLength;                     ///< Current transmission data length
    float _packetCntStep;                     ///< Packet count increment step
    float _currentPacketCnt;                  ///< Current packet count value
    uint32_t _txPeriodUs;                     ///< Transmission period in microseconds

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

    /// Check if alarm line with given ID already exists
    bool _alarmLineExists(uint32_t id);

    /// Remove alarm line from the system
    esp_err_t _removeAlarmLine(uint32_t id);

    /// Process HTTP action requests for alarm line operations
    esp_err_t _performAction(PsychicRequest *request, JsonVariant &json);

    /// Emit WebSocket event for new alarm line discovery
    void _emitNewAlarmLineEvent(uint32_t id);

    /// Emit WebSocket event for action completion
    void _emitActionFinishedEvent(bool timedOut = false);
};
