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

#include <SystemStatus.h>
#include <esp32-hal.h>

#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/rtc.h"
#define ESP_TARGET "ESP32";
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32/rom/rtc.h"
#define ESP_TARGET "ESP32-S2";
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#define ESP_TARGET "ESP32-C3";
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#define ESP_TARGET "ESP32-S3";
#elif CONFIG_IDF_TARGET_ESP32C6
#include "esp32c6/rom/rtc.h"
#define ESP_TARGET "ESP32-C6";
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif

#ifndef ARDUINO_VERSION
#ifndef STRINGIZE
#define STRINGIZE(s) #s
#endif
#define ARDUINO_VERSION_STR(major, minor, patch) "v" STRINGIZE(major) "." STRINGIZE(minor) "." STRINGIZE(patch)
#define ARDUINO_VERSION ARDUINO_VERSION_STR(ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH)
#endif

namespace
{
struct ResetReasonText
{
    int reason; ///< esp_reset_reason_t
    const char *text;
};

// The texts follow the descriptions of esp_reset_reason_t. ESP_RST_USB and the reasons after it
// were added in later ESP-IDF versions; they are enum constants, so #ifdef cannot test for them.
// Their numbers are used instead, which compiles with every version.
const ResetReasonText RESET_REASON_TEXTS[] = {
    {ESP_RST_UNKNOWN, "Reset reason can not be determined"},
    {ESP_RST_POWERON, "Reset due to power-on event"},
    {ESP_RST_EXT, "Reset by external pin (not applicable for ESP32)"},
    {ESP_RST_SW, "Software reset via esp_restart"},
    {ESP_RST_PANIC, "Software reset due to exception/panic"},
    {ESP_RST_INT_WDT, "Reset (software or hardware) due to interrupt watchdog"},
    {ESP_RST_TASK_WDT, "Reset due to task watchdog"},
    {ESP_RST_WDT, "Reset due to other watchdogs"},
    {ESP_RST_DEEPSLEEP, "Reset after exiting deep sleep mode"},
    {ESP_RST_BROWNOUT, "Brownout reset (software or hardware)"},
    {ESP_RST_SDIO, "Reset over SDIO"},
    {11, "Reset by USB peripheral"},                     // ESP_RST_USB
    {12, "Reset by JTAG"},                               // ESP_RST_JTAG
    {13, "Reset due to efuse error"},                    // ESP_RST_EFUSE
    {14, "Reset due to power glitch detected"},          // ESP_RST_PWR_GLITCH
    {15, "Reset due to CPU lock up (double exception)"}, // ESP_RST_CPU_LOCKUP
};

/// For a code that is not in the table (a reason added by a newer ESP-IDF)
const char *const RESET_REASON_UNKNOWN_CODE = "Unknown reset reason";
} // namespace

const char *resetReasonText(int reason)
{
    for (const ResetReasonText &entry : RESET_REASON_TEXTS)
    {
        if (entry.reason == reason)
            return entry.text;
    }
    return RESET_REASON_UNKNOWN_CODE;
}

std::vector<const char *> resetReasonTexts()
{
    std::vector<const char *> texts;
    for (const ResetReasonText &entry : RESET_REASON_TEXTS)
        texts.push_back(entry.text);
    texts.push_back(RESET_REASON_UNKNOWN_CODE);
    return texts;
}

SystemStatus::SystemStatus(PsychicHttpServer *server,
                           SecurityManager *securityManager) : _server(server),
                                                               _securityManager(securityManager)
{
}

void SystemStatus::begin()
{
    _server->on(SYSTEM_STATUS_SERVICE_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&SystemStatus::systemStatus, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));

    ESP_LOGV(SVK_TAG, "Registered GET endpoint: %s", SYSTEM_STATUS_SERVICE_PATH);
}

esp_err_t SystemStatus::systemStatus(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();

    root["esp_platform"] = ESP_TARGET;
    root["firmware_version"] = APP_VERSION;
    root["build_target"] = BUILD_TARGET;
    root["max_alloc_heap"] = ESP.getMaxAllocHeap();
    if (psramFound())
    {
        root["free_psram"] = ESP.getFreePsram();
        root["used_psram"] = ESP.getPsramSize() - ESP.getFreePsram();
        root["psram_size"] = ESP.getPsramSize();
    }
    root["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
    root["cpu_type"] = ESP.getChipModel();
    root["cpu_rev"] = ESP.getChipRevision();
    root["cpu_cores"] = ESP.getChipCores();
    root["free_heap"] = ESP.getFreeHeap();
    root["used_heap"] = ESP.getHeapSize() - ESP.getFreeHeap();
    root["total_heap"] = ESP.getHeapSize();
    root["min_free_heap"] = ESP.getMinFreeHeap();
    root["sketch_size"] = ESP.getSketchSize();
    root["free_sketch_space"] = ESP.getFreeSketchSpace();
    root["sdk_version"] = ESP.getSdkVersion();
    root["arduino_version"] = ARDUINO_VERSION;
    root["flash_chip_size"] = ESP.getFlashChipSize();
    root["flash_chip_speed"] = ESP.getFlashChipSpeed();
    root["fs_total"] = ESPFS.totalBytes();
    root["fs_used"] = ESPFS.usedBytes();
    root["core_temp"] = temperatureRead();
    root["cpu_reset_reason"] = resetReasonText(esp_reset_reason());
    root["uptime"] = millis() / 1000;

    return response.send();
}
