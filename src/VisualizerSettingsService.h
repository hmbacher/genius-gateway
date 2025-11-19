/// @file VisualizerSettingsService.h
/// @brief Service for managing packet visualizer settings
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

#include <FSPersistence.h>
#include <HttpEndpoint.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <ESP32SvelteKit.h>
#include <Utils.hpp>

#define PACKET_VISUALIZER_SETTINGS_FILE "/config/packet-visualizer.json"  ///< Configuration file path
#define PACKET_VISUALIZER_SETTINGS_SERVICE_PATH "/rest/packet-visualizer" ///< REST API endpoint

#define PACKET_VISUALIZER_SETTINGS_STR_SHOW_DETAILS "showDetails"   ///< JSON key for show details setting
#define PACKET_VISUALIZER_SETTINGS_DEFAULT_SHOW_DETAILS true        ///< Default show details setting
#define PACKET_VISUALIZER_SETTINGS_STR_SHOW_METADATA "showMetadata" ///< JSON key for show metadata setting
#define PACKET_VISUALIZER_SETTINGS_DEFAULT_SHOW_METADATA true       ///< Default show metadata setting

/// Packet visualizer settings data model
class VisualizerSettings
{

public:
    bool showDetails = PACKET_VISUALIZER_SETTINGS_DEFAULT_SHOW_DETAILS;   ///< Show packet details in visualizer
    bool showMetadata = PACKET_VISUALIZER_SETTINGS_DEFAULT_SHOW_METADATA; ///< Show packet metadata in visualizer

    /// Serialize settings to JSON
    static void read(VisualizerSettings &vizSettings, JsonObject &root)
    {
        root[PACKET_VISUALIZER_SETTINGS_STR_SHOW_DETAILS] = vizSettings.showDetails;
        root[PACKET_VISUALIZER_SETTINGS_STR_SHOW_METADATA] = vizSettings.showMetadata;

        ESP_LOGV(VisualizerSettings::TAG, "Packet visualizer settings read.");
    }

    /// Update settings from JSON
    static StateUpdateResult update(JsonObject &root, VisualizerSettings &vizSettings)
    {
        vizSettings.showDetails = root[PACKET_VISUALIZER_SETTINGS_STR_SHOW_DETAILS] | PACKET_VISUALIZER_SETTINGS_DEFAULT_SHOW_DETAILS;
        vizSettings.showMetadata = root[PACKET_VISUALIZER_SETTINGS_STR_SHOW_METADATA] | PACKET_VISUALIZER_SETTINGS_DEFAULT_SHOW_METADATA;

        ESP_LOGV(VisualizerSettings::TAG, "Packet visualizer settings updated.");

        return StateUpdateResult::UNCHANGED;
    }

protected:
    static constexpr const char *TAG = "VisualizerSettings"; ///< Log tag for visualizer settings
};

/// Service managing packet visualizer settings with REST API and persistence
class VisualizerSettingsService : public StatefulService<VisualizerSettings>
{
public:
    VisualizerSettingsService(ESP32SvelteKit *sveltekit);

    /// Initialize HTTP endpoint and load settings from filesystem
    void begin();

private:
    HttpEndpoint<VisualizerSettings> _httpEndpoint;   ///< HTTP endpoint for settings management
    FSPersistence<VisualizerSettings> _fsPersistence; ///< Filesystem persistence handler
};
