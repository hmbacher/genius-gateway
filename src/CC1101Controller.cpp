/**
 * @file CC1101Controller.cpp
 * @brief Implementation of the CC1101 radio controller service
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

#include <CC1101Controller.h>
#include <CC1101PinsService.h>
#include <cc1101.h>

CC1101Controller::CC1101Controller(ESP32SvelteKit *sveltekit) : _sveltekit(sveltekit),
                                                                _server(sveltekit->getServer()),
                                                                _securityManager(sveltekit->getSecurityManager()),
                                                                _eventSocket(sveltekit->getSocket()),
                                                                _lastGDO0Check(0),
                                                                _rxMonitorEnabled(false),
                                                                _radioState(CC1101_RADIO_UNCONFIGURED),
                                                                _transmitting(false),
                                                                _pinsService(nullptr),
                                                                _rxCallback(nullptr)
{
}

void CC1101Controller::begin()
{
    _sveltekit->addLoopFunction(std::bind(&CC1101Controller::loop, this));

    // Register endpoint for CC1101 status
    _server->on(CC1101CONTROLLER_SERVICE_PATH "/state",
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&CC1101Controller::_handlerGetStatus, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));

    // Register endpoint to set the CC1101 to RX state
    _server->on(CC1101CONTROLLER_SERVICE_PATH "/rx",
                HTTP_POST,
                _securityManager->wrapRequest(std::bind(&CC1101Controller::_handlerSetRxState, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_ADMIN));

    // Radio lifecycle status: live event + REST snapshot
    _eventSocket->registerEvent(CC1101_STATUS_EVENT);
    _server->on(CC1101CONTROLLER_SERVICE_PATH "/status",
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&CC1101Controller::_handlerGetRadioStatus, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));

    // Pin self-test (probe candidate pins). Only valid while the radio is not running.
    _server->on(CC1101CONTROLLER_SERVICE_PATH "/probe",
                HTTP_POST,
                _securityManager->wrapCallback(std::bind(&CC1101Controller::_handleProbe, this, std::placeholders::_1, std::placeholders::_2),
                                               AuthenticationPredicates::IS_ADMIN));
}

esp_err_t CC1101Controller::_handleProbe(PsychicRequest *request, JsonVariant &json)
{
    if (!json.is<JsonObject>())
        return request->reply(400, "application/json", "{\"success\":false,\"reason\":\"Invalid JSON\"}");

    JsonObject in = json.as<JsonObject>();

    cc1101_pins_t cand;
    cand.csn = in["csn"] | -1;
    cand.miso = in["miso"] | -1;
    cand.mosi = in["mosi"] | -1;
    cand.sck = in["sck"] | -1;
    cand.gdo0 = in["gdo0"] | -1;
    cand.spi_host = in["spi_host"] | (int)SPI2_HOST;

    // Validate before touching GPIO/SPI (prevents asserts on -1 pins). Allowlist/expert checks
    // are a UI concern; the probe accepts any valid, distinct GPIO set.
    CC1101Pins check;
    check.csn = cand.csn;
    check.miso = cand.miso;
    check.mosi = cand.mosi;
    check.sck = cand.sck;
    check.gdo0 = cand.gdo0;
    check.spiHost = cand.spi_host;
    if (!CC1101Pins::pinsValid(check))
        return request->reply(400, "application/json", "{\"success\":false,\"reason\":\"Invalid pin set (pins must be distinct and valid).\"}");

    cc1101_probe_result_t result;

    ESP_LOGI(TAG, "=== Pin self-test START === candidate CSN=%d SCK=%d MOSI=%d MISO=%d GDO0=%d "
                  "(any errors logged below are part of this test, not normal operation)",
             cand.csn, cand.sck, cand.mosi, cand.miso, cand.gdo0);

    // Hold the controller mutex across probe + restore so the monitoring loop/handlers can't
    // interleave; the driver lock inside cc1101_probe serializes the teardown against the RX task.
    beginTransaction();
    bool wasOk = (_radioState == CC1101_RADIO_OK);
    cc1101_radio_state_t prev = _radioState;
    if (wasOk)
        disableRXMonitoring();
    esp_err_t ret = cc1101_probe(&cand, &result);

    bool passed = (ret == ESP_OK && result.spi_ok && result.chip_detected && result.gdo0_ok);
    if (passed)
        ESP_LOGI(TAG, "=== Pin self-test PASSED === chip v0x%02x detected, GDO0 verified", result.version);
    else
        ESP_LOGW(TAG, "=== Pin self-test FAILED === spi_ok=%d chip_detected=%d gdo0_ok=%d",
                 result.spi_ok, result.chip_detected, result.gdo0_ok);

    if (wasOk)
    {
        // The probe tore the running radio down — restore the previously working configuration.
        // The "set up successfully" line that follows is this restore, NOT the test result.
        ESP_LOGI(TAG, "Restoring the previous radio configuration after the test...");
        bringUp(_pinsService, _rxCallback);
    }
    else
    {
        // Radio was already down (unconfigured/error); just re-assert the prior state.
        _setRadioState(prev);
    }
    endTransaction();

    if (ret != ESP_OK)
        return request->reply(400, "application/json", "{\"success\":false,\"reason\":\"Probe failed to run.\"}");

    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    root["success"] = true;
    root["spi_ok"] = result.spi_ok;
    root["chip_detected"] = result.chip_detected;
    root["gdo0_ok"] = result.gdo0_ok;
    root["partnum"] = result.partnum;
    root["version"] = result.version;
    return response.send();
}

void CC1101Controller::bringUp(CC1101PinsService *pinsService, void (*rxCallback)())
{
    _pinsService = pinsService;
    _rxCallback = rxCallback;

    // Pause monitoring during (re-)initialization; re-enabled on success.
    disableRXMonitoring();

    if (pinsService == nullptr || !pinsService->isConfigured())
    {
        ESP_LOGW(TAG, "CC1101 not configured; radio left unconfigured.");
        _setRadioState(CC1101_RADIO_UNCONFIGURED);
        return;
    }

    _setRadioState(CC1101_RADIO_INITIALIZING);

    cc1101_pins_t pins = pinsService->getPins();
    esp_err_t ret = cc1101_init(&pins, rxCallback);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "CC1101 set up successfully.");
        _setRadioState(CC1101_RADIO_OK);
        enableRXMonitoring();
    }
    else
    {
        ESP_LOGE(TAG, "CC1101 could not be set up (err=0x%x).", ret);
        _setRadioState(CC1101_RADIO_ERROR);
    }
}

void CC1101Controller::reconfigure()
{
    if (_pinsService == nullptr)
        return;

    // Serialize against the monitoring loop/handlers; the driver lock serializes the actual
    // teardown/re-init against the RX task. A live radio is torn down and rebuilt with the new
    // pins (no reboot). Stays UNCONFIGURED/ERROR if the new pins are unusable.
    beginTransaction();
    ESP_LOGI(TAG, "Reconfiguring CC1101 from updated pin configuration.");
    bringUp(_pinsService, _rxCallback);
    endTransaction();
}

void CC1101Controller::_setRadioState(cc1101_radio_state_t state)
{
    _radioState = state;
    _emitStatus();
}

const char *CC1101Controller::_stateName(cc1101_radio_state_t state)
{
    switch (state)
    {
    case CC1101_RADIO_UNCONFIGURED:
        return "unconfigured";
    case CC1101_RADIO_INITIALIZING:
        return "initializing";
    case CC1101_RADIO_OK:
        return "ok";
    case CC1101_RADIO_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

void CC1101Controller::_emitStatus()
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["state"] = _stateName(_radioState);
    // When OK, the radio is listening (RX) unless the TX path has flagged a send in progress.
    root["mode"] = _transmitting ? "tx" : "rx";
    root["configured"] = (_pinsService != nullptr && _pinsService->isConfigured());
    _eventSocket->emitEvent(CC1101_STATUS_EVENT, root);
}

void CC1101Controller::setTransmitting(bool transmitting)
{
    if (_transmitting == transmitting)
        return;
    _transmitting = transmitting;
    _emitStatus();
}

esp_err_t CC1101Controller::_handlerGetRadioStatus(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    root["state"] = _stateName(_radioState);
    root["mode"] = _transmitting ? "tx" : "rx";
    root["configured"] = (_pinsService != nullptr && _pinsService->isConfigured());
    return response.send();
}

void CC1101Controller::loop()
{
    // GDO0 monitoring only makes sense once the radio is up and pins are valid
    if (_radioState != CC1101_RADIO_OK)
        return;

    uint32_t currentMillis = millis();

    // Check for GDO0 stuck-high issue every second
    uint32_t timeElapsed = currentMillis - _lastGDO0Check;
    if (timeElapsed >= CC1101CONTROLLER_LOOP_PERIOD_MS)
    {
        _lastGDO0Check = currentMillis;

        uint32_t lastRisingEdge = cc1101_get_last_rising_edge();
        if (!(lastRisingEdge > 0)) // No rising edge stored yet
            return;

        uint32_t current_time_ms = (unsigned long)(esp_timer_get_time() / 1000ULL);

        int gpio_level = gpio_get_level(static_cast<gpio_num_t>(cc1101_get_gdo0_pin()));
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

    // Radio not up: no valid pins to read, report failure cleanly
    if (_radioState != CC1101_RADIO_OK)
    {
        json["state_success"] = false;
        json["state"] = -1;
        return response.send();
    }

    uint8_t cc1101_state = -1;
    bool success = false;
    bool action = (gpio_get_level(static_cast<gpio_num_t>(cc1101_get_gdo0_pin())) == 1); // GDO0 high indicates ongoing packet reception/transmission

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