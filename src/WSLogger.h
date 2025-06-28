#pragma once

#include <ESP32SvelteKit.h>
#include <WSLoggerSettingsService.h>
#include <cc1101.h>

#define WEB_SOCKET_LOGGER_ORIGIN "wslogger"
#define WEB_SOCKET_LOGGER_ORIGIN_CLIENT_ID_PREFIX "wslogger:"
#define WEB_SOCKET_LOGGER_PATH "/ws/logger"

class WSLogger
{
public:
    WSLogger(ESP32SvelteKit *sveltekit);
    
    void begin();
    String clientId(PsychicWebSocketClient *client);
    void logPacket(cc1101_packet_t *packet);

private:
    static constexpr const char *TAG = "WSLogger";

    SecurityManager *_securityManager;
    PsychicHttpServer *_server;
    PsychicWebSocketHandler _webSocket;
    WSLoggerSettingsService _settings;

    void transmitId(PsychicWebSocketClient *client);
};

