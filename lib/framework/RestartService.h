#ifndef RestartService_h
#define RestartService_h

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *   Copyright (C) 2025 hmbacher
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <WiFi.h>

#include <ESPmDNS.h>
#include <PsychicHttp.h>
#include <SecurityManager.h>

#define RESTART_SERVICE_PATH "/rest/restart"

class RestartService
{
public:
    RestartService(PsychicHttpServer *server, SecurityManager *securityManager);

    void begin();

    /**
     * @brief Gracefully shuts down the HTTP server and restarts the ESP.
     *
     * Performs a controlled shutdown sequence:
     * 1. Stops the HTTP server (httpd_stop), which closes all sessions.
     *    With SO_LINGER enabled, each close() blocks until the TCP send
     *    buffer is flushed or the linger timeout (2s) expires.
     * 2. Stops mDNS.
     * 3. Disconnects WiFi and turns off the radio.
     * 4. Restarts the ESP.
     */
    static void restartNow();

private:
    static PsychicHttpServer *_server;
    SecurityManager *_securityManager;
    esp_err_t restart(PsychicRequest *request);
};

#endif // end RestartService_h
