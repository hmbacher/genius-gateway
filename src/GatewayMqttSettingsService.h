/**
 * @file GatewayMqttSettingsService.h
 * @brief Gateway MQTT settings service for managing MQTT configuration
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
