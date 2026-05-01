/**
 * @file GatewayDeviceMqttService.cpp
 * @brief Implementation of Gateway Device MQTT service (app-specific entities)
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

#include <GatewayDeviceMqttService.h>

GatewayDeviceMqttService::GatewayDeviceMqttService(HAService *haService,
                                                   GatewaySettingsService *gatewaySettingsService)
    : _switchPublisher(gatewaySettingsService, haService, "gateway")
{
}

void GatewayDeviceMqttService::begin()
{
    _switchPublisher
        .addSwitch(
            "alert_unknown", "Alert on Unknown Detectors",
            [](const GatewaySettings &s) { return s.alertOnUnknownDetectors; },
            [](GatewaySettings &s, bool v) { s.alertOnUnknownDetectors = v; },
            "mdi:toggle-switch-off-outline", HACategory::Config)
        .addSwitch(
            "line_commissioning", "Add Line from Commissioning",
            [](const GatewaySettings &s) { return s.addALarmLineFromCommissioningPacket; },
            [](GatewaySettings &s, bool v) { s.addALarmLineFromCommissioningPacket = v; },
            "mdi:toggle-switch-off-outline", HACategory::Config)
        .addSwitch(
            "line_alarm", "Add Line from Alarm",
            [](const GatewaySettings &s) { return s.addAlarmLineFromAlarmPacket; },
            [](GatewaySettings &s, bool v) { s.addAlarmLineFromAlarmPacket = v; },
            "mdi:toggle-switch-off-outline", HACategory::Config)
        .addSwitch(
            "line_test", "Add Line from Line Test",
            [](const GatewaySettings &s) { return s.addAlarmLineFromLineTestPacket; },
            [](GatewaySettings &s, bool v) { s.addAlarmLineFromLineTestPacket = v; },
            "mdi:toggle-switch-off-outline", HACategory::Config);

    _switchPublisher.begin();
}
