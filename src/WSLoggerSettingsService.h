/// @file WSLoggerSettingsService.h
/// @brief Service for managing WebSocket logger settings
/// 
/// @copyright Copyright (c) 2024-2025 Genius Gateway Project
/// @license AGPL-3.0 with Commons Clause
/// 
/// This file is part of Genius Gateway.
/// 
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Affero General Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version, with the Commons Clause restriction.
/// 
/// "Commons Clause" License Condition v1.0
/// The Software is provided to you by the Licensor under the License,
/// as defined below, subject to the following condition:
/// Without limiting other conditions in the License, the grant of rights
/// under the License will not include, and the License does not grant to you,
/// the right to Sell the Software.
/// 
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
/// GNU Affero General Public License for more details.
/// 
/// See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.

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