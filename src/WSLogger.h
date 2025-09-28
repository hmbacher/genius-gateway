/// @file WSLogger.h
/// @brief WebSocket logger for CC1101 packet streaming

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
