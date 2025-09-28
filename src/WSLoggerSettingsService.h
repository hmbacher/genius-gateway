/// @file WSLoggerSettingsService.h
/// @brief Service for managing WebSocket logger settings

#pragma once

#include <ESP32SvelteKit.h>

#define WSLOGGER_SETTINGS_FILE "/config/wslogger.json" ///< Configuration file path
#define WSLOGGER_SETTINGS_PATH "/rest/wslogger"        ///< REST API endpoint

/// WebSocket logger settings data model
class WSLoggerSettings
{
public:
    static constexpr const char *TAG = "WSLoggerSettings"; ///< Log tag for logger settings

    WSLoggerSettings()
        : wsLoggerEnabled(true)
    {
    }

    bool wsLoggerEnabled; ///< Enable WebSocket packet logging

    /// Serialize settings to JSON
    static void read(WSLoggerSettings &settings, JsonObject &root)
    {
        root["wsLoggerEnabled"] = settings.wsLoggerEnabled;

        ESP_LOGV(TAG, "WebSocket Logger Settings read.");
    }

    /// Update settings from JSON with change detection
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

        return hasChanges ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
    }
};

/// Service managing WebSocket logger settings with REST API and persistence
class WSLoggerSettingsService : public StatefulService<WSLoggerSettings>
{
public:
    static constexpr const char *TAG = "WSLoggerSettingsService"; ///< Log tag for settings service

    WSLoggerSettingsService(ESP32SvelteKit *sveltekit);

    /// Initialize HTTP endpoint and load settings from filesystem
    void begin();

    /// Get a copy of current settings
    WSLoggerSettings getSettingsCopy() const
    {
        return _state;
    }

    /// Check if WebSocket logging is enabled (thread-safe)
    bool isEnabled()
    {
        bool enabled = false;

        beginTransaction();
        enabled = _state.wsLoggerEnabled;
        endTransaction();

        return enabled;
    }

private:
    HttpEndpoint<WSLoggerSettings> _httpEndpoint;   ///< HTTP endpoint for settings management
    FSPersistence<WSLoggerSettings> _fsPersistence; ///< Filesystem persistence handler
};