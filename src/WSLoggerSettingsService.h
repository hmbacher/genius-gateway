#pragma once

#include <ESP32SvelteKit.h>

#define WSLOGGER_SETTINGS_FILE "/config/wslogger.json"
#define WSLOGGER_SETTINGS_PATH "/rest/wslogger"

class WSLoggerSettings
{
public:
    static constexpr const char *TAG = "WSLoggerSettings";

    WSLoggerSettings()
        : wsLoggerEnabled(true)
    {
    }

    bool wsLoggerEnabled;

    static void read(WSLoggerSettings &settings, JsonObject &root)
    {
        root["wsLoggerEnabled"] = settings.wsLoggerEnabled;

        ESP_LOGV(TAG, "WebSocket Logger Settings read.");
    }

    static StateUpdateResult update(JsonObject &root, WSLoggerSettings &settings)
    {
        bool hasChanges = false;

        if (root["wsLoggerEnabled"].is<bool>())
        {
            bool newValue = root["wsLoggerEnabled"].as<bool>();

            if (settings.wsLoggerEnabled != newValue)
            {
                settings.wsLoggerEnabled = newValue;
                hasChanges = true;
            }
        }

        if (hasChanges)
        {
            ESP_LOGV(TAG, "WebSocket Logger Settings updated.");
        }

        return hasChanges ? StateUpdateResult::CHANGED : 
                           StateUpdateResult::UNCHANGED;
    }
};

class WSLoggerSettingsService : public StatefulService<WSLoggerSettings>
{
public:
    static constexpr const char *TAG = "WSLoggerSettingsService";

    WSLoggerSettingsService(ESP32SvelteKit *sveltekit);

    void begin();

    WSLoggerSettings getSettingsCopy() const
    {
        return _state;
    }

    bool isEnabled()
    {
        bool enabled = false;

        beginTransaction();
        enabled = _state.wsLoggerEnabled;
        endTransaction();

        return enabled;
    }

private:
    HttpEndpoint<WSLoggerSettings> _httpEndpoint;
    FSPersistence<WSLoggerSettings> _fsPersistence;
};