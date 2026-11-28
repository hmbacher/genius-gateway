#ifndef SystemStatus_h
#define SystemStatus_h

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

#include <WiFi.h>

#include <ArduinoJson.h>
#include <PsychicHttp.h>
#include <SecurityManager.h>
#include <ESPFS.h>
#include <vector>

#define SYSTEM_STATUS_SERVICE_PATH "/rest/systemStatus"

/**
 * Text for a reset reason (esp_reset_reason()), the same wherever the reason is shown: in the
 * system status and in Home Assistant. It is one of a fixed set of texts; a code that is not
 * known gets a text of its own.
 */
const char *resetReasonText(int reason);

/// Every text resetReasonText() can return, e.g. as the options of an enum sensor.
std::vector<const char *> resetReasonTexts();

class SystemStatus
{
public:
    SystemStatus(PsychicHttpServer *server, SecurityManager *securityManager);

    void begin();

private:
    PsychicHttpServer *_server;
    SecurityManager *_securityManager;
    esp_err_t systemStatus(PsychicRequest *request);
};

#endif // end SystemStatus_h
