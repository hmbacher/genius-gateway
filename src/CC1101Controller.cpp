#include <CC1101Controller.h>
#include <cc1101.h>

CC1101Controller::CC1101Controller(ESP32SvelteKit *sveltekit) : _sveltekit(sveltekit),
                                                                _server(sveltekit->getServer()),
                                                                _securityManager(sveltekit->getSecurityManager())
                                                                //_eventSocket(sveltekit->getSocket()),
                                                                //_lastEmitted(0)
{
}

void CC1101Controller::begin()
{
    //_eventSocket->registerEvent(CC1101CONTROLLER_EVENT_STATE);

    //_sveltekit->addLoopFunction(std::bind(&CC1101Controller::loop, this));

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

esp_err_t CC1101Controller::_handlerGetStatus(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject json = response.getRoot();

    uint8_t cc1101_state = 0;
    esp_err_t res = cc1101_get_state(&cc1101_state);
    json["state_success"] = (res == ESP_OK);
    if (res == ESP_OK)
        json["state"] = cc1101_state;

    return response.send();
}

esp_err_t CC1101Controller::_handlerSetRxState(PsychicRequest *request)
{
    return request->reply(cc1101_set_rx_state() == ESP_OK ? 200 : 500);
}