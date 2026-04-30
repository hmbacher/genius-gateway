/**
 * @file GatewayDeviceMqttService.cpp
 * @brief Implementation of Gateway Device MQTT service (app-specific entities)
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

GatewayDeviceMqttService::GatewayDeviceMqttService(HAService *haService,
                                                   GatewaySettingsService *gatewaySettingsService)
    : _haService(haService),
      _gatewaySettingsService(gatewaySettingsService)
{
}

void GatewayDeviceMqttService::begin()
{
    _updateGatewaySettingsCache();

    // Register with HAService so our entities are published on MQTT connect
    _haService->onPublishAll([this]() {
        this->publishAll();
    });

    // Update cache and republish switch states when gateway settings change
    if (_gatewaySettingsService != nullptr)
    {
        _gatewaySettingsService->addUpdateHandler([this](const String &originId)
                                                  {
                                                      this->_updateGatewaySettingsCache();
                                                      if (this->_haService->isReady())
                                                      {
                                                          this->_publishSettingSwitchStates();
                                                      } },
                                                  false);
    }
}

void GatewayDeviceMqttService::publishAll()
{
    if (!_haService->isReady())
        return;

    ESP_LOGI(TAG, "Publishing Genius Gateway app-specific entities");

    // Publish setting switches
    _publishSettingSwitches();

    // Subscribe to switch command topics
    _subscribeToCommands();

    ESP_LOGI(TAG, "Gateway app-specific entity publishing complete");
}

String GatewayDeviceMqttService::getGatewayDeviceId()
{
    return _haService->getDeviceId();
}

// ============================================================================
// Private Methods - Settings Cache
// ============================================================================

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
// Private Methods - Setting Switches
// ============================================================================

void GatewayDeviceMqttService::_publishSettingSwitches()
{
    if (_gatewaySettingsService == nullptr)
        return;

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
        JsonDocument config;
        config["name"] = sw.name;
        config["unique_id"] = _haService->getDeviceId() + "_" + sw.suffix;
        config["state_topic"] = "~/gateway/state";
        config["value_template"] = "{{ value_json." + String(sw.jsonKey) + " }}";
        config["command_topic"] = "~/gateway/switch/" + String(sw.suffix) + "/set";
        config["payload_on"] = "ON";
        config["payload_off"] = "OFF";
        config["state_on"] = "ON";
        config["state_off"] = "OFF";
        config["icon"] = sw.icon;
        config["entity_category"] = "config";
        
        if (_haService->publishConfig("switch", sw.suffix, config))
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
    if (_gatewaySettingsService == nullptr || !_haService->isReady())
        return;

    String baseTopic = _haService->getBaseTopic();
    
    // Create single JSON state message with all switch states
    JsonDocument stateDoc;
    stateDoc["alert_unknown"] = _cachedGatewaySettings.alertOnUnknownDetectors ? "ON" : "OFF";
    stateDoc["line_commissioning"] = _cachedGatewaySettings.addALarmLineFromCommissioningPacket ? "ON" : "OFF";
    stateDoc["line_alarm"] = _cachedGatewaySettings.addAlarmLineFromAlarmPacket ? "ON" : "OFF";
    stateDoc["line_test"] = _cachedGatewaySettings.addAlarmLineFromLineTestPacket ? "ON" : "OFF";
    
    String statePayload;
    serializeJson(stateDoc, statePayload);
    
    String stateTopic = baseTopic + "/gateway/state";
    if (_haService->publish(stateTopic, statePayload, 0, true))
    {
        ESP_LOGV(TAG, "Published combined switch states: %s", statePayload.c_str());
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish switch states");
    }
}

// ============================================================================
// Private Methods - MQTT Subscriptions
// ============================================================================

void GatewayDeviceMqttService::_subscribeToCommands()
{
    String baseTopic = _haService->getBaseTopic();
    
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
        _haService->subscribe(cmdTopic, 
            [this, settingName = String(sw.name)](char *topic, char *payload, int retain, int qos, bool dup)
            {
                this->_onSettingSwitchCommand(settingName.c_str(), topic, payload, retain, qos, dup);
            });
    }
    
    ESP_LOGI(TAG, "Subscribed to setting switch command topics");
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
