/// @file VisualizerSettingsService.h
/// @brief Service for managing packet visualizer settings

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
