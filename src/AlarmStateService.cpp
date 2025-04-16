#include <AlarmStateService.h>

AlarmStateService::AlarmStateService(ESP32SvelteKit *sveltekit) : _socket(sveltekit->getSocket()),
                                                                  _server(sveltekit->getServer()),
                                                                  _securityManager(sveltekit->getSecurityManager())
{
}

void AlarmStateService::begin()
{
    _server->on(ALARMSTATE_SERVICE_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(
                    [this](PsychicRequest *request)
                    {
                        PsychicJsonResponse response = PsychicJsonResponse(request, false);
                        JsonObject jsonObject = response.getRoot();
                        this->read(jsonObject, AlarmState::read);
                        return response.send();
                    },
                    AuthenticationPredicates::IS_AUTHENTICATED));

    ESP_LOGV(AlarmStateService::TAG, "Registered GET endpoint: %s", ALARMSTATE_SERVICE_PATH);

    _socket->registerEvent(EVENT_ALARM);

    // Start the loop task
    ESP_LOGV(AlarmStateService::TAG, "Starting test loop task");
    xTaskCreatePinnedToCore(
        this->_loopImpl,            // Function that should be called
        "AlarmTestLoop",        	// Name of the task (for debugging)
        4096,                       // Stack size (bytes)
        this,                       // Pass reference to this class instance
        (tskIDLE_PRIORITY + 2),     // task priority
        NULL,                       // Task handle
        ESP32SVELTEKIT_RUNNING_CORE // Pin to application core
    );
}

// For testing purposes
void AlarmStateService::_loop()
{
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    {   
        // HACKY!!!
        beginTransaction();
        if (_state._alarmingDevices.size() > 0)
            _state._alarmingDevices.clear();
        else
            _state._alarmingDevices.push_back(2222699880);
        endTransaction();
        // HACKY!!!

        _emitAlarm();

        vTaskDelayUntil(&xLastWakeTime, 10000 / portTICK_PERIOD_MS);
    }
}

void AlarmStateService::_emitAlarm()
{
    /* Prepare event data (payload) */
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    read(root, AlarmState::read);

    /* Emit event */
    _socket->emitEvent(EVENT_ALARM, root);
}