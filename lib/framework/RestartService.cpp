/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <RestartService.h>

// Static member initialization
PsychicHttpServer *RestartService::_server = nullptr;

RestartService::RestartService(PsychicHttpServer *server, SecurityManager *securityManager) : _securityManager(securityManager)
{
    _server = server;
}

void RestartService::begin()
{
    _server->on(RESTART_SERVICE_PATH,
                HTTP_POST,
                _securityManager->wrapRequest(std::bind(&RestartService::restart, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_ADMIN));

    ESP_LOGV(SVK_TAG, "Registered POST endpoint: %s", RESTART_SERVICE_PATH);
}

void RestartService::restartNow()
{
    // Give any in-flight send() calls a moment to copy data into the TCP buffer
    delay(100);

    // Gracefully stop the HTTP server.
    // httpd_stop() → httpd_sess_close_all() → httpd_sess_delete() per session:
    //   - With SO_LINGER enabled (2s), close() blocks until pcb->unsent and
    //     pcb->unacked are empty, i.e. the TCP stack has transmitted all data
    //     AND received ACKs from the client — or the timeout expires.
    //   - This ensures WebSocket events and HTTP responses are fully delivered.
    if (_server)
    {
        ESP_LOGI("RestartService", "Stopping HTTP server (graceful TCP flush via SO_LINGER)...");
        _server->stop();
        ESP_LOGI("RestartService", "HTTP server stopped");
    }

    MDNS.end();
    WiFi.disconnect(true);
    delay(100);
    ESP.restart();
}

esp_err_t RestartService::restart(PsychicRequest *request)
{
    request->reply(200);
    restartNow();
    return ESP_OK;
}
