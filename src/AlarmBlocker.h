#pragma once

#include <ESP32SvelteKit.h>
#include <EventSocket.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <ThreadSafeService.h>

#define ALARMBLOCKER_SERVICE_PATH "/rest/endalarm"
#define ALARMBLOCKER_EVENT_REMAINING_BLOCK_TIME "rem-alarm-blck-t"  // Remaining block time in seconds
#define ALARMBLOCKER_LOOP_PERIOD_MS 1000 // 10 seconds

class AlarmBlocker : ThreadSafeService
{
public:
    AlarmBlocker(ESP32SvelteKit *sveltekit);

    void begin();
    void loop();

    void startBlocking(uint32_t seconds)
    {
        beginTransaction();
        _isBlocked = true;
        _remainingBlockingTimeMS = seconds * 1000; // Convert seconds to milliseconds
        endTransaction();
    }

    bool isBlocked()
    {
        bool isBlocked;
        beginTransaction();
        isBlocked = _isBlocked;
        endTransaction();
        return isBlocked;
    }

private:
    ESP32SvelteKit *_sveltekit;
    PsychicHttpServer *_server;
    SecurityManager *_securityManager;
    EventSocket *_eventSocket;

    volatile uint32_t _lastLooped; // Last time (millies) loop() was processed

    bool _isBlocked;
    uint32_t _remainingBlockingTimeMS; // in milliseconds

    void _emitRemainingBlockingTime();
};