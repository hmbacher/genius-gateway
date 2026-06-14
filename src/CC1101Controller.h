/**
 * @file CC1101Controller.h
 * @brief CC1101 radio controller service for RF communication management
 * 
 * @copyright Copyright (c) 2024-2025 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 * 
 * This file is part of Genius Gateway.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the Commons Clause restriction.
 * 
 * "Commons Clause" License Condition v1.0
 * The Software is provided to you by the Licensor under the License,
 * as defined below, subject to the following condition:
 * Without limiting other conditions in the License, the grant of rights
 * under the License will not include, and the License does not grant to you,
 * the right to Sell the Software.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 * 
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#pragma once

#include <ESP32SvelteKit.h>
#include <EventSocket.h>
#include <FSPersistence.h>
#include <HttpEndpoint.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <Utils.hpp>
#include <cc1101.h>
#include <ThreadSafeService.h>

#define CC1101CONTROLLER_SERVICE_PATH "/rest/cc1101"   ///< REST API service endpoint path
#define CC1101CONTROLLER_LOOP_PERIOD_MS 1000           ///< Loop processing period (1 second)
#define CC1101CONTROLLER_RX_MONITOR_PERIOD_MS 60000    ///< RX monitoring period (1 minute)
#define CC1101CONTROLLER_MAX_GDO0_HIGH_DURATION_MS 200 ///< Maximum duration for GDO0 high state (milliseconds)
#define CC1101_STATUS_EVENT "cc1101_status"            ///< WebSocket event name for radio lifecycle state

class CC1101PinsService; // forward declaration (radio pin configuration source)

/// High-level lifecycle state of the radio (distinct from the chip's internal MARCSTATE)
typedef enum cc1101_radio_state
{
    CC1101_RADIO_UNCONFIGURED = 0, ///< No valid pin configuration; radio not brought up
    CC1101_RADIO_INITIALIZING,     ///< Bring-up in progress
    CC1101_RADIO_OK,               ///< Radio initialized and operating
    CC1101_RADIO_ERROR             ///< Bring-up failed (e.g. chip not found on configured pins)
} cc1101_radio_state_t;

/// CC1101 radio controller service for RF communication management
class CC1101Controller : public ThreadSafeService
{
public:
    static constexpr const char *TAG = "CC1101Controller"; ///< Logging tag

    CC1101Controller(ESP32SvelteKit *sveltekit);

    /// Initialize the CC1101 controller service
    void begin();

    /// Main service loop for GDO0 monitoring and RX state management
    void loop();

    /**
     * @brief Bring up the radio from the persisted pin configuration.
     *
     * Reads the pins from @p pinsService: if a valid set is configured, initializes the radio
     * and enables RX monitoring (state OK), otherwise leaves the radio off (state UNCONFIGURED).
     * On init failure the state becomes ERROR. The lifecycle state is emitted to the Web UI.
     *
     * @param pinsService Source of the runtime pin configuration
     * @param rxCallback ISR callback invoked on a fully received packet
     */
    void bringUp(CC1101PinsService *pinsService, void (*rxCallback)());

    /// Current radio lifecycle state
    cc1101_radio_state_t getRadioState() const { return _radioState; }

    /// Re-initialize the radio from the (updated) persisted pin configuration, live (no reboot).
    void reconfigure();

    /// Mark the radio as transmitting (true) or back to listening (false). Pushes a status
    /// update so the Web UI can show TX vs RX live. Called by the TX path around a send burst.
    void setTransmitting(bool transmitting);

    /// Enable RX monitoring functionality
    void enableRXMonitoring()
    {
        beginTransaction();
        _rxMonitorEnabled = true;
        endTransaction();
    }

    /// Disable RX monitoring functionality
    void disableRXMonitoring()
    {
        beginTransaction();
        _rxMonitorEnabled = false;
        endTransaction();
    }

private:
    ESP32SvelteKit *_sveltekit;        ///< ESP32SvelteKit framework instance
    PsychicHttpServer *_server;        ///< HTTP server instance
    SecurityManager *_securityManager; ///< Security manager instance
    EventSocket *_eventSocket;         ///< WebSocket event manager (radio status push)

    volatile uint32_t _lastGDO0Check;            ///< Last GDO0 state check timestamp (milliseconds)
    bool _rxMonitorEnabled;                      ///< RX monitoring enabled flag
    volatile cc1101_radio_state_t _radioState;   ///< Radio lifecycle state
    volatile bool _transmitting;                 ///< true while the TX path is sending (UI shows TX vs RX)
    CC1101PinsService *_pinsService;             ///< Pin configuration source (set in bringUp)
    void (*_rxCallback)();                       ///< ISR packet callback (set in bringUp)

    /// Update the lifecycle state and emit it to the Web UI
    void _setRadioState(cc1101_radio_state_t state);

    /// Emit the current radio lifecycle state over the EventSocket.
    /// Pass an originId to send only to that subscriber; omit to broadcast.
    void _emitStatus(const String &originId = "");

    /// String name for a radio lifecycle state (UI-facing)
    static const char *_stateName(cc1101_radio_state_t state);

    /// HTTP handler for CC1101 chip MARCSTATE requests
    esp_err_t _handlerGetStatus(PsychicRequest *request);

    /// HTTP handler for the radio lifecycle status (state + configured)
    esp_err_t _handlerGetRadioStatus(PsychicRequest *request);

    /// HTTP handler for setting CC1101 to RX state
    esp_err_t _handlerSetRxState(PsychicRequest *request);

    /// HTTP handler for the pin self-test (probe candidate pins, radio must not be running)
    esp_err_t _handleProbe(PsychicRequest *request, JsonVariant &json);
};
