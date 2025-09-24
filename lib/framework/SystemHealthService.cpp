/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <SystemHealthService.h>

SystemHealthService::SystemHealthService(EventSocket *socket) : _socket(socket), _lastHealthUpdate(0)
{
    _bootTime = millis();
    _lastResetReason = esp_reset_reason();
}

void SystemHealthService::begin()
{
    _socket->registerEvent(EVENT_SYSTEM_HEALTH);
    ESP_LOGI("SystemHealth", "System Health Service initialized - Boot time: %lu ms, Reset reason: %s", 
             _bootTime, getResetReasonString((esp_reset_reason_t)_lastResetReason).c_str());
}

void SystemHealthService::loop()
{
    unsigned long currentMillis = millis();
    
    if (!_lastHealthUpdate || (unsigned long)(currentMillis - _lastHealthUpdate) >= SYSTEM_HEALTH_EVENT_DELAY)
    {
        _lastHealthUpdate = currentMillis;
        updateSystemHealth();
        
        // Log detailed task info every 30 seconds
        static unsigned long lastTaskLog = 0;
        if (currentMillis - lastTaskLog >= 30000) {
            lastTaskLog = currentMillis;
            logTaskInfo();
        }
    }
}

void SystemHealthService::updateSystemHealth()
{
    JsonDocument doc;
    
    // Basic system info
    doc["uptime"] = millis();
    doc["boot_time"] = _bootTime;
    doc["reset_reason"] = getResetReasonString((esp_reset_reason_t)_lastResetReason);
    
    // Memory information
    doc["heap_free"] = esp_get_free_heap_size();
    doc["heap_min"] = esp_get_minimum_free_heap_size();
    doc["heap_total"] = ESP.getHeapSize();
    doc["heap_usage_percent"] = (float)(ESP.getHeapSize() - esp_get_free_heap_size()) / ESP.getHeapSize() * 100.0;
    
    // PSRAM information (if available)
    if (ESP.getPsramSize() > 0) {
        doc["psram_total"] = ESP.getPsramSize();
        doc["psram_free"] = ESP.getFreePsram();
        doc["psram_usage_percent"] = (float)(ESP.getPsramSize() - ESP.getFreePsram()) / ESP.getPsramSize() * 100.0;
    } else {
        doc["psram_total"] = 0;
        doc["psram_free"] = 0;
        doc["psram_usage_percent"] = 0;
    }
    
    // CPU information
    doc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
    doc["core_count"] = ESP.getChipCores();
    doc["chip_model"] = ESP.getChipModel();
    doc["chip_revision"] = ESP.getChipRevision();
    
    // Flash information
    doc["flash_size"] = ESP.getFlashChipSize();
    doc["flash_speed"] = ESP.getFlashChipSpeed();
    doc["sketch_size"] = ESP.getSketchSize();
    doc["free_sketch_space"] = ESP.getFreeSketchSpace();
    doc["sketch_usage_percent"] = (float)ESP.getSketchSize() / (ESP.getSketchSize() + ESP.getFreeSketchSpace()) * 100.0;
    
    // Task information
    doc["task_count"] = uxTaskGetNumberOfTasks();
    
    // WiFi connection stability (if connected)
    if (WiFi.isConnected()) {
        doc["wifi_connected"] = true;
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["wifi_channel"] = WiFi.channel();
        doc["wifi_tx_power"] = WiFi.getTxPower();
    } else {
        doc["wifi_connected"] = false;
        doc["wifi_rssi"] = 0;
        doc["wifi_channel"] = 0;
        doc["wifi_tx_power"] = 0;
    }
    
    // System temperature (if available)
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
    doc["temperature_celsius"] = temperatureRead();
#else
    doc["temperature_celsius"] = 0;
#endif
    
    // Calculate health score (0-100)
    float healthScore = 100.0;
    
    // Reduce score for high memory usage
    float heapUsage = (float)(ESP.getHeapSize() - esp_get_free_heap_size()) / ESP.getHeapSize() * 100.0;
    if (heapUsage > 90) healthScore -= 30;
    else if (heapUsage > 80) healthScore -= 20;
    else if (heapUsage > 70) healthScore -= 10;
    
    // Reduce score for poor WiFi signal
    if (WiFi.isConnected()) {
        int rssi = WiFi.RSSI();
        if (rssi < -80) healthScore -= 20;
        else if (rssi < -70) healthScore -= 10;
        else if (rssi < -60) healthScore -= 5;
    } else {
        healthScore -= 25;  // No WiFi connection
    }
    
    // Reduce score for low free heap
    if (esp_get_free_heap_size() < 50000) healthScore -= 20;
    else if (esp_get_free_heap_size() < 100000) healthScore -= 10;
    
    doc["health_score"] = (int)healthScore;
    
    // Health status
    String status;
    if (healthScore >= 90) status = "excellent";
    else if (healthScore >= 80) status = "good";
    else if (healthScore >= 60) status = "fair";
    else if (healthScore >= 40) status = "poor";
    else status = "critical";
    
    doc["health_status"] = status;
    
    ESP_LOGD("SystemHealth", "Health Update - Score: %d (%s), Heap: %.1f%%, RSSI: %d dBm", 
             (int)healthScore, status.c_str(), heapUsage, WiFi.isConnected() ? WiFi.RSSI() : 0);
    
    JsonObject jsonObject = doc.as<JsonObject>();
    _socket->emitEvent(EVENT_SYSTEM_HEALTH, jsonObject);
}

String SystemHealthService::getResetReasonString(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_UNKNOWN: return "UNKNOWN";
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXTERNAL";
        case ESP_RST_SW: return "SOFTWARE";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNDEFINED";
    }
}

void SystemHealthService::logTaskInfo()
{
    size_t taskCount = uxTaskGetNumberOfTasks();
    TaskStatus_t *taskArray = (TaskStatus_t*)malloc(taskCount * sizeof(TaskStatus_t));
    
    if (taskArray != NULL) {
        UBaseType_t actualTaskCount = uxTaskGetSystemState(taskArray, taskCount, NULL);
        
        ESP_LOGI("SystemHealth", "=== Task Information ===");
        ESP_LOGI("SystemHealth", "Total tasks: %d", actualTaskCount);
        
        for (UBaseType_t i = 0; i < actualTaskCount; i++) {
            ESP_LOGI("SystemHealth", "Task[%d]: %s - Priority: %d, State: %d, Stack HWM: %d", 
                     i, taskArray[i].pcTaskName, taskArray[i].uxCurrentPriority, 
                     taskArray[i].eCurrentState, taskArray[i].usStackHighWaterMark);
        }
        
        free(taskArray);
    } else {
        ESP_LOGW("SystemHealth", "Failed to allocate memory for task info");
    }
}