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

#define CC1101CONTROLLER_SERVICE_PATH "/rest/cc1101"

#define CC1101CONTROLLER_LOOP_PERIOD_MS 1000 // 1 second
#define CC1101CONTROLLER_RX_MONITOR_PERIOD_MS 60000 // 1 minute

#define CC1101CONTROLLER_MAX_GDO0_HIGH_DURATION_MS 200 // Maximum duration for GDO0 high state in milliseconds

class CC1101Controller: ThreadSafeService
{
public:
    static constexpr const char *TAG = "CC1101Controller";

    CC1101Controller(ESP32SvelteKit *sveltekit);

    void begin();
    void loop();

    void enableRXMonitoring()
    {
        beginTransaction();
        _rxMonitorEnabled = true;
        endTransaction();
    }

    void disableRXMonitoring()
    {
        beginTransaction();
        _rxMonitorEnabled = false;
        endTransaction();
    }

private:
    ESP32SvelteKit *_sveltekit;
    PsychicHttpServer *_server;
    SecurityManager *_securityManager;

    volatile uint32_t _lastGDO0Check; // Last time (millies) the state was emitted

    bool _rxMonitorEnabled;

    esp_err_t _handlerGetStatus(PsychicRequest *request);
    esp_err_t _handlerSetRxState(PsychicRequest *request);
};
