/**
 * @file GatewayDeviceMqttService.h
 * @brief MQTT service for publishing Genius Gateway app-specific entities to Home Assistant
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

#include <GatewayMqttSettingsService.h>
#include <GatewaySettingsService.h>
#include <ArduinoJson.h>
#include <HomeAssistant/HAService.h>

/**
 * @brief Service for publishing Genius Gateway app-specific entities to Home Assistant
 *
 * Uses the framework's HAService for device identity, discovery topic formatting,
 * and MQTT publish/subscribe. Framework-level entities (diagnostics, update, restart)
 * are handled by HADiagnosticService and HAUpdateService.
 *
 * This service manages app-specific entities:
 * - Setting switches (Alert on Unknown Detectors, Add Line from Commissioning/Alarm/Test)
 *
 * It also acts as the bridge between app-specific GatewayMqttSettings (HA enabled flag,
 * discovery prefix) and the framework HAService configuration.
 */
class GatewayDeviceMqttService
{
public:
    static constexpr const char *TAG = "GatewayDeviceMqtt"; ///< Logging tag

    /**
     * @brief Constructor
     * 
     * @param haService Framework HAService for device identity and MQTT helpers
     * @param mqttSettingsService Gateway MQTT settings (HA enabled, discovery prefix)
     * @param gatewaySettingsService Gateway settings (switch states)
     */
    GatewayDeviceMqttService(HAService *haService,
                            GatewayMqttSettingsService *mqttSettingsService,
                            GatewaySettingsService *gatewaySettingsService);

    /**
     * @brief Initialize the service
     * 
     * - Configures HAService with HA enabled state and discovery prefix
     * - Registers settings change handlers
     * - Registers with HAService for entity publishing on MQTT connect
     */
    void begin();

    /**
     * @brief Publish app-specific entities (setting switches)
     * 
     * Called by HAService::publishAll() on MQTT connect.
     */
    void publishAll();

    /**
     * @brief Get the gateway device identifier
     * @return String Gateway device identifier from HAService
     */
    String getGatewayDeviceId();

private:
    // Services
    HAService *_haService;
    GatewayMqttSettingsService *_mqttSettingsService;
    GatewaySettingsService *_gatewaySettingsService;
    
    // Cached settings
    GatewayMqttSettings _cachedMqttSettings;
    GatewaySettings _cachedGatewaySettings;

    /**
     * @brief Update cached MQTT settings and reconfigure HAService
     */
    void _updateMqttSettingsCache();

    /**
     * @brief Update cached gateway settings
     */
    void _updateGatewaySettingsCache();

    /**
     * @brief Publish gateway setting switches
     */
    void _publishSettingSwitches();

    /**
     * @brief Publish state for all setting switches
     */
    void _publishSettingSwitchStates();

    /**
     * @brief Subscribe to setting switch command topics
     */
    void _subscribeToCommands();

    /**
     * @brief Handle setting switch command from HA
     */
    void _onSettingSwitchCommand(const char *settingName, char *topic, char *payload, int retain, int qos, bool dup);
};
