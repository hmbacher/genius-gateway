/**
 * @file AlarmPublishingSettingsService.h
 * @brief Settings service for the gateway's simple alarm-publishing feature.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
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

#define ALARM_PUBLISHING_SETTINGS_FILE "/config/alarm-publishing.json" ///< Configuration file path
#define ALARM_PUBLISHING_SETTINGS_PATH "/rest/alarm-publishing"        ///< REST API service endpoint path

#define DEFAULT_ALARM_PUBLISHING_TOPIC "smarthome/genius-gateway/alarm" ///< Default alarm MQTT topic

/// Simple alarm-publishing settings data model
class AlarmPublishingSettings
{
public:
    AlarmPublishingSettings()
        : alarmEnabled(false),
          alarmTopic(DEFAULT_ALARM_PUBLISHING_TOPIC)
    {
    }

    boolean alarmEnabled; ///< Enable simple alarm publishing over MQTT
    String alarmTopic;    ///< MQTT topic for alarm state

    static void read(AlarmPublishingSettings &settings, JsonObject &root)
    {
        root["alarmEnabled"] = settings.alarmEnabled;
        root["alarmTopic"] = settings.alarmTopic;

        ESP_LOGV(AlarmPublishingSettings::TAG, "Alarm publishing settings read.");
    }

    static StateUpdateResult update(JsonObject &root, AlarmPublishingSettings &settings, const String &originId)
    {
        bool changed = false;

        if (root["alarmEnabled"].is<bool>())
        {
            bool newSetting = root["alarmEnabled"];
            if (settings.alarmEnabled != newSetting)
            {
                settings.alarmEnabled = newSetting;
                changed = true;
            }
        }

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
            ESP_LOGV(AlarmPublishingSettings::TAG, "Alarm publishing settings updated.");

        return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
    }

private:
    static constexpr const char *TAG = "AlarmPublishing"; ///< Logging tag
};

/// Service for the gateway's simple alarm-publishing feature
class AlarmPublishingSettingsService : public StatefulService<AlarmPublishingSettings>
{
public:
    AlarmPublishingSettingsService(ESP32SvelteKit *sveltekit);

    /// Initialize the alarm-publishing settings service
    void begin();

    /// Get a copy of the current settings
    AlarmPublishingSettings getSettingsCopy() const
    {
        return _state;
    }

private:
    static constexpr const char *TAG = "AlarmPublishingSvc"; ///< Logging tag

    HttpEndpoint<AlarmPublishingSettings> _httpEndpoint;   ///< REST API endpoint handler
    FSPersistence<AlarmPublishingSettings> _fsPersistence; ///< File system persistence handler
    FS *_fs;                                               ///< File system pointer (not owned)

    // Pre-v1.3.0 the alarm-publishing keys lived in /config/mqtt-settings.json
    // alongside the HA settings. Copy them into the new file on first boot
    // after upgrade so the user keeps their enabled flag and topic.
    void _migrateLegacySettings();
};
