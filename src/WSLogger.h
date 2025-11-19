/// @file WSLogger.h
/// @brief WebSocket logger for CC1101 packet streaming
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
#include <WSLoggerSettingsService.h>
#include <cc1101.h>

#define WEB_SOCKET_LOGGER_ORIGIN "wslogger"                   ///< WebSocket logger origin identifier
#define WEB_SOCKET_LOGGER_ORIGIN_CLIENT_ID_PREFIX "wslogger:" ///< Client ID prefix for logger connections
#define WEB_SOCKET_LOGGER_PATH "/ws/logger"                   ///< WebSocket endpoint path

/// WebSocket logger for streaming CC1101 packets to connected clients
class WSLogger
{
public:
    WSLogger(ESP32SvelteKit *sveltekit);

    /// Initialize WebSocket handler and settings
    void begin();

    /// Generate client ID for WebSocket connection
    String clientId(PsychicWebSocketClient *client);

    /// Log CC1101 packet to all connected WebSocket clients
    void logPacket(cc1101_packet_t *packet);

private:
    static constexpr const char *TAG = "WSLogger"; ///< Log tag for WebSocket logger

    SecurityManager *_securityManager;  ///< Security manager for connection filtering
    PsychicHttpServer *_server;         ///< HTTP server instance
    PsychicWebSocketHandler _webSocket; ///< WebSocket handler
    WSLoggerSettingsService _settings;  ///< Logger settings service

    /// Send client ID to newly connected client
    void transmitId(PsychicWebSocketClient *client);
};
