#pragma once

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <SettingValue.h>
#include <ESP32SvelteKit.h>

#define GATEWAY_MQTT_SETTINGS_FILE "/config/mqtt-settings.json"
#define GATEWAY_MQTT_SETTINGS_PATH "/rest/mqtt-settings"

#define GATEWAY_HA_MQTT_TOPIC_PREFIX "homeassistant/binary_sensor/genius-"
#define GATEWAY_ALARM_MQTT_TOPIC "smarthome/genius-gateway/alarm"

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

    boolean haMQTTEnabled;      // Enable Home Assistant compatible MQTT publishing
    String haMQTTTopicPrefix;   // Home Assistant MQTT topic prefix
    boolean alarmEnabled;       // Enable alarm publishing over MQTT
    String alarmTopic;          // MQTT topic for alarm state


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
    static constexpr const char *TAG = "GatewayMqttSettings";
};

class GatewayMqttSettingsService : public StatefulService<GatewayMqttSettings>
{
public:
    GatewayMqttSettingsService(ESP32SvelteKit *sveltekit);

    void begin();

    GatewayMqttSettings &getSettings()
    {
        return _state;
    }

private:
    HttpEndpoint<GatewayMqttSettings> _httpEndpoint;
    FSPersistence<GatewayMqttSettings> _fsPersistence;
};
