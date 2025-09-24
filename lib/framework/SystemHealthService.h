#ifndef SystemHealthService_h
#define SystemHealthService_h

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

#include <PsychicHttp.h>
#include <EventSocket.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#define SYSTEM_HEALTH_EVENT_DELAY 5000  // 5 seconds
#define EVENT_SYSTEM_HEALTH "system_health"

class SystemHealthService
{
public:
    SystemHealthService(EventSocket *socket);

    void begin();
    void loop();

private:
    EventSocket *_socket;
    unsigned long _lastHealthUpdate;
    unsigned long _bootTime;
    unsigned long _lastResetReason;
    
    void updateSystemHealth();
    String getResetReasonString(esp_reset_reason_t reason);
    void logTaskInfo();
};

#endif