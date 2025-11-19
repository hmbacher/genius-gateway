/**
 * @file GatewayMqttSettingsService.h
 * @brief Gateway MQTT settings service for managing MQTT configuration
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

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <SettingValue.h>
#include <ESP32SvelteKit.h>

#define GATEWAY_MQTT_SETTINGS_FILE "/config/mqtt-settings.json" ///< Configuration file path
#define GATEWAY_MQTT_SETTINGS_PATH "/rest/mqtt-settings"        ///< REST API service endpoint path

#define GATEWAY_HA_MQTT_TOPIC_PREFIX "homeassistant/binary_sensor/genius-" ///< Default Home Assistant MQTT topic prefix
#define GATEWAY_ALARM_MQTT_TOPIC "smarthome/genius-gateway/alarm"          ///< Default alarm MQTT topic

/// Gateway MQTT settings data model class
class GatewayMqttSettings
{
public:
    GatewayMqttSettings()
        : haMQTTEnabled(false),
          haMQTTTopicPrefix(GATEWAY_HA_MQTT_TOPIC_PREFIX),
          alarmEnabled(false),
          alarmTopic(GATEWAY_ALARM_MQTT_TOPIC)
    {
    }

    boolean haMQTTEnabled;    ///< Enable Home Assistant compatible MQTT publishing
    String haMQTTTopicPrefix; ///< Home Assistant MQTT topic prefix
    boolean alarmEnabled;     ///< Enable alarm publishing over MQTT
    String alarmTopic;        ///< MQTT topic for alarm state

    static void read(GatewayMqttSettings &settings, JsonObject &root)
    {
        root["haMQTTEnabled"] = settings.haMQTTEnabled;
        root["haMQTTTopicPrefix"] = settings.haMQTTTopicPrefix;
        root["alarmEnabled"] = settings.alarmEnabled;
        root["alarmTopic"] = settings.alarmTopic;

        ESP_LOGV(GatewayMqttSettings::TAG, "Gateway MQTT settings read.");
    }

    static StateUpdateResult update(JsonObject &root, GatewayMqttSettings &settings)
    {
        bool changed = false;

        // haMQTTEnabled
        if (root["haMQTTEnabled"].is<bool>())
        {
            bool newSetting = root["haMQTTEnabled"];
            if (settings.haMQTTEnabled != newSetting)
            {
                settings.haMQTTEnabled = newSetting;
                changed = true;
            }
        }

        // haMQTTTopicPrefix
        if (root["haMQTTTopicPrefix"].is<String>())
        {
            String newSetting = root["haMQTTTopicPrefix"];
            if (settings.haMQTTTopicPrefix != newSetting)
            {
                settings.haMQTTTopicPrefix = newSetting;
                changed = true;
            }
        }

        // alarmEnabled
        if (root["alarmEnabled"].is<bool>())
        {
            bool newSetting = root["alarmEnabled"];
            if (settings.alarmEnabled != newSetting)
            {
                settings.alarmEnabled = newSetting;
                changed = true;
            }
        }

        // alarmTopic
        if (root["alarmTopic"].is<String>())
        {
            String newSetting = root["alarmTopic"];
            if (settings.alarmTopic != newSetting)
            {
                settings.alarmTopic = newSetting;
                changed = true;
            }
        }

        if (changed)
            ESP_LOGV(GatewayMqttSettings::TAG, "Gateway MQTT settings updated.");

        return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
    }

private:
    static constexpr const char *TAG = "GatewayMqttSettings"; ///< Logging tag
};

/// Service for managing gateway MQTT configuration settings
class GatewayMqttSettingsService : public StatefulService<GatewayMqttSettings>
{
public:
    GatewayMqttSettingsService(ESP32SvelteKit *sveltekit);

    /// Initialize the gateway MQTT settings service
    void begin();

    /// Get a copy of the current MQTT settings
    GatewayMqttSettings getSettingsCopy() const
    {
        return _state;
    }

private:
    HttpEndpoint<GatewayMqttSettings> _httpEndpoint;   ///< REST API endpoint handler
    FSPersistence<GatewayMqttSettings> _fsPersistence; ///< File system persistence handler
};
