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
                _remainingBlockingTimeMS = 0;
                _isBlocked = false; // Unblock when time runs out
            }
        }
        endTransaction();

        // Emit WS event with remaining block time
        _emitRemainingBlockingTime();
    }
}

void AlarmBlocker::_emitRemainingBlockingTime()
{
    JsonDocument jsonDoc;
    JsonObject jsonRoot = jsonDoc.to<JsonObject>();

    beginTransaction();
    jsonRoot["isBlocked"] = _isBlocked;
    jsonRoot["remainingBlockingTime"] = _remainingBlockingTimeMS / 1000; // Convert milliseconds to seconds
    endTransaction();

    _eventSocket->emitEvent(ALARMBLOCKER_EVENT_REMAINING_BLOCK_TIME, jsonRoot);
}