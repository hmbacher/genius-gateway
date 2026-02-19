/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2025 theelims
 *   Copyright (C) 2026 hmbacher
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <HomeAssistant/HADiagnosticService.h>

HADiagnosticService::HADiagnosticService(HAService *haService)
    : _haService(haService),
      _diagnosticSensorTimer(nullptr)
{
}

void HADiagnosticService::begin()
{
    // Register with HAService for entity publishing on MQTT connect
    _haService->onPublishAll([this]()
                             { this->publishAll(); });

    // Create timer for periodic diagnostic sensor updates
    _diagnosticSensorTimer = xTimerCreate(
        "HADiagSensor",
        pdMS_TO_TICKS(DIAGNOSTIC_SENSOR_UPDATE_INTERVAL_MS),
        pdTRUE, // Auto-reload
        this,
        _diagnosticSensorTimerCallback);

    if (_diagnosticSensorTimer != nullptr)
    {
        xTimerStart(_diagnosticSensorTimer, 0);
        ESP_LOGI(TAG, "Diagnostic sensor timer started (%d minute interval)",
                 DIAGNOSTIC_SENSOR_UPDATE_INTERVAL_MS / (60 * 1000));
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create diagnostic sensor timer");
    }
}

// ============================================================================
// Entity publishing
// ============================================================================

void HADiagnosticService::publishAll()
{
    if (!_haService->isReady())
        return;

    _publishStatusSensor();
    _publishHeapSensor();
    _publishTempSensor();
    _publishDiagnosticSensorStates();
    _publishRestartButton();
    _subscribeToCommands();
}

void HADiagnosticService::_publishStatusSensor()
{
    JsonDocument config;
    config["name"] = "Status";
    config["unique_id"] = _haService->getDeviceId() + "_status";
    config["state_topic"] = "~/status/state";
    config["value_template"] = "{{value_json.state}}";
    config["icon"] = "mdi:heart-pulse";
    config["entity_category"] = "diagnostic";

    if (_haService->publishConfig("sensor", "status", config))
    {
        // Publish initial state
        String stateTopic = _haService->getBaseTopic() + "/status/state";
        JsonDocument state;
        state["state"] = "online";
        String payload;
        serializeJson(state, payload);
        _haService->publish(stateTopic, payload);

        ESP_LOGV(TAG, "Published status sensor");
    }
}

void HADiagnosticService::_publishHeapSensor()
{
    JsonDocument config;
    config["name"] = "Free Heap";
    config["unique_id"] = _haService->getDeviceId() + "_free_heap";
    config["state_topic"] = "~/diagnostics/state";
    config["value_template"] = "{{value_json.free_heap_percent|round(1)}}";
    config["unit_of_measurement"] = "%";
    config["state_class"] = "measurement";
    config["icon"] = "mdi:memory";
    config["entity_category"] = "diagnostic";

    if (_haService->publishConfig("sensor", "free_heap", config))
    {
        ESP_LOGV(TAG, "Published free heap sensor config");
    }
}

void HADiagnosticService::_publishTempSensor()
{
    JsonDocument config;
    config["name"] = "Core Temperature";
    config["unique_id"] = _haService->getDeviceId() + "_core_temp";
    config["state_topic"] = "~/diagnostics/state";
    config["value_template"] = "{{value_json.core_temp|round(1)}}";
    config["unit_of_measurement"] = "°C";
    config["device_class"] = "temperature";
    config["state_class"] = "measurement";
    config["icon"] = "mdi:thermometer";
    config["entity_category"] = "diagnostic";

    if (_haService->publishConfig("sensor", "core_temp", config))
    {
        ESP_LOGV(TAG, "Published core temperature sensor config");
    }
}

void HADiagnosticService::_publishDiagnosticSensorStates()
{
    if (!_haService->isReady())
        return;

    String stateTopic = _haService->getBaseTopic() + "/diagnostics/state";

    JsonDocument state;
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    float freeHeapPercent = (totalHeap > 0) ? (static_cast<float>(freeHeap) * 100.0f / static_cast<float>(totalHeap)) : 0.0f;
    state["free_heap_percent"] = freeHeapPercent;
    state["core_temp"] = temperatureRead();

    String payload;
    serializeJson(state, payload);

    if (_haService->publish(stateTopic, payload))
    {
        ESP_LOGV(TAG, "Published diagnostic sensor states: heap=%.1f%%, temp=%.1f",
                 freeHeapPercent, temperatureRead());
    }
    else
    {
        ESP_LOGW(TAG, "Failed to publish diagnostic sensor states");
    }
}

void HADiagnosticService::_publishRestartButton()
{
    JsonDocument config;
    config["name"] = "Restart";
    config["unique_id"] = _haService->getDeviceId() + "_restart";
    config["command_topic"] = "~/restart/command";
    config["payload_press"] = "PRESS";
    config["icon"] = "mdi:restart";
    // No entity_category - button should appear under Controls and in Overview

    if (_haService->publishConfig("button", "restart", config))
    {
        ESP_LOGV(TAG, "Published restart button config");
    }
}

// ============================================================================
// Command handlers
// ============================================================================

void HADiagnosticService::_subscribeToCommands()
{
    String baseTopic = _haService->getBaseTopic();
    String restartTopic = baseTopic + "/restart/command";

    _haService->subscribe(restartTopic,
                          [this](char *topic, char *payload, int retain, int qos, bool dup)
                          {
                              this->_onRestartCommand(topic, payload, retain, qos, dup);
                          });

    ESP_LOGV(TAG, "Subscribed to restart command");
}

void HADiagnosticService::_onRestartCommand(char *topic, char *payload, int retain, int qos, bool dup)
{
    ESP_LOGI(TAG, "Restart button pressed via MQTT - restarting in 1 second");
    vTaskDelay(pdMS_TO_TICKS(1000));
    RestartService::restartNow();
}

// ============================================================================
// Timer
// ============================================================================

void HADiagnosticService::_diagnosticSensorTimerCallback(TimerHandle_t timer)
{
    HADiagnosticService *service = static_cast<HADiagnosticService *>(pvTimerGetTimerID(timer));
    if (service != nullptr)
    {
        service->_publishDiagnosticSensorStates();
    }
}
