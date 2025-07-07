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

// GPIO to use for testing purposes
#define GPIO_TEST1 GPIO_NUM_21
#define GPIO_TEST2 GPIO_NUM_14

#define RX_TASK_STACK_SIZE 4096
#define RX_TASK_PRIORITY 20
#define RX_TASK_CORE_AFFINITY 1
#define RX_TASK_NAME "genius-rx"

#define HOPS_FIRST 0xF
#define HOPS_LAST 0x0

#define MIN_GENIUS_PACKET_LENGTH LEN_UNKNOWN_PURPOSE_1_PACKET

#define LEN_COMMISSIONING_PACKET 37
#define LEN_DISCOVERY_REQUEST_PACKET 28
#define LEN_DISCOVERY_RESPONSE_PACKET 32
#define LEN_ALARM_PACKET 36
#define LEN_LINE_TEST_PACKET 29

#define DATAPOS_GENERAL_ORIGIN_RADIO_MODULE_ID 9
#define DATAPOS_GENERAL_SENDER_RADIO_MODULE_ID 14
#define DATAPOS_GENERAL_LINE_ID 18
#define DATAPOS_GENERAL_HOPS 22
#define DATAPOS_COMISSIONING_NEW_LINE_ID 28
#define DATAPOS_COMOSSIONING_TIME_HOUR 32
#define DATAPOS_COMOSSIONING_TIME_MINUTE 33
#define DATAPOS_COMOSSIONING_TIME_SECOND 34
#define DATAPOS_ALARM_ACTIVE_FLAG 28
#define DATAPOS_ALARM_SILENCE_FLAG 30
#define DATAPOS_ALARM_SOURCE_SMOKE_ALARM_ID 32
#define DATAPOS_LINE_TEST_START_STOP_FLAG 28

#define EXTRACT32(buffer, pos) (__ntohl(*(uint32_t *)&buffer[pos]))
#define EXTRACT32_REV(buffer, pos) (*(uint32_t *)&buffer[pos])

#define GATEWAY_ID 0xFFFFFFFE

/**
 * Index within the target task's array of task notifications to use
 * NOTE: This value muss be LESS than the value of CONFIG_FREERTOS_TASK_NOTIFICATION_ARRAY_ENTRIES,
 * as RX_TASK_NOTIFICATION_INDEX is an 0-based index.
 */
#define RX_TASK_NOTIFICATION_INDEX 0

/**
 * Maximum ticks to wait for a packet to be received, before the blocking
 * task is unblocked to do some further checks or error handling.
 * NOTE: If the value is set to 'portMAX_DELAY', the task will block indefinitely.
 */
#define RX_TASK_MAX_WAITING_TICKS portMAX_DELAY

#define GATEWAY_EVENT_ALARM "alarm"

#define GATEWAY_ALARM_STATE_EMIT_INTERVAL_MS 1000 // 1 second

typedef enum genius_packet_type
{
  HPT_UNKNOWN = -1,       // Unknown packet type
  HPT_COMMISSIONING = 0,  // Commissioning packet (comissioning of smoke detectors to an alarm line)
  HPT_DISCOVERY_REQUEST,  // Discovery request packet (request for smoke detectors to self identify)
  HPT_DISCOVERY_RESPONSE, // Discovery response packet (response from smoke detectors with their IDs)
  HPT_ALARM_START,        // Alarm start packet (indicating smoke has been detected)
  HPT_ALARM_STOP,         // Alarm stop packet (indicating smoke has been cleared or alarm has been silenced)
  HPT_LINE_TEST_START,    // Line test start packet (indicating a line test has been started)
  HPT_LINE_TEST_STOP      // Line test stop packet (indicating a line test has been stopped)
} genius_packet_type_t;

typedef struct genius_packet_t
{
  genius_packet_type_t type;
  uint32_t origin_id;
  uint32_t sender_id;
  uint32_t line_id;
  uint8_t hops;
} genius_packet_t;

class GeniusGateway
{
public:
  GeniusGateway(ESP32SvelteKit *sveltekit);

  static TaskHandle_t xRxTaskHandle;

  void begin();

private:
  static constexpr const char *TAG = "GeniusGateway";

  GatewayDevicesService _gatewayDevices;
  AlarmLinesService _alarmLines;
  GatewaySettingsService _gatewaySettings;
  GatewayMqttSettingsService _gatewayMqttSettingsService;
  WSLogger _wsLogger;
  VisualizerSettingsService _visualizerSettingsService;
  CC1101Controller _cc1101Controller;
  PsychicMqttClient *_mqttClient;
  EventSocket *_eventSocket;
  FeaturesService *_featureService;

  void _mqttPublishDevices(bool onlyStates = false);

  void _rx_packets();
  static void _rx_packetsImpl(void *_this) { static_cast<GeniusGateway *>(_this)->_rx_packets(); }

  /**
   * @brief Analyze a received packet
   *
   * Analyze a received packet and fill the analyzed_packet struct with the results.
   *
   * @param packet_data Pointer to the received packet buffer
   * @param data_length Length of the received packet
   * @param analyzed_packet Pointer to the analyzed packet
   */
  esp_err_t _genius_analyze_packet_data(uint8_t *packet_data, size_t data_length, genius_packet_t *analyzed_packet);

  void _emitAlarmState();
};
