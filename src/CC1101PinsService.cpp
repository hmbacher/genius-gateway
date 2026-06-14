/**
 * @file CC1101PinsService.cpp
 * @brief Implementation of the runtime CC1101 pin configuration service
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

#include <CC1101PinsService.h>

// Board identity for the UI: prefer the Home Assistant hardware-version string, fall back to the
// build target (PIOENV). Both are framework -D defines; HW_VERSION may be absent on generic builds.
#ifndef HW_VERSION
#define HW_VERSION ""
#endif
#ifndef BUILD_TARGET
#define BUILD_TARGET "unknown"
#endif

/* ESP32-S3 pin classification for expert mode. All current targets are ESP32-S3; revisit when
 * other chips are added. Reserved pins are hard-blocked in the UI; strapping pins are allowed
 * with a warning. */
static bool cc1101_pin_is_reserved(int n)
{
    if (n == 19 || n == 20) // USB D-/D+ (USB-Serial-JTAG)
        return true;
    if (n >= 26 && n <= 37) // SPI flash / PSRAM bus (conservative for octal variants)
        return true;
    return false;
}

static bool cc1101_pin_is_strapping(int n)
{
    return (n == 0 || n == 3 || n == 45 || n == 46);
}

bool CC1101Pins::pinsValid(const CC1101Pins &s)
{
    const int pins[5] = {s.csn, s.miso, s.mosi, s.sck, s.gdo0};

    // All pins must be set and distinct
    for (int i = 0; i < 5; i++)
    {
        if (pins[i] < 0)
            return false;
        for (int j = i + 1; j < 5; j++)
            if (pins[i] == pins[j])
                return false;
    }

    // Output pins (CSn/MOSI/SCK) must be output-capable; input pins (MISO/GDO0) must be valid
    if (!GPIO_IS_VALID_OUTPUT_GPIO(s.csn) || !GPIO_IS_VALID_OUTPUT_GPIO(s.mosi) || !GPIO_IS_VALID_OUTPUT_GPIO(s.sck))
        return false;
    if (!GPIO_IS_VALID_GPIO(s.miso) || !GPIO_IS_VALID_GPIO(s.gdo0))
        return false;

    if (s.spiHost != SPI2_HOST && s.spiHost != SPI3_HOST)
        return false;

    return true;
}

StateUpdateResult CC1101Pins::update(JsonObject &root, CC1101Pins &s, const String &originId)
{
    // Every target allows free ("Custom") pin assignment; named presets are just one-click
    // known-good sets the UI can apply. Validity is checked here; configured reflects it.
    CC1101Pins incoming;
    incoming.csn = root[CC1101_PINS_KEY_CSN] | s.csn;
    incoming.miso = root[CC1101_PINS_KEY_MISO] | s.miso;
    incoming.mosi = root[CC1101_PINS_KEY_MOSI] | s.mosi;
    incoming.sck = root[CC1101_PINS_KEY_SCK] | s.sck;
    incoming.gdo0 = root[CC1101_PINS_KEY_GDO0] | s.gdo0;
    incoming.spiHost = root[CC1101_PINS_KEY_SPI_HOST] | s.spiHost;

    bool changed = (incoming.csn != s.csn) || (incoming.miso != s.miso) || (incoming.mosi != s.mosi) ||
                   (incoming.sck != s.sck) || (incoming.gdo0 != s.gdo0) || (incoming.spiHost != s.spiHost);

    s.csn = incoming.csn;
    s.miso = incoming.miso;
    s.mosi = incoming.mosi;
    s.sck = incoming.sck;
    s.gdo0 = incoming.gdo0;
    s.spiHost = incoming.spiHost;
    // configured = pins are distinct and valid for the chip. Reserved/strapping guidance is
    // surfaced in the UI via /rest/cc1101/valid-pins; this is the authoritative backend gate.
    s.configured = pinsValid(s);

    if (changed)
        ESP_LOGI(TAG, "CC1101 pin config updated (configured=%d).", s.configured);

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}

CC1101PinsService::CC1101PinsService(ESP32SvelteKit *sveltekit) : _httpEndpoint(CC1101Pins::read,
                                                                                CC1101Pins::update,
                                                                                this,
                                                                                sveltekit->getServer(),
                                                                                CC1101_PINS_SERVICE_PATH,
                                                                                sveltekit->getSecurityManager(),
                                                                                AuthenticationPredicates::IS_ADMIN),
                                                                  _eventEndpoint(CC1101Pins::read,
                                                                                 CC1101Pins::update,
                                                                                 this,
                                                                                 sveltekit->getSocket(),
                                                                                 CC1101_PINS_EVENT),
                                                                  _fsPersistence(CC1101Pins::read,
                                                                                 CC1101Pins::update,
                                                                                 this,
                                                                                 sveltekit->getFS(),
                                                                                 CC1101_PINS_FILE),
                                                                  _server(sveltekit->getServer()),
                                                                  _securityManager(sveltekit->getSecurityManager())
{
    // Seed the in-memory defaults from the active board profile (single source of truth).
    // Overridden by readFromFS() in begin() when a persisted file exists.
    const cc1101_pins_t *d = cc1101_default_pins();
    _state.csn = d->csn;
    _state.miso = d->miso;
    _state.mosi = d->mosi;
    _state.sck = d->sck;
    _state.gdo0 = d->gdo0;
    _state.spiHost = d->spi_host;
    _state.configured = CC1101Pins::pinsValid(_state);
}

void CC1101PinsService::begin()
{
    _httpEndpoint.begin();
    _eventEndpoint.begin();
    _fsPersistence.readFromFS();

    // Read-only endpoint exposing the active board pin profile (catalog + capabilities) to the UI.
    _server->on(CC1101_PIN_PROFILE_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&CC1101PinsService::_handleGetProfile, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));

    // Chip's assignable GPIO set (runtime-derived) for expert mode pin selection.
    _server->on(CC1101_VALID_PINS_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&CC1101PinsService::_handleGetValidPins, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_ADMIN));
}

esp_err_t CC1101PinsService::_handleGetProfile(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();

    const cc1101_pin_profile_t *p = cc1101_active_profile();
    root["label"] = (HW_VERSION[0] != '\0') ? HW_VERSION : BUILD_TARGET;

    JsonArray presets = root["presets"].to<JsonArray>();
    for (size_t i = 0; i < p->preset_count; i++)
    {
        JsonObject pr = presets.add<JsonObject>();
        pr["name"] = p->presets[i].name;
        JsonObject pins = pr["pins"].to<JsonObject>();
        pins[CC1101_PINS_KEY_CSN] = p->presets[i].pins.csn;
        pins[CC1101_PINS_KEY_MISO] = p->presets[i].pins.miso;
        pins[CC1101_PINS_KEY_MOSI] = p->presets[i].pins.mosi;
        pins[CC1101_PINS_KEY_SCK] = p->presets[i].pins.sck;
        pins[CC1101_PINS_KEY_GDO0] = p->presets[i].pins.gdo0;
        pins[CC1101_PINS_KEY_SPI_HOST] = p->presets[i].pins.spi_host;
    }

    return response.send();
}

esp_err_t CC1101PinsService::_handleGetValidPins(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();

    JsonArray gpios = root["gpios"].to<JsonArray>();
    for (int n = 0; n < GPIO_NUM_MAX; n++)
    {
        if (!GPIO_IS_VALID_GPIO(n))
            continue;

        JsonObject g = gpios.add<JsonObject>();
        char label[12];
        snprintf(label, sizeof(label), "GPIO %d", n);
        g["num"] = n;
        g["label"] = label; // copied by ArduinoJson (non-const buffer)
        g["input"] = true;   // all valid ESP32-S3 GPIOs are input-capable
        g["output"] = (bool)GPIO_IS_VALID_OUTPUT_GPIO(n);
        g["reserved"] = cc1101_pin_is_reserved(n);
        g["strapping"] = cc1101_pin_is_strapping(n);
    }

    return response.send();
}
