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

    volatile uint32_t _lastGDO0Check; ///< Last GDO0 state check timestamp (milliseconds)
    bool _rxMonitorEnabled;           ///< RX monitoring enabled flag

    /// HTTP handler for CC1101 status requests
    esp_err_t _handlerGetStatus(PsychicRequest *request);

    /// HTTP handler for setting CC1101 to RX state
    esp_err_t _handlerSetRxState(PsychicRequest *request);
};
