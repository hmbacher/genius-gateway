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
#include <map>

#define WEB_SOCKET_LOGGER_ORIGIN "wslogger"                   ///< WebSocket logger origin identifier
#define WEB_SOCKET_LOGGER_ORIGIN_CLIENT_ID_PREFIX "wslogger:" ///< Client ID prefix for logger connections
#define WEB_SOCKET_LOGGER_PATH "/ws/logger"                   ///< WebSocket endpoint path
#define WEB_SOCKET_LOGGER_SEND_TIMEOUT_MS 1000                ///< Longest wait for one client to take a packet
#define WEB_SOCKET_LOGGER_CLIENTS_PATH "/rest/wslogger/clients" ///< REST endpoint listing the connected clients
#define WEB_SOCKET_LOGGER_CLIENT_PARAM "client"               ///< Query parameter a client can name itself with
#define WEB_SOCKET_LOGGER_CLIENT_NAME_MAX 32                  ///< Longest client name kept
#define WEB_SOCKET_LOGGER_USER_AGENT_MAX 160                  ///< Longest User-Agent kept per client

/// What the logger knows about one connected client
struct WSLoggerClient
{
    String address;           ///< Remote IP address
    uint16_t port = 0;        ///< Remote TCP port
    String name;              ///< Name the client gave itself (?client=...), empty if none
    String userAgent;         ///< User-Agent of the connection request, empty if none
    uint32_t connectedAt = 0; ///< millis() when the connection was opened
    uint32_t packets = 0;     ///< Radio packets sent to the client
    uint64_t bytes = 0;       ///< Payload bytes of these packets
};

/// WebSocket logger for streaming CC1101 packets to connected clients
class WSLogger
{
public:
    WSLogger(ESP32SvelteKit *sveltekit);

    /// Initialize WebSocket handler and settings
    void begin();

    /// Generate client ID for WebSocket connection
    String clientId(PsychicWebSocketClient *client);

    /**
     * @brief Log CC1101 packet to all connected WebSocket clients
     *
     * Called from the packet reception task for every packet. A client that does not take
     * the packet within WEB_SOCKET_LOGGER_SEND_TIMEOUT_MS is disconnected, so reception
     * stands still for that time at most, once per unresponsive client.
     */
    void logPacket(cc1101_packet_t *packet);

    /**
     * @brief Write the connected clients to JSON
     *
     * root: { enabled, clients: [ { id, address, port, name?, userAgent?, connectedMs, packets, bytes } ] }
     * connectedMs is the age of the connection, so it needs no clock.
     */
    void clientsToJson(JsonObject &root);

private:
    static constexpr const char *TAG = "WSLogger"; ///< Log tag for WebSocket logger

    SecurityManager *_securityManager;  ///< Security manager for connection filtering
    PsychicHttpServer *_server;         ///< HTTP server instance
    PsychicWebSocketHandler _webSocket; ///< WebSocket handler
    WSLoggerSettingsService _settings;  ///< Logger settings service
    std::vector<int> _droppedSockets;   ///< Clients being disconnected after a failed send (reception task only)

    SemaphoreHandle_t _clientsMutex;              ///< Guards _clients and _pendingClients (HTTP and reception task)
    std::map<int, WSLoggerClient> _clients;        ///< Connected clients by socket
    std::map<int, WSLoggerClient> _pendingClients; ///< Name / User-Agent of accepted requests, until the socket opens

    /// Send client ID to newly connected client
    void transmitId(PsychicWebSocketClient *client);

    /// Remember name and User-Agent of an accepted connection request for its socket
    void _rememberRequest(PsychicRequest *request);

    /// Close every connected client (the logger was switched off)
    void _closeAll();

    /// A client name as given in the query: printable ASCII only, shortened
    static String _cleanName(const String &raw);
};
