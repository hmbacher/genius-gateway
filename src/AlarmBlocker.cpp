/**
 * @file AlarmBlocker.cpp
 * @brief Implementation of the AlarmBlocker service
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

#include <AlarmBlocker.h>

AlarmBlocker::AlarmBlocker(ESP32SvelteKit *sveltekit) : _sveltekit(sveltekit),
                                                        _server(sveltekit->getServer()),
                                                        _securityManager(sveltekit->getSecurityManager()),
                                                        _eventSocket(sveltekit->getSocket()),
                                                        _lastLooped(0),
                                                        _isBlocked(false),
                                                        _remainingBlockingTimeMS(0)
{
}

void AlarmBlocker::begin()
{
    _eventSocket->registerEvent(ALARMBLOCKER_EVENT_REMAINING_BLOCK_TIME);
    _sveltekit->addLoopFunction(std::bind(&AlarmBlocker::loop, this));
}

void AlarmBlocker::loop()
{
    uint32_t currentMillis = millis();
    uint32_t timeElapsed = currentMillis - _lastLooped;

    // Process only if enough time has elapsed
    if (timeElapsed >= ALARMBLOCKER_LOOP_PERIOD_MS)
    {
        _lastLooped = currentMillis;

        beginTransaction();
        if (_isBlocked)
        {
            // Decrease remaining alarm blocking time
            if (_remainingBlockingTimeMS >= timeElapsed)
                _remainingBlockingTimeMS -= timeElapsed;
            else
            {
                // Time has expired, unblock alarms
                _remainingBlockingTimeMS = 0;
                _isBlocked = false;
                ESP_LOGI(TAG, "Alarm blocking ended due to time expiration.");
            }

            // Emit current status to WebSocket clients
            _emitRemainingBlockingTime();
        }
        endTransaction();
    }
}

void AlarmBlocker::_emitRemainingBlockingTime()
{
    // Create JSON object with current blocking status
    JsonDocument jsonDoc;
    JsonObject jsonRoot = jsonDoc.to<JsonObject>();

    beginTransaction();
    jsonRoot["isBlocked"] = _isBlocked;
    jsonRoot["remainingBlockingTimeS"] = _remainingBlockingTimeMS / 1000; // Convert milliseconds to seconds
    endTransaction();

    // Send status update via WebSocket
    _eventSocket->emitEvent(ALARMBLOCKER_EVENT_REMAINING_BLOCK_TIME, jsonRoot);
}