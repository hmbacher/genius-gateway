#include <CC1101Controller.h>
#include <cc1101.h>

CC1101Controller::CC1101Controller(ESP32SvelteKit *sveltekit) : _sveltekit(sveltekit),
                                                                _server(sveltekit->getServer()),
                                                                _securityManager(sveltekit->getSecurityManager()),
                                                                _lastGDO0Check(0),
                                                                _rxMonitorEnabled(false)
{
}

void CC1101Controller::begin()
{
    _sveltekit->addLoopFunction(std::bind(&CC1101Controller::loop, this));

    /* Register endpoints for CC1101 status */
    _server->on(CC1101CONTROLLER_SERVICE_PATH "/state",
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&CC1101Controller::_handlerGetStatus, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));

    /* Register endpoints to set the CC1101 ro RX state */
    _server->on(CC1101CONTROLLER_SERVICE_PATH "/rx",
                HTTP_POST,
                _securityManager->wrapRequest(std::bind(&CC1101Controller::_handlerSetRxState, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_ADMIN));
}

void CC1101Controller::loop()
{
    uint32_t currentMillis = millis();

    // --- 1s: Checking "GDO0-stuck-high" issue ---
    uint32_t timeElapsed = currentMillis - _lastGDO0Check;
    if (timeElapsed >= CC1101CONTROLLER_LOOP_PERIOD_MS)
    {
        _lastGDO0Check = currentMillis;

        uint32_t lastRisingEdge = cc1101_get_last_rising_edge();
        if (!(lastRisingEdge > 0)) // Already stored a rising edge?
            return;

        uint32_t current_time_ms = (unsigned long)(esp_timer_get_time() / 1000ULL);

        int gpio_level = gpio_get_level(static_cast<gpio_num_t>(CONFIG_GDO0_GPIO));
        if (gpio_level == 1 &&
            cc1101_get_mode() == CCM_RX &&
            current_time_ms - lastRisingEdge > CC1101CONTROLLER_MAX_GDO0_HIGH_DURATION_MS)
        {
            ESP_LOGW(TAG, "GDO0 was in high state longer than %d ms. Flushing RX FIFO and returning to RX mode.", CC1101CONTROLLER_MAX_GDO0_HIGH_DURATION_MS);
            cc1101_flush_rx_fifo();
            cc1101_set_rx_state();
            return;
        }
    }
}

esp_err_t CC1101Controller::_handlerGetStatus(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject json = response.getRoot();

    uint8_t cc1101_state = -1;
    bool success = false;
    bool action = (gpio_get_level(static_cast<gpio_num_t>(CONFIG_GDO0_GPIO)) == 1); // GDO0 high indicates an ongoing packet reception/transmission

    beginTransaction();

    if (_rxMonitorEnabled && !action)
    {
        success = (cc1101_get_state(&cc1101_state) == ESP_OK);
    }

    endTransaction();

    json["state_success"] = success;
    json["state"] = cc1101_state;

    return response.send();
}

esp_err_t CC1101Controller::_handlerSetRxState(PsychicRequest *request)
{
    return request->reply(cc1101_set_rx_state() == ESP_OK ? 200 : 500);
}