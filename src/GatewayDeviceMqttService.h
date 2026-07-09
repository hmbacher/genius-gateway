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

#include <GatewaySettingsService.h>
#include <HomeAssistant/HAService.h>
#include <HomeAssistant/HAGroupedSwitchPublisher.h>

/**
 * @brief Publishes Genius Gateway app-specific switch entities to Home Assistant.
 *
 * Four configuration switches (alert_unknown, line_commissioning, line_alarm,
 * line_test) are bound to GatewaySettings fields via HAGroupedSwitchPublisher.
 * All discovery, state publishing, and command subscriptions are handled by
 * the publisher - this class is a thin wiring layer.
 *
 * Topic layout (relative to gateway base topic):
 *   State:   ~/gateway/state           (shared JSON for all four switches)
 *   Commands:~/gateway/<objectId>/set  (per-switch command topic)
 */
class GatewayDeviceMqttService
{
public:
    static constexpr const char *TAG = "GatewayDeviceMqtt";

    GatewayDeviceMqttService(HAService *haService,
                             GatewaySettingsService *gatewaySettingsService);

    /**
     * @brief Register switches and start publishing.
     *
     * Must be called after HAService::begin(). Adds the four setting switches
     * and registers with HAService::onPublishAll for automatic republish on
     * MQTT (re)connect.
     */
    void begin();

private:
    HAGroupedSwitchPublisher<GatewaySettings> _switchPublisher;
};
