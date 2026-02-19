#pragma once

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2025 theelims
 *   Copyright (C) 2026 hmbacher
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <Arduino.h>
#include <HomeAssistant/HAService.h>
#include <RestartService.h>

/**
 * @brief Home Assistant diagnostic entities service
 *
 * Provides standard diagnostic entities for any ESP32 device:
 * - Status sensor (device registration, online state)
 * - Free heap memory sensor (percentage)
 * - Core temperature sensor (°C)
 * - Restart button
 *
 * Updates diagnostic sensor values periodically via FreeRTOS timer.
 * Registers itself with HAService via onPublishAll() for automatic
 * entity publishing on MQTT connect.
 */
class HADiagnosticService
{
public:
    static constexpr const char *TAG = "HADiagnosticSvc";

    /// Diagnostic sensor update interval (1 minute)
    static constexpr uint32_t DIAGNOSTIC_SENSOR_UPDATE_INTERVAL_MS = 1 * 60 * 1000;

    /**
     * @brief Constructor
     * @param haService Shared HAService for device identity and MQTT helpers
     */
    HADiagnosticService(HAService *haService);

    /**
     * @brief Initialize the service
     *
     * Creates diagnostic sensor timer and registers with HAService
     * for entity publishing on MQTT connect.
     */
    void begin();

    /**
     * @brief Publish all diagnostic entity configs and current states
     *
     * Called by HAService::publishAll() on MQTT connect.
     */
    void publishAll();

private:
    HAService *_haService;
    TimerHandle_t _diagnosticSensorTimer;

    // ========================================================================
    // Entity publishing
    // ========================================================================

    /** @brief Publish status sensor (registers the device in HA) */
    void _publishStatusSensor();

    /** @brief Publish free heap memory sensor config */
    void _publishHeapSensor();

    /** @brief Publish core temperature sensor config */
    void _publishTempSensor();

    /** @brief Publish current diagnostic sensor values */
    void _publishDiagnosticSensorStates();

    /** @brief Publish restart button entity config */
    void _publishRestartButton();

    // ========================================================================
    // Command handlers
    // ========================================================================

    /** @brief Subscribe to command topics */
    void _subscribeToCommands();

    /** @brief Handle restart command from HA */
    void _onRestartCommand(char *topic, char *payload, int retain, int qos, bool dup);

    // ========================================================================
    // Timer
    // ========================================================================

    /** @brief Static timer callback for periodic diagnostic updates */
    static void _diagnosticSensorTimerCallback(TimerHandle_t timer);
};
