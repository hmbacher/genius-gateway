/**
 * @file GatewayDeviceMqttService.h
 * @brief MQTT service for publishing Genius Gateway as Home Assistant device
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

#include <PsychicMqttClient.h>
#include <GatewayMqttSettingsService.h>
#include <GatewaySettingsService.h>
#include <RestartService.h>
#include <DownloadFirmwareService.h>
#include <FirmwareUpdateEvents.h>
#include <EventSocket.h>
#include <WiFi.h>
#include <IPUtils.h>
#include <ArduinoJson.h>
#include <ESP32SvelteKit.h>
#include <GitHubReleaseService.h>
#include <SettingValue.h>

/// Service for publishing Genius Gateway as Home Assistant device with control entities
class GatewayDeviceMqttService
{
public:
    static constexpr const char *TAG = "GatewayDeviceMqtt"; ///< Logging tag

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    static constexpr uint32_t UPDATE_CHECK_TIMER_INTERVAL_MS = 30 * 60 * 1000; ///< Timer fires every 30 minutes
    static constexpr uint8_t UPDATE_CHECK_COUNTER_TARGET = 12; ///< Perform check every 12 callbacks for long interval
#endif

    static constexpr uint32_t DIAGNOSTIC_SENSOR_UPDATE_INTERVAL_MS = 1 * 60 * 1000; ///< Update every 1 minute

    /**
     * @brief Constructor
     * 
     * @param mqttClient MQTT client instance
     * @param mqttSettingsService MQTT settings service
     * @param gatewaySettingsService Gateway settings service
     * @param downloadFirmwareService Download firmware service for OTA updates
     * @param eventSocket WebSocket for coordinated progress reporting
     */
    GatewayDeviceMqttService(PsychicMqttClient *mqttClient, 
                            GatewayMqttSettingsService *mqttSettingsService,
                            GatewaySettingsService *gatewaySettingsService,
                            DownloadFirmwareService *downloadFirmwareService,
                            EventSocket *eventSocket);

    /**
     * @brief Initialize the service
     * 
     * Registers update handlers for MQTT settings changes
     */
    void begin();

    /**
     * @brief Publish gateway device and all entities
     * 
     * Called on MQTT connect to register the gateway as a device
     * and publish all control entities (buttons, switches, etc.)
     */
    void publishAll();

    /**
     * @brief Get the gateway device identifier
     * 
     * Returns the unique identifier for the gateway device based on MAC address
     * 
     * @return String Gateway device identifier (format: "genius-gateway-<MAC>")
     */
    String getGatewayDeviceId();

    /**
     * @brief Static helper to get gateway device identifier
     * 
     * Can be used by other services to reference the gateway device consistently
     * 
     * @return String Gateway device identifier (format: "genius-gateway-<MAC>")
     */
    static String getGatewayIdentifier();

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    /**
     * @brief Check for firmware updates on GitHub
     * 
     * Queries GitHub API for latest release and compares with current version
     */
    void checkForUpdates();
#endif

private:
    // Services
    PsychicMqttClient *_mqttClient;
    GatewayMqttSettingsService *_mqttSettingsService;
    GatewaySettingsService *_gatewaySettingsService;
    DownloadFirmwareService *_downloadFirmwareService;
    EventSocket *_eventSocket;
    
    // Cached settings
    GatewayMqttSettings _cachedMqttSettings;
    GatewaySettings _cachedGatewaySettings;
    
    // State
    String _gatewayDeviceId; ///< Cached gateway device ID
    TimerHandle_t _diagnosticSensorTimer; ///< Timer for periodic diagnostic sensor updates

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    String _latestVersion;   ///< Latest available version from GitHub
    String _downloadUrl;     ///< Download URL for latest version
    bool _updateAvailable;   ///< Flag indicating if update is available
    TimerHandle_t _updateCheckTimer; ///< Timer for periodic update checks
    TaskHandle_t _updateCheckTaskHandle; ///< Persistent task handle for update checks (single 8KB stack)
    bool _initialUpdateCheckDone; ///< Flag to ensure initial check runs only once
    uint8_t _updateCheckCounter; ///< Counter for timer callbacks (check every Nth callback)
    
    // OTA Update coordination
    static SemaphoreHandle_t _otaUpdateMutex; ///< Mutex for OTA update operations (shared across instances)
    static GatewayDeviceMqttService *_activeOtaInstance; ///< Active OTA instance for progress callbacks
    
    // Update check task
    bool _updateCheckPending; ///< Flag indicating update check is pending
#endif

    /**
     * @brief Update cached MQTT settings
     */
    void _updateMqttSettingsCache();

    /**
     * @brief Update cached gateway settings
     */
    void _updateGatewaySettingsCache();

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    /**
     * @brief WiFi event handler for initial update check
     * 
     * @param event WiFi event type
     * @param info Event information
     */
    void _onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
#endif

    /**
     * @brief Publish the gateway device to Home Assistant
     * 
     * Creates a sensor entity just to register the device.
     * The sensor itself shows diagnostic information.
     */
    void _publishGatewayDevice();

    /**
     * @brief Publish Free Heap diagnostic sensor
     * 
     * Publishes config for heap memory sensor
     */
    void _publishHeapSensor();

    /**
     * @brief Publish Core Temperature diagnostic sensor
     * 
     * Publishes config for ESP32 core temperature sensor
     */
    void _publishTempSensor();

    /**
     * @brief Publish diagnostic sensor states (heap, temperature)
     * 
     * Called periodically to update sensor values in Home Assistant
     */
    void _publishDiagnosticSensorStates();

    /**
     * @brief Static timer callback for diagnostic sensor updates
     * 
     * @param timer Timer handle
     */
    static void _diagnosticSensorTimerCallback(TimerHandle_t timer);

    /**
     * @brief Publish the restart/reboot button entity
     * 
     * Allows triggering a gateway restart from Home Assistant
     */
    void _publishRestartButton();

    /**
     * @brief Publish gateway setting switches
     * 
     * Publishes 4 switch entities for gateway configuration options
     */
    void _publishSettingSwitches();

    /**
     * @brief Publish state for all setting switches
     */
    void _publishSettingSwitchStates();

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    /**
     * @brief Publish firmware update entity
     * 
     * Publishes Home Assistant update entity for firmware management
     */
    void _publishUpdateEntity();

    /**
     * @brief Publish update entity state
     * 
     * Publishes current firmware version and availability of updates
     */
    void _publishUpdateState();
#endif

    /**
     * @brief Subscribe to MQTT command topics
     * 
     * Sets up subscriptions for button commands
     */
    void _subscribeToCommands();

    /**
     * @brief Callback for restart button command
     * 
     * @param topic MQTT topic
     * @param payload MQTT payload
     * @param retain Retain flag
     * @param qos QoS level
     * @param dup Duplicate flag
     */
    void _onRestartCommand(char *topic, char *payload, int retain, int qos, bool dup);

    /**
     * @brief Callback for setting switch commands
     * 
     * @param settingName Name of the setting (for logging)
     * @param topic MQTT topic
     * @param payload MQTT payload ("ON" or "OFF")
     * @param retain Retain flag
     * @param qos QoS level
     * @param dup Duplicate flag
     */
    void _onSettingSwitchCommand(const char *settingName, char *topic, char *payload, int retain, int qos, bool dup);

#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    /**
     * @brief Callback for update install command
     * 
     * @param topic MQTT topic
     * @param payload MQTT payload (should be empty for install)
     * @param retain Retain flag
     * @param qos QoS level
     * @param dup Duplicate flag
     */
    void _onUpdateInstallCommand(char *topic, char *payload, int retain, int qos, bool dup);

    /**
     * @brief Perform OTA update with progress reporting
     * 
     * Runs in separate task to avoid blocking MQTT
     */
    void _performOTAUpdate();

    /**
     * @brief Static task wrapper for OTA update
     * 
     * @param parameter Pointer to GatewayDeviceMqttService instance
     */
    static void _otaUpdateTask(void *parameter);

    /**
     * @brief Publish update progress state
     * 
     * @param progress Progress percentage (0-100)
     */
    void _publishUpdateProgress(int progress);

    /**
     * @brief Static HTTPUpdate progress callback
     * 
     * Called by HTTPUpdate library, forwards to active instance
     * 
     * @param current Current bytes downloaded
     * @param total Total bytes to download
     */
    static void _httpUpdateProgressCallback(int current, int total);

    /**
     * @brief Report progress to both MQTT and WebSocket
     * 
     * @param progress Progress percentage (0-100)
     * @param bytesWritten Optional: Bytes written so far
     * @param totalBytes Optional: Total bytes to write
     */
    void _reportProgressToBothChannels(int progress, int bytesWritten = 0, int totalBytes = 0);

    /**
     * @brief Static timer callback for update checks
     * 
     * @param timer Timer handle
     */
    static void _updateCheckTimerCallback(TimerHandle_t timer);
    
    /**
     * @brief Static task for performing update check
     * 
     * @param parameter Pointer to GatewayDeviceMqttService instance
     */
    static void _updateCheckTask(void *parameter);

    /**
     * @brief Query GitHub API for latest release
     * 
     * @return true if query was successful and update info is available
     */
    bool _queryGitHubForLatestRelease();
#endif

    /**
     * @brief Helper to add gateway device info to a config document
     * 
     * @param doc JsonDocument to modify
     */
    void _addGatewayDeviceInfo(JsonDocument &doc);
};
