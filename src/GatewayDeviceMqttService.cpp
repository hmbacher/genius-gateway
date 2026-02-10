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
SemaphoreHandle_t GatewayDeviceMqttService::_otaUpdateMutex = nullptr;
GatewayDeviceMqttService *GatewayDeviceMqttService::_activeOtaInstance = nullptr;

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
      _updateAvailable(false),
      _updateCheckTimer(nullptr)
{
    // Create mutex for OTA updates (only once)
    if (_otaUpdateMutex == nullptr)
    {
        _otaUpdateMutex = xSemaphoreCreateMutex();
    }
}

void GatewayDeviceMqttService::begin()
{
    // Initialize cached MQTT settings
    _updateMqttSettingsCache();

    // Initialize cached gateway settings
    _updateGatewaySettingsCache();

    // Update gateway device ID
    _gatewayDeviceId = "genius-gateway-" + WiFi.macAddress();

    // Create timer for periodic update checks
    _updateCheckTimer = xTimerCreate(
        "UpdateCheck",
        pdMS_TO_TICKS(UPDATE_CHECK_INTERVAL_MS),
        pdTRUE, // Auto-reload
        this,   // Timer ID (pass this pointer)
        _updateCheckTimerCallback
    );

    if (_updateCheckTimer != nullptr)
    {
        xTimerStart(_updateCheckTimer, 0);
        ESP_LOGI(TAG, "Update check timer started (interval: %lu ms)", UPDATE_CHECK_INTERVAL_MS);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create update check timer");
    }

    // Perform initial update check
    checkForUpdates();

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

    // Publish restart button
    _publishRestartButton();

    // Publish setting switches
    _publishSettingSwitches();

    // Publish update entity
    _publishUpdateEntity();

    // Subscribe to command topics
    _subscribeToCommands();

    ESP_LOGI(TAG, "Gateway device publishing complete");
}

void GatewayDeviceMqttService::checkForUpdates()
{
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

String GatewayDeviceMqttService::getGatewayDeviceId()
{
    return _gatewayDeviceId;
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
        _cachedGatewaySettings = _gatewaySettingsService->getSettingsCopy();
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
    config["icon"] = "mdi:router-wireless";
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
    
    // Define switch configurations: {unique_suffix, display_name, icon, setting_key}
    struct SwitchConfig {
        const char* suffix;
        const char* name;
        const char* icon;
        bool state;
    };
    
    SwitchConfig switches[] = {
        {"alert_unknown", "Alert on Unknown Detectors", "mdi:alert-circle", _cachedGatewaySettings.alertOnUnknownDetectors},
        {"line_commissioning", "Add Line from Commissioning", "mdi:link-plus", _cachedGatewaySettings.addALarmLineFromCommissioningPacket},
        {"line_alarm", "Add Line from Alarm", "mdi:fire", _cachedGatewaySettings.addAlarmLineFromAlarmPacket},
        {"line_test", "Add Line from Test", "mdi:test-tube", _cachedGatewaySettings.addAlarmLineFromLineTestPacket}
    };
    
    for (const auto& sw : switches)
    {
        String configTopic = discoveryPrefix + "switch/" + _gatewayDeviceId + "/" + sw.suffix + "/config";
        
        JsonDocument config;
        config["~"] = baseTopic;
        config["name"] = sw.name;
        config["unique_id"] = _gatewayDeviceId + "_" + sw.suffix;
        config["state_topic"] = "~/" + String(sw.suffix) + "/state";
        config["command_topic"] = "~/" + String(sw.suffix) + "/set";
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
            
            // Publish initial state
            String stateTopic = baseTopic + "/" + sw.suffix + "/state";
            String statePayload = sw.state ? "ON" : "OFF";
            _mqttClient->publish(stateTopic.c_str(), 0, true, statePayload.c_str());
        }
        else
        {
            ESP_LOGE(TAG, "Failed to publish switch config: %s", sw.name);
        }
    }
}

void GatewayDeviceMqttService::_publishSettingSwitchStates()
{
    if (_gatewaySettingsService == nullptr || _mqttClient == nullptr || !_mqttClient->connected())
        return;

    if (!_cachedMqttSettings.HAIntegrationEnabled || _cachedMqttSettings.HAMQTTDiscoveryPrefix.isEmpty())
        return;

    String discoveryPrefix = _cachedMqttSettings.HAMQTTDiscoveryPrefix;
    String baseTopic = discoveryPrefix + "genius-gateway/" + _gatewayDeviceId;
    
    // Publish states for all switches
    struct SwitchState {
        const char* suffix;
        bool state;
    };
    
    SwitchState switches[] = {
        {"alert_unknown", _cachedGatewaySettings.alertOnUnknownDetectors},
        {"line_commissioning", _cachedGatewaySettings.addALarmLineFromCommissioningPacket},
        {"line_alarm", _cachedGatewaySettings.addAlarmLineFromAlarmPacket},
        {"line_test", _cachedGatewaySettings.addAlarmLineFromLineTestPacket}
    };
    
    for (const auto& sw : switches)
    {
        String stateTopic = baseTopic + "/" + sw.suffix + "/state";
        String statePayload = sw.state ? "ON" : "OFF";
        _mqttClient->publish(stateTopic.c_str(), 0, true, statePayload.c_str());
        ESP_LOGV(TAG, "Published switch state: %s = %s", sw.suffix, statePayload.c_str());
    }
}

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
    
    _addGatewayDeviceInfo(config);
    
    String payload;
    serializeJson(config, payload);
    
    if (_mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str()) != -1)
    {
        ESP_LOGV(TAG, "Published update entity config");
        
        // Publish initial state
        _publishUpdateState();
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
    
    if (_updateAvailable && !_latestVersion.isEmpty())
    {
        // Update is available
        state["latest_version"] = _latestVersion;
        state["title"] = "Genius Gateway Firmware " + _latestVersion;
        state["release_url"] = "https://github.com/hmbacher/genius-gateway/releases/latest";
        
        ESP_LOGI(TAG, "Update available: %s -> %s", APP_VERSION, _latestVersion.c_str());
    }
    else
    {
        // No update available
        state["latest_version"] = String(APP_VERSION);
        ESP_LOGV(TAG, "No update available (current: %s)", APP_VERSION);
    }
    
    String payload;
    serializeJson(state, payload);
    
    _mqttClient->publish(stateTopic.c_str(), 0, true, payload.c_str());
    ESP_LOGV(TAG, "Published update state");
}

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
        String cmdTopic = baseTopic + "/" + sw.suffix + "/set";
        _mqttClient->onTopic(cmdTopic.c_str(), 0, 
            [this, settingName = String(sw.name)](char *topic, char *payload, int retain, int qos, bool dup)
            {
                this->_onSettingSwitchCommand(settingName.c_str(), topic, payload, retain, qos, dup);
            });
    }
    
    // Subscribe to update install command
    String updateInstallTopic = baseTopic + "/update/install";
    _mqttClient->onTopic(updateInstallTopic.c_str(), 0, 
        [this](char *topic, char *payload, int retain, int qos, bool dup)
        {
            this->_onUpdateInstallCommand(topic, payload, retain, qos, dup);
        });
    
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
    _gatewaySettingsService->update([settingName, newValue](GatewaySettings &settings)
                                    {
                                        String settingNameStr = String(settingName);
                                        
                                        if (settingNameStr == "alert_on_unknown_detectors")
                                        {
                                            settings.alertOnUnknownDetectors = newValue;
                                        }
                                        else if (settingNameStr == "add_alarm_line_from_commissioning_packet")
                                        {
                                            settings.addALarmLineFromCommissioningPacket = newValue;
                                        }
                                        else if (settingNameStr == "add_alarm_line_from_alarm_packet")
                                        {
                                            settings.addAlarmLineFromAlarmPacket = newValue;
                                        }
                                        else if (settingNameStr == "add_alarm_line_from_line_test_packet")
                                        {
                                            settings.addAlarmLineFromLineTestPacket = newValue;
                                        }
                                        
                                        return StateUpdateResult::CHANGED;
                                    }, 
                                    "mqtt");
}

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
    
    // Publish "installing" state immediately
    _reportProgressToBothChannels(0);
    
    // Start OTA update in separate task to avoid blocking MQTT
    xTaskCreate(
        _otaUpdateTask,
        "OTA_Update",
        9216, // Stack size (same as DownloadFirmwareService)
        this, // Pass this pointer as parameter
        5,    // Priority
        nullptr
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
    
    WiFiClientSecure client;
    client.setInsecure(); // Accept any certificate (could use cert bundle)
    client.setTimeout(10);
    
    // Configure HTTPUpdate
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(true);
    
    // Set up progress callback (static function)
    httpUpdate.onProgress(_httpUpdateProgressCallback);
    
    // Start update
    t_httpUpdate_return ret = httpUpdate.update(client, _downloadUrl);
    
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
            _eventSocket->emitEvent(EVENT_DOWNLOAD_OTA, jsonObject);
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
            _eventSocket->emitEvent(EVENT_DOWNLOAD_OTA, jsonObject);
        }
        
        if (_mqttClient != nullptr && _mqttClient->connected())
        {
            _publishUpdateState();
        }
        break;
        
    case HTTP_UPDATE_OK:
        ESP_LOGI(TAG, "HTTP Update successful - Device will restart");
        _reportProgressToBothChannels(100);
        
        if (_eventSocket != nullptr)
        {
            JsonDocument doc;
            doc["status"] = "finished";
            JsonObject jsonObject = doc.as<JsonObject>();
            _eventSocket->emitEvent(EVENT_DOWNLOAD_OTA, jsonObject);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Allow messages to be sent
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
        _activeOtaInstance->_reportProgressToBothChannels(progress);
    }
}

void GatewayDeviceMqttService::_reportProgressToBothChannels(int progress)
{
    static int lastProgress = -1;
    
    // Only report every 10% to avoid flooding
    if (progress != lastProgress && (progress % 10 == 0 || progress == 0 || progress == 100))
    {
        ESP_LOGI(TAG, "OTA Progress: %d%%", progress);
        
        // Report to MQTT (Home Assistant)
        _publishUpdateProgress(progress);
        
        // Report to WebSocket (Web Interface)
        if (_eventSocket != nullptr)
        {
            JsonDocument doc;
            doc["status"] = "progress";
            doc["progress"] = progress;
            JsonObject jsonObject = doc.as<JsonObject>();
            _eventSocket->emitEvent(EVENT_DOWNLOAD_OTA, jsonObject);
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
    if (service != nullptr)
    {
        service->checkForUpdates();
    }
}

bool GatewayDeviceMqttService::_queryGitHubForLatestRelease()
{
    WiFiClientSecure client;
    client.setInsecure(); // For GitHub API, we could also embed cert bundle
    
    HTTPClient http;
    http.setUserAgent("Genius-Gateway/" + String(APP_VERSION));
    
    if (!http.begin(client, GITHUB_API_URL))
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP connection to GitHub API");
        return false;
    }
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK)
    {
        ESP_LOGE(TAG, "GitHub API request failed with code: %d", httpCode);
        http.end();
        return false;
    }
    
    String payload = http.getString();
    http.end();
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error)
    {
        ESP_LOGE(TAG, "Failed to parse GitHub API response: %s", error.c_str());
        return false;
    }
    
    // Extract version information
    String tagName = doc["tag_name"] | "";
    if (tagName.isEmpty())
    {
        ESP_LOGW(TAG, "No tag_name found in GitHub release");
        return false;
    }
    
    // Remove 'v' prefix if present (e.g., "v1.2.3" -> "1.2.3")
    if (tagName.startsWith("v") || tagName.startsWith("V"))
    {
        tagName = tagName.substring(1);
    }
    
    _latestVersion = tagName;
    
    // Find download URL for the binary (look for .bin file in assets)
    JsonArray assets = doc["assets"];
    _downloadUrl = "";
    
    for (JsonVariant asset : assets)
    {
        String assetName = asset["name"] | "";
        if (assetName.endsWith(".bin"))
        {
            _downloadUrl = asset["browser_download_url"] | "";
            break;
        }
    }
    
    if (_downloadUrl.isEmpty())
    {
        ESP_LOGW(TAG, "No .bin download URL found in GitHub release");
    }
    
    // Compare versions
    String currentVersion = String(APP_VERSION);
    _updateAvailable = _isNewerVersion(currentVersion, _latestVersion);
    
    ESP_LOGI(TAG, "GitHub check complete - Current: %s, Latest: %s, Update available: %s",
             currentVersion.c_str(), _latestVersion.c_str(), _updateAvailable ? "YES" : "NO");
    
    return true;
}

bool GatewayDeviceMqttService::_isNewerVersion(const String &current, const String &latest)
{
    // Simple semantic version comparison (major.minor.patch)
    // Parse current version
    int curMajor = 0, curMinor = 0, curPatch = 0;
    int latMajor = 0, latMinor = 0, latPatch = 0;
    
    sscanf(current.c_str(), "%d.%d.%d", &curMajor, &curMinor, &curPatch);
    sscanf(latest.c_str(), "%d.%d.%d", &latMajor, &latMinor, &latPatch);
    
    // Compare versions
    if (latMajor > curMajor) return true;
    if (latMajor < curMajor) return false;
    
    if (latMinor > curMinor) return true;
    if (latMinor < curMinor) return false;
    
    if (latPatch > curPatch) return true;
    
    return false;
}

// ============================================================================
// Private Methods - Helpers
// ============================================================================

void GatewayDeviceMqttService::_addGatewayDeviceInfo(JsonDocument &doc)
{
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"] = _gatewayDeviceId;
    device["name"] = "Genius Gateway";
    device["manufacturer"] = "Genius Gateway Project";
    device["model"] = "Genius Gateway";
    device["sw_version"] = String(APP_VERSION);
    
    // Add configuration URL if we have a valid IP
    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
    {
        device["configuration_url"] = "http://" + localIP.toString() + "/";
    }
}
