/**
 * @file AlarmBlocker.h
 * @brief AlarmBlocker service for temporarily blocking alarm notifications
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
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <ThreadSafeService.h>

#define ALARMBLOCKER_EVENT_REMAINING_BLOCK_TIME "rem-alarm-block-time" ///< WebSocket event for remaining block time updates
#define ALARMBLOCKER_LOOP_PERIOD_MS 1000                               ///< Loop processing period in milliseconds

/// Service for managing temporary alarm blocking functionality
class AlarmBlocker : public ThreadSafeService
{
public:
    AlarmBlocker(ESP32SvelteKit *sveltekit);

    /// Initialize the AlarmBlocker service
    void begin();

    /// Main service loop function
    void loop();

    /// Start blocking alarms for specified duration (seconds)
    void startBlocking(uint32_t seconds)
    {
        beginTransaction();
        _isBlocked = true;
        _remainingBlockingTimeMS = seconds * 1000; // Convert seconds to milliseconds
        endTransaction();
    }

    /// Immediately end alarm blocking
    esp_err_t endBlocking()
    {
        beginTransaction();
        _isBlocked = false;
        _remainingBlockingTimeMS = 0;
        endTransaction();

        return ESP_OK;
    }

    /// Check if alarms are currently blocked
    bool isBlocked()
    {
        bool isBlocked;
        beginTransaction();
        isBlocked = _isBlocked;
        endTransaction();
        return isBlocked;
    }

private:
    static constexpr const char *TAG = "AlarmBlocker"; ///< Logging tag

    ESP32SvelteKit *_sveltekit;        ///< ESP32SvelteKit framework instance
    PsychicHttpServer *_server;        ///< HTTP server instance
    SecurityManager *_securityManager; ///< Security manager instance
    EventSocket *_eventSocket;         ///< WebSocket event manager

    volatile uint32_t _lastLooped;     ///< Last loop processing time (ms)
    bool _isBlocked;                   ///< Current blocking state
    uint32_t _remainingBlockingTimeMS; ///< Remaining blocking time (ms)

    /// Emit remaining blocking time via WebSocket (thread-safe)
    void _emitRemainingBlockingTime();
};