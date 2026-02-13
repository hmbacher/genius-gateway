/**
 * @file GatewayDeviceMqttService.cpp
 * @brief Implementation of Gateway Device MQTT service
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

#include <GatewayDeviceMqttService.h>
#include <ESP32SvelteKit.h>

// Static member initialization
#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
SemaphoreHandle_t GatewayDeviceMqttService::_otaUpdateMutex = nullptr;
GatewayDeviceMqttService *GatewayDeviceMqttService::_activeOtaInstance = nullptr;
#endif

GatewayDeviceMqttService::GatewayDeviceMqttService(PsychicMqttClient *mqttClient,
                                                   GatewayMqttSettingsService *mqttSettingsService,
                                                   GatewaySettingsService *gatewaySettingsService,
                                                   DownloadFirmwareService *downloadFirmwareService,
                                                   EventSocket *eventSocket)
    : _mqttClient(mqttClient),
      _mqttSettingsService(mqttSettingsService),
      _gatewaySettingsService(gatewaySettingsService),
      _downloadFirmwareService(downloadFirmwareService),
      _eventSocket(eventSocket),
      _diagnosticSensorTimer(nullptr)
#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
      ,_updateAvailable(false),
      _updateCheckTimer(nullptr),
      _initialUpdateCheckDone(false),
      _updateCheckPending(false),
      _updateCheckCounter(0)
#endif
{
#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    // Create mutex for OTA updates (only once)
    if (_otaUpdateMutex == nullptr)
    {
        _otaUpdateMutex = xSemaphoreCreateMutex();
    }
#endif
}

void GatewayDeviceMqttService::begin()
{
    // Initialize cached MQTT settings
    _updateMqttSettingsCache();

    // Initialize cached gateway settings
    _updateGatewaySettingsCache();

    // Update gateway device ID (use unique_id without colons for MQTT topic compatibility)
    _gatewayDeviceId = "genius-gateway-" + SettingValue::getUniqueId();

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    // Create persistent task for update checks (single 8KB stack, reused)
    xTaskCreate(
        _updateCheckTask,
        "update_check",
        8192, // 8KB stack for HTTPS/TLS
        this,
        1, // Low priority
        &_updateCheckTaskHandle
    );
    
    if (_updateCheckTaskHandle != nullptr)
    {
        ESP_LOGI(TAG, "Update check task created");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create update check task");
    }

    // Register WiFi event handler for initial update check
    WiFi.onEvent(
        std::bind(&GatewayDeviceMqttService::_onWiFiGotIP, this, std::placeholders::_1, std::placeholders::_2),
        WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    
    // Create timer for periodic update checks (counter-based for long intervals)
    _updateCheckTimer = xTimerCreate(
        "UpdateCheck",
        pdMS_TO_TICKS(UPDATE_CHECK_TIMER_INTERVAL_MS),
        pdTRUE, // Auto-reload
        this,   // Timer ID (pass this pointer)
        _updateCheckTimerCallback
    );

    if (_updateCheckTimer != nullptr)
    {
        xTimerStart(_updateCheckTimer, 0);
        ESP_LOGI(TAG, "Update check timer started (%d min intervals, check every %d callbacks = %lu hours)", 
                 UPDATE_CHECK_TIMER_INTERVAL_MS / (60 * 1000), UPDATE_CHECK_COUNTER_TARGET, (UPDATE_CHECK_TIMER_INTERVAL_MS * UPDATE_CHECK_COUNTER_TARGET) / (60 * 60 * 1000));
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create update check timer");
    }
#endif

    // Create timer for periodic diagnostic sensor updates
    _diagnosticSensorTimer = xTimerCreate(
        "DiagSensors",
        pdMS_TO_TICKS(DIAGNOSTIC_SENSOR_UPDATE_INTERVAL_MS),
        pdTRUE, // Auto-reload
        this,   // Timer ID (pass this pointer)
        _diagnosticSensorTimerCallback
    );

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

    // Update cache when MQTT settings change
    if (_mqttSettingsService != nullptr)
    {
        _mqttSettingsService->addUpdateHandler([this](const String &originId)
                                               {
                                                   this->_updateMqttSettingsCache();
                                                   // Republish on settings change
                                                   if (this->_mqttClient->connected())
                                                   {
                                                       this->publishAll();
                                                   } },
                                               false);
    }

    // Update cache and republish states when gateway settings change
    if (_gatewaySettingsService != nullptr)
    {
        _gatewaySettingsService->addUpdateHandler([this](const String &originId)
                                                  {
                                                      this->_updateGatewaySettingsCache();
                                                      // Only republish switch states, not full config
                                                      if (this->_mqttClient->connected())
                                                      {
                                                          this->_publishSettingSwitchStates();
                                                      } },
                                                  false);
    }
}

void GatewayDeviceMqttService::publishAll()
{
    if (_mqttClient == nullptr || !_mqttClient->connected() || _mqttSettingsService == nullptr)
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled || _cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        return;

    ESP_LOGI(TAG, "Publishing Genius Gateway device and entities to Home Assistant");

    // Publish gateway device (via diagnostic sensor)
    _publishGatewayDevice();

    // Publish diagnostic sensors
    _publishHeapSensor();
    _publishTempSensor();

    // Publish initial diagnostic sensor states
    _publishDiagnosticSensorStates();

    // Publish restart button
    _publishRestartButton();

    // Publish setting switches
    _publishSettingSwitches();

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    // Publish update entity
    _publishUpdateEntity();
#endif

    // Subscribe to command topics
    _subscribeToCommands();

    ESP_LOGI(TAG, "Gateway device publishing complete");
}

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
void GatewayDeviceMqttService::checkForUpdates()
{
    // Only check if WiFi is connected
    if (WiFi.status() != WL_CONNECTED)
    {
        ESP_LOGW(TAG, "Skipping update check - WiFi not connected");
        return;
    }
    
    ESP_LOGI(TAG, "Checking for firmware updates...");
    
    if (_queryGitHubForLatestRelease())
    {
        // Check if we're connected to MQTT and should publish state
        if (_mqttClient != nullptr && _mqttClient->connected() && 
            _cachedMqttSettings.HAIntegrationEnabled && 
            !_cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        {
            _publishUpdateState();
        }
    }
}
#endif

String GatewayDeviceMqttService::getGatewayDeviceId()
{
    return _gatewayDeviceId;
}

String GatewayDeviceMqttService::getGatewayIdentifier()
{
    return "genius-gateway-" + SettingValue::getUniqueId();
}

// ============================================================================
// Private Methods - MQTT Settings
// ============================================================================

void GatewayDeviceMqttService::_updateMqttSettingsCache()
{
    if (_mqttSettingsService != nullptr)
    {
        _cachedMqttSettings = _mqttSettingsService->getSettingsCopy();
        ESP_LOGV(TAG, "Updated cached MQTT settings (enabled: %d, prefix: %s)",
                 _cachedMqttSettings.HAIntegrationEnabled,
                 _cachedMqttSettings.HAMQTTDiscoveryPrefix.c_str());
    }
}

void GatewayDeviceMqttService::_updateGatewaySettingsCache()
{
    if (_gatewaySettingsService != nullptr)
    {
        _gatewaySettingsService->read([this](GatewaySettings &settings) {
            _cachedGatewaySettings = settings;
        });
        ESP_LOGV(TAG, "Updated cached gateway settings");
    }
}

// ============================================================================
// Private Methods - Device Publishing
// ============================================================================

void GatewayDeviceMqttService::_publishGatewayDevice()
{
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    
    // Publish a diagnostic sensor to register the device
    String configTopic = discoveryPrefix + "sensor/" + _gatewayDeviceId + "/status/config";
    
    JsonDocument config;
    config["~"] = baseTopic;
    config["name"] = "Status";
    config["unique_id"] = _gatewayDeviceId + "_status";
    config["state_topic"] = "~/status/state";
    config["value_template"] = "{{value_json.state}}";
    config["icon"] = "mdi:heart-pulse";
    config["entity_category"] = "diagnostic";
    
    _addGatewayDeviceInfo(config);
    
    String payload;
    serializeJson(config, payload);
    
    if (_mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str()) != -1)
    {
        ESP_LOGV(TAG, "Published gateway device config");
        
        // Publish state
        String stateTopic = baseTopic + "/status/state";
        JsonDocument state;
        state["state"] = "online";
        String statePayload;
        serializeJson(state, statePayload);
        _mqttClient->publish(stateTopic.c_str(), 0, true, statePayload.c_str());
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish gateway device config");
    }
}

void GatewayDeviceMqttService::_publishHeapSensor()
{
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    
    String configTopic = discoveryPrefix + "sensor/" + _gatewayDeviceId + "/free_heap/config";
    
    JsonDocument config;
    config["~"] = baseTopic;
    config["name"] = "Free Heap";
    config["unique_id"] = _gatewayDeviceId + "_free_heap";
    config["state_topic"] = "~/diagnostics/state";
    config["value_template"] = "{{value_json.free_heap_percent|round(1)}}";
    config["unit_of_measurement"] = "%";
    config["state_class"] = "measurement";
    config["icon"] = "mdi:memory";
    config["entity_category"] = "diagnostic";
    
    _addGatewayDeviceInfo(config);
    
    String payload;
    serializeJson(config, payload);
    
    if (_mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str()) != -1)
    {
        ESP_LOGV(TAG, "Published free heap sensor config");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish free heap sensor config");
    }
}

void GatewayDeviceMqttService::_publishTempSensor()
{
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    
    String configTopic = discoveryPrefix + "sensor/" + _gatewayDeviceId + "/core_temp/config";
    
    JsonDocument config;
    config["~"] = baseTopic;
    config["name"] = "Core Temperature";
    config["unique_id"] = _gatewayDeviceId + "_core_temp";
    config["state_topic"] = "~/diagnostics/state";
    config["value_template"] = "{{value_json.core_temp|round(1)}}";
    config["unit_of_measurement"] = "°C";
    config["device_class"] = "temperature";
    config["state_class"] = "measurement";
    config["icon"] = "mdi:thermometer";
    config["entity_category"] = "diagnostic";
    
    _addGatewayDeviceInfo(config);
    
    String payload;
    serializeJson(config, payload);
    
    if (_mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str()) != -1)
    {
        ESP_LOGV(TAG, "Published core temperature sensor config");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish core temperature sensor config");
    }
}

void GatewayDeviceMqttService::_publishDiagnosticSensorStates()
{
    if (_mqttClient == nullptr || !_mqttClient->connected())
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled || _cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        return;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    String stateTopic = baseTopic + "/diagnostics/state";
    
    JsonDocument state;
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    float freeHeapPercent = (totalHeap > 0) ? (static_cast<float>(freeHeap) * 100.0f / static_cast<float>(totalHeap)) : 0.0f;
    state["free_heap_percent"] = freeHeapPercent;
    state["core_temp"] = temperatureRead();
    
    String statePayload;
    serializeJson(state, statePayload);
    
    if (_mqttClient->publish(stateTopic.c_str(), 0, false, statePayload.c_str()) != -1)
    {
        ESP_LOGV(TAG, "Published diagnostic sensor states: heap=%.1f%%, temp=%.1f", 
                 freeHeapPercent, temperatureRead());
    }
    else
    {
        ESP_LOGW(TAG, "Failed to publish diagnostic sensor states");
    }
}

void GatewayDeviceMqttService::_diagnosticSensorTimerCallback(TimerHandle_t timer)
{
    GatewayDeviceMqttService *service = static_cast<GatewayDeviceMqttService *>(pvTimerGetTimerID(timer));
    if (service != nullptr)
    {
        service->_publishDiagnosticSensorStates();
    }
}

void GatewayDeviceMqttService::_publishRestartButton()
{
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    
    String configTopic = discoveryPrefix + "button/" + _gatewayDeviceId + "/restart/config";
    
    JsonDocument config;
    config["~"] = baseTopic;
    config["name"] = "Restart";
    config["unique_id"] = _gatewayDeviceId + "_restart";
    config["command_topic"] = "~/restart/command";
    config["payload_press"] = "PRESS";
    config["icon"] = "mdi:restart";
    config["entity_category"] = "config";
    
    _addGatewayDeviceInfo(config);
    
    String payload;
    serializeJson(config, payload);
    
    if (_mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str()) != -1)
    {
        ESP_LOGV(TAG, "Published restart button config");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish restart button config");
    }
}

void GatewayDeviceMqttService::_publishSettingSwitches()
{
    if (_gatewaySettingsService == nullptr)
        return;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    
    // Define switch configurations using shared JSON state topic
    struct SwitchConfig {
        const char* suffix;
        const char* name;
        const char* icon;
        const char* jsonKey;
    };
    
    SwitchConfig switches[] = {
        {"alert_unknown", "Alert on Unknown Detectors", "mdi:toggle-switch-off-outline", "alert_unknown"},
        {"line_commissioning", "Add Line from Commissioning", "mdi:toggle-switch-off-outline", "line_commissioning"},
        {"line_alarm", "Add Line from Alarm", "mdi:toggle-switch-off-outline", "line_alarm"},
        {"line_test", "Add Line from Line Test", "mdi:toggle-switch-off-outline", "line_test"}
    };
    
    for (const auto& sw : switches)
    {
        String configTopic = discoveryPrefix + "switch/" + _gatewayDeviceId + "/" + sw.suffix + "/config";
        
        JsonDocument config;
        config["~"] = baseTopic;
        config["name"] = sw.name;
        config["unique_id"] = _gatewayDeviceId + "_" + sw.suffix;
        config["state_topic"] = "~/gateway/state";
        config["value_template"] = "{{ value_json." + String(sw.jsonKey) + " }}";
        config["command_topic"] = "~/gateway/switch/" + String(sw.suffix) + "/set";
        config["payload_on"] = "ON";
        config["payload_off"] = "OFF";
        config["state_on"] = "ON";
        config["state_off"] = "OFF";
        config["icon"] = sw.icon;
        config["entity_category"] = "config";
        
        _addGatewayDeviceInfo(config);
        
        String payload;
        serializeJson(config, payload);
        
        if (_mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str()) != -1)
        {
            ESP_LOGV(TAG, "Published switch config: %s", sw.name);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to publish switch config: %s", sw.name);
        }
    }
    
    // Publish initial combined state
    _publishSettingSwitchStates();
}

void GatewayDeviceMqttService::_publishSettingSwitchStates()
{
    if (_gatewaySettingsService == nullptr || _mqttClient == nullptr || !_mqttClient->connected())
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled || _cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        return;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    
    // Create single JSON state message with all switch states
    JsonDocument stateDoc;
    stateDoc["alert_unknown"] = _cachedGatewaySettings.alertOnUnknownDetectors ? "ON" : "OFF";
    stateDoc["line_commissioning"] = _cachedGatewaySettings.addALarmLineFromCommissioningPacket ? "ON" : "OFF";
    stateDoc["line_alarm"] = _cachedGatewaySettings.addAlarmLineFromAlarmPacket ? "ON" : "OFF";
    stateDoc["line_test"] = _cachedGatewaySettings.addAlarmLineFromLineTestPacket ? "ON" : "OFF";
    
    String statePayload;
    serializeJson(stateDoc, statePayload);
    
    String stateTopic = baseTopic + "/gateway/state";
    if (_mqttClient->publish(stateTopic.c_str(), 0, true, statePayload.c_str()) != -1)
    {
        ESP_LOGV(TAG, "Published combined switch states: %s", statePayload.c_str());
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish switch states");
    }
}

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
void GatewayDeviceMqttService::_publishUpdateEntity()
{
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    
    String configTopic = discoveryPrefix + "update/" + _gatewayDeviceId + "/firmware/config";
    
    JsonDocument config;
    config["~"] = baseTopic;
    config["name"] = "Firmware Update";
    config["unique_id"] = _gatewayDeviceId + "_firmware_update";
    config["state_topic"] = "~/update/state";
    config["command_topic"] = "~/update/install";
    config["payload_install"] = "INSTALL";
    config["title"] = "Genius Gateway Firmware";
    config["device_class"] = "firmware";
    config["entity_category"] = "config";
    config["icon"] = "mdi:cloud-download";
    
    _addGatewayDeviceInfo(config);
    
    String payload;
    serializeJson(config, payload);
    
    if (_mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str()) != -1)
    {
        ESP_LOGI(TAG, "Published update entity config (state will be published after GitHub check)");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish update entity config");
    }
}

void GatewayDeviceMqttService::_publishUpdateState()
{
    if (_mqttClient == nullptr || !_mqttClient->connected())
        return;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    String stateTopic = baseTopic + "/update/state";
    
    JsonDocument state;
    
    // Current installed version
    state["installed_version"] = String(APP_VERSION);
    
    // Latest version from GitHub (or current version if not checked yet)
    String latestVer = !_latestVersion.isEmpty() ? _latestVersion : String(APP_VERSION);
    state["latest_version"] = latestVer;
    state["title"] = "Genius Gateway Firmware " + latestVer;
    state["release_url"] = "https://github.com/hmbacher/genius-gateway/releases/latest";
    
    if (_updateAvailable)
    {
        ESP_LOGI(TAG, "Update available: %s -> %s", APP_VERSION, _latestVersion.c_str());
    }
    else
    {
        ESP_LOGV(TAG, "No update available (installed: %s, latest: %s)", APP_VERSION, latestVer.c_str());
    }
    
    String payload;
    serializeJson(state, payload);
    
    _mqttClient->publish(stateTopic.c_str(), 0, true, payload.c_str());
    ESP_LOGV(TAG, "Published update state");
}
#endif

// ============================================================================
// Private Methods - MQTT Subscriptions
// ============================================================================

void GatewayDeviceMqttService::_subscribeToCommands()
{
    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    
    // Subscribe to restart button
    String restartCmdTopic = baseTopic + "/restart/command";
    _mqttClient->onTopic(restartCmdTopic.c_str(), 0, 
        [this](char *topic, char *payload, int retain, int qos, bool dup)
        {
            this->_onRestartCommand(topic, payload, retain, qos, dup);
        });
    
    // Subscribe to setting switches
    struct SwitchSubscription {
        const char* suffix;
        const char* name;
    };
    
    SwitchSubscription switches[] = {
        {"alert_unknown", "alert_on_unknown_detectors"},
        {"line_commissioning", "add_alarm_line_from_commissioning_packet"},
        {"line_alarm", "add_alarm_line_from_alarm_packet"},
        {"line_test", "add_alarm_line_from_line_test_packet"}
    };
    
    for (const auto& sw : switches)
    {
        String cmdTopic = baseTopic + "/gateway/switch/" + sw.suffix + "/set";
        _mqttClient->onTopic(cmdTopic.c_str(), 0, 
            [this, settingName = String(sw.name)](char *topic, char *payload, int retain, int qos, bool dup)
            {
                this->_onSettingSwitchCommand(settingName.c_str(), topic, payload, retain, qos, dup);
            });
    }
    
#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    // Subscribe to update install command
    String updateInstallTopic = baseTopic + "/update/install";
    _mqttClient->onTopic(updateInstallTopic.c_str(), 0, 
        [this](char *topic, char *payload, int retain, int qos, bool dup)
        {
            this->_onUpdateInstallCommand(topic, payload, retain, qos, dup);
        });
#endif
    
    ESP_LOGI(TAG, "Subscribed to command topics");
}

void GatewayDeviceMqttService::_onRestartCommand(char *topic, char *payload, int retain, int qos, bool dup)
{
    ESP_LOGI(TAG, "Restart button pressed via MQTT - restarting in 1 second");
    
    // Give time for MQTT ACK to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RestartService::restartNow();
}

void GatewayDeviceMqttService::_onSettingSwitchCommand(const char *settingName, char *topic, char *payload, int retain, int qos, bool dup)
{
    if (_gatewaySettingsService == nullptr || payload == nullptr)
        return;
    
    // Parse payload
    String payloadStr = String(payload);
    bool newValue = (payloadStr == "ON");
    
    ESP_LOGI(TAG, "Received switch command for '%s': %s", settingName, newValue ? "ON" : "OFF");
    
    // Update gateway settings via StatefulService
    // This will automatically trigger all update handlers including EventEndpoint for WebSocket sync
    _gatewaySettingsService->update([settingName, newValue](GatewaySettings &settings)
                                    {
                                        String settingNameStr = String(settingName);
                                        bool changed = false;
                                        
                                        if (settingNameStr == "alert_on_unknown_detectors")
                                        {
                                            if (settings.alertOnUnknownDetectors != newValue)
                                            {
                                                settings.alertOnUnknownDetectors = newValue;
                                                changed = true;
                                            }
                                        }
                                        else if (settingNameStr == "add_alarm_line_from_commissioning_packet")
                                        {
                                            if (settings.addALarmLineFromCommissioningPacket != newValue)
                                            {
                                                settings.addALarmLineFromCommissioningPacket = newValue;
                                                changed = true;
                                            }
                                        }
                                        else if (settingNameStr == "add_alarm_line_from_alarm_packet")
                                        {
                                            if (settings.addAlarmLineFromAlarmPacket != newValue)
                                            {
                                                settings.addAlarmLineFromAlarmPacket = newValue;
                                                changed = true;
                                            }
                                        }
                                        else if (settingNameStr == "add_alarm_line_from_line_test_packet")
                                        {
                                            if (settings.addAlarmLineFromLineTestPacket != newValue)
                                            {
                                                settings.addAlarmLineFromLineTestPacket = newValue;
                                                changed = true;
                                            }
                                        }
                                        
                                        return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
                                    }, 
                                    "mqtt");
}

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
void GatewayDeviceMqttService::_onUpdateInstallCommand(char *topic, char *payload, int retain, int qos, bool dup)
{
    if (!_updateAvailable || _downloadUrl.isEmpty())
    {
        ESP_LOGW(TAG, "Update install requested but no update is available");
        return;
    }
    
    // Try to acquire OTA mutex
    if (xSemaphoreTake(_otaUpdateMutex, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "OTA update already in progress (via Web Interface or other MQTT client)");
        return;
    }
    
    ESP_LOGI(TAG, "Update install requested via MQTT - starting OTA update to version %s", _latestVersion.c_str());
    
    // Publish "preparing" state immediately (matches DownloadFirmwareService)
    _reportProgressToBothChannels(0, 0, 0);
    
    // Start OTA update in separate task to avoid blocking MQTT
    // Use same priority as DownloadFirmwareService: (configMAX_PRIORITIES - 1)
    xTaskCreatePinnedToCore(
        _otaUpdateTask,
        "OTA_Update",
        9216, // Stack size (same as DownloadFirmwareService: OTA_TASK_STACK_SIZE)
        this, // Pass this pointer as parameter
        (configMAX_PRIORITIES - 1), // Same priority as DownloadFirmwareService
        nullptr,
        1 // Pin to application core (same as DownloadFirmwareService)
    );
}

void GatewayDeviceMqttService::_otaUpdateTask(void *parameter)
{
    GatewayDeviceMqttService *service = static_cast<GatewayDeviceMqttService *>(parameter);
    if (service != nullptr)
    {
        service->_performOTAUpdate();
    }
    vTaskDelete(nullptr);
}

void GatewayDeviceMqttService::_performOTAUpdate()
{
    ESP_LOGI(TAG, "Starting OTA update from: %s", _downloadUrl.c_str());
    
    // Register this instance as active for callbacks
    _activeOtaInstance = this;
    
    // Send initial "preparing" state
    _reportProgressToBothChannels(0, 0, 0);
    
    WiFiClientSecure client;
    
#ifndef DOWNLOAD_OTA_SKIP_CERT_VERIFY
    // Use certificate bundle for verification
    extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_certs_x509_crt_bundle_bin_start");
    extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_certs_x509_crt_bundle_bin_end");
    
#if ESP_ARDUINO_VERSION_MAJOR == 3
    client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
#else
    client.setCACertBundle(rootca_crt_bundle_start);
#endif
#else
    ESP_LOGW(TAG, "Skipping SSL certificate verification for OTA update!");
    client.setInsecure();
#endif
    
    client.setTimeout(12000);
    
    // Configure HTTPUpdate
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(true);
    
    // Set up progress callback (static function)
    httpUpdate.onProgress(_httpUpdateProgressCallback);
    
    // Start update
    t_httpUpdate_return ret = httpUpdate.update(client, _downloadUrl);
    
    // Reduce task priority to allow other tasks to run
    vTaskPrioritySet(NULL, tskIDLE_PRIORITY + 1);
    
    // Handle result
    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
        ESP_LOGE(TAG, "HTTP Update failed with error (%d): %s", 
                 httpUpdate.getLastError(), 
                 httpUpdate.getLastErrorString().c_str());
        
        // Publish error state to both channels
        if (_eventSocket != nullptr)
        {
            JsonDocument doc;
            doc["status"] = "error";
            doc["error"] = httpUpdate.getLastErrorString().c_str();
            JsonObject jsonObject = doc.as<JsonObject>();
            _eventSocket->emitEvent(EVENT_OTA_UPDATE, jsonObject);
        }
        
        if (_mqttClient != nullptr && _mqttClient->connected())
        {
            _publishUpdateState(); // Back to normal state
        }
        break;
        
    case HTTP_UPDATE_NO_UPDATES:
        ESP_LOGW(TAG, "HTTP Update: No update needed (same version)");
        
        if (_eventSocket != nullptr)
        {
            JsonDocument doc;
            doc["status"] = "error";
            doc["error"] = "Update failed, has same firmware version";
            JsonObject jsonObject = doc.as<JsonObject>();
            _eventSocket->emitEvent(EVENT_OTA_UPDATE, jsonObject);
        }
        
        if (_mqttClient != nullptr && _mqttClient->connected())
        {
            _publishUpdateState();
        }
        break;
        
    case HTTP_UPDATE_OK:
        ESP_LOGI(TAG, "HTTP Update successful - Device will restart");
        
        if (_eventSocket != nullptr)
        {
            JsonDocument doc;
            doc["status"] = "finished";
            doc["progress"] = 100;
            JsonObject jsonObject = doc.as<JsonObject>();
            _eventSocket->emitEvent(EVENT_OTA_UPDATE, jsonObject);
        }
        
        // Also publish to MQTT
        _reportProgressToBothChannels(100, 0, 0);
        
        vTaskDelay(pdMS_TO_TICKS(250)); // Allow messages to be sent
        // Device will restart automatically
        break;
    }
    
    // Clear active instance
    _activeOtaInstance = nullptr;
    
    // Release mutex
    xSemaphoreGive(_otaUpdateMutex);
}

void GatewayDeviceMqttService::_publishUpdateProgress(int progress)
{
    if (_mqttClient == nullptr || !_mqttClient->connected())
        return;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    String stateTopic = baseTopic + "/update/state";
    
    JsonDocument state;
    state["installed_version"] = String(APP_VERSION);
    state["latest_version"] = _latestVersion;
    state["title"] = "Genius Gateway Firmware " + _latestVersion;
    state["release_url"] = "https://github.com/hmbacher/genius-gateway/releases/latest";
    
    // Add progress information
    if (progress >= 0 && progress <= 100)
    {
        state["in_progress"] = progress;
        ESP_LOGV(TAG, "Publishing MQTT update progress: %d%%", progress);
    }
    
    String payload;
    serializeJson(state, payload);
    
    _mqttClient->publish(stateTopic.c_str(), 0, true, payload.c_str());
}

void GatewayDeviceMqttService::_httpUpdateProgressCallback(int current, int total)
{
    if (_activeOtaInstance != nullptr && total > 0)
    {
        int progress = (current * 100) / total;
        _activeOtaInstance->_reportProgressToBothChannels(progress, current, total);
    }
}

void GatewayDeviceMqttService::_reportProgressToBothChannels(int progress, int bytesWritten, int totalBytes)
{
    static int lastProgress = -1;
    
    // Report on progress change (avoid flooding)
    if (progress != lastProgress)
    {
        ESP_LOGV(TAG, "OTA Progress: %d%% (%d / %d bytes)", progress, bytesWritten, totalBytes);
        
        // Report to MQTT (Home Assistant) - only significant progress steps
        if (progress % 10 == 0 || progress == 0 || progress == 100)
        {
            _publishUpdateProgress(progress);
        }
        
        // Report to WebSocket (Web Interface) - matches DownloadFirmwareService format
        if (_eventSocket != nullptr)
        {
            JsonDocument doc;
            
            if (progress == 0 && bytesWritten == 0 && totalBytes == 0)
            {
                // Initial "preparing" state
                doc["status"] = "preparing";
                doc["progress"] = 0;
                doc["bytes_written"] = 0;
                doc["total_bytes"] = 0;
            }
            else
            {
                // Progress update
                doc["status"] = "progress";
                doc["progress"] = progress;
                doc["bytes_written"] = bytesWritten;
                doc["total_bytes"] = totalBytes;
            }
            
            JsonObject jsonObject = doc.as<JsonObject>();
            _eventSocket->emitEvent(EVENT_OTA_UPDATE, jsonObject);
        }
        
        lastProgress = progress;
    }
}

// ============================================================================
// Private Methods - Update Checking
// ============================================================================

void GatewayDeviceMqttService::_updateCheckTimerCallback(TimerHandle_t timer)
{
    // Retrieve the service instance from timer ID
    GatewayDeviceMqttService *service = static_cast<GatewayDeviceMqttService *>(pvTimerGetTimerID(timer));
    if (service != nullptr && service->_updateCheckTaskHandle != nullptr)
    {
        // Increment counter
        service->_updateCheckCounter++;
        
        // Only trigger update check every Nth callback
        if (service->_updateCheckCounter >= UPDATE_CHECK_COUNTER_TARGET)
        {
            service->_updateCheckCounter = 0; // Reset counter
            uint32_t intervalHours = (UPDATE_CHECK_TIMER_INTERVAL_MS * UPDATE_CHECK_COUNTER_TARGET) / (60 * 60 * 1000);
            ESP_LOGI(TAG, "Triggering periodic firmware update check (%lu-hour interval)", intervalHours);
            // Notify persistent task to perform update check
            xTaskNotifyGive(service->_updateCheckTaskHandle);
        }
    }
}

void GatewayDeviceMqttService::_updateCheckTask(void *parameter)
{
    GatewayDeviceMqttService *service = static_cast<GatewayDeviceMqttService *>(parameter);
    
    if (service == nullptr)
    {
        vTaskDelete(nullptr);
        return;
    }
    
    // Persistent task loop - wait for notifications
    while (true)
    {
        // Wait indefinitely for notification (blocks, no CPU usage)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Notification received - perform update check
        if (!service->_updateCheckPending)
        {          
            service->_updateCheckPending = true;
            service->checkForUpdates();
            service->_updateCheckPending = false;
        }
        else
        {
            ESP_LOGW(TAG, "Update check already pending - skipping");
        }
    }
}

bool GatewayDeviceMqttService::_queryGitHubForLatestRelease()
{
    // Use shared GitHubReleaseService for querying
    String userAgent = "Genius-Gateway/" + String(APP_VERSION);
    
    GitHubReleaseInfo releaseInfo = GitHubReleaseService::queryLatestRelease(
        String(GITHUB_REPO_OWNER),
        String(GITHUB_REPO_NAME),
        userAgent,
        false  // Verify SSL certificates
    );
    
    if (!releaseInfo.valid)
    {
        ESP_LOGE(TAG, "Failed to query GitHub for latest release");
        return false;
    }
    
    _latestVersion = releaseInfo.version;
    _downloadUrl = releaseInfo.downloadUrl;
    
    // Compare versions
    String currentVersion = String(APP_VERSION);
    _updateAvailable = GitHubReleaseService::isNewerVersion(currentVersion, _latestVersion);
    
    ESP_LOGI(TAG, "GitHub check complete - Current: %s, Latest: %s, Update available: %s",
             currentVersion.c_str(), _latestVersion.c_str(), _updateAvailable ? "YES" : "NO");
    
    return true;
}

void GatewayDeviceMqttService::_onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    // Only perform initial check once after boot (subsequent checks via periodic timer)
    if (!_initialUpdateCheckDone && _updateCheckTaskHandle != nullptr)
    {
        _initialUpdateCheckDone = true;
        ESP_LOGI(TAG, "WiFi connected (initial boot), triggering first update check");
        
        // Notify persistent task to perform check
        xTaskNotifyGive(_updateCheckTaskHandle);
    }
    else if (_initialUpdateCheckDone)
    {
        ESP_LOGD(TAG, "WiFi reconnect detected - ignoring (initial check already done, periodic checks via timer)");
    }
}
#endif

// ============================================================================
// Private Methods - Helpers
// ============================================================================

void GatewayDeviceMqttService::_addGatewayDeviceInfo(JsonDocument &doc)
{
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = _gatewayDeviceId;
    device["name"] = "Genius Gateway";
    device["manufacturer"] = "Genius Gateway Project";
    device["model"] = "Genius Gateway";
    device["sw_version"] = String(APP_VERSION);
    device["hw_version"] = String(HW_VERSION);
    
    // Add configuration URL if we have a valid IP
    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
    {
        device["configuration_url"] = "http://" + localIP.toString() + "/";
    }
}
