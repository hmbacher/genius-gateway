/**
 * @file AlarmBlocker.cpp
 * @brief Implementation of the AlarmBlocker service
 * 
 * This file contains the implementation of the AlarmBlocker class methods
 * for managing temporary alarm blocking functionality.
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