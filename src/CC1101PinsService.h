/**
 * @file CC1101PinsService.h
 * @brief Persisted, runtime-configurable CC1101 SPI/GDO pin assignment.
 *
 * Stores the user/seed pin configuration and serves it (and the active board pin profile)
 * to the Web UI. The pins seeded here come from the active board profile
 * (the board pin-profile table in cc1101.c) - the single source of truth - and are overridden by the persisted
 * file when present. Boot-time consumption of these pins is wired in a later phase.
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

#ifndef CC1101PinsService_h
#define CC1101PinsService_h

#include <ESP32SvelteKit.h>
#include <FSPersistence.h>
#include <HttpEndpoint.h>
#include <EventEndpoint.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <cc1101.h>

#define CC1101_PINS_FILE "/config/cc1101-pins.json"      ///< Configuration file path
#define CC1101_PINS_SERVICE_PATH "/rest/cc1101-pins"     ///< REST API endpoint for the pin config
#define CC1101_PIN_PROFILE_PATH "/rest/cc1101/pin-profile" ///< REST API endpoint for the read-only board profile
#define CC1101_VALID_PINS_PATH "/rest/cc1101/valid-pins"   ///< REST API endpoint for the chip's assignable GPIOs (expert mode)
#define CC1101_PINS_EVENT "cc1101-pins"                  ///< WebSocket event name for pin config sync

#define CC1101_PINS_KEY_CSN "csn"
#define CC1101_PINS_KEY_MISO "miso"
#define CC1101_PINS_KEY_MOSI "mosi"
#define CC1101_PINS_KEY_SCK "sck"
#define CC1101_PINS_KEY_GDO0 "gdo0"
#define CC1101_PINS_KEY_SPI_HOST "spi_host"
#define CC1101_PINS_KEY_CONFIGURED "configured"

/// CC1101 pin configuration data model
class CC1101Pins
{
public:
    int csn = -1;            ///< Chip select GPIO
    int miso = -1;           ///< SPI MISO GPIO
    int mosi = -1;           ///< SPI MOSI GPIO
    int sck = -1;            ///< SPI clock GPIO
    int gdo0 = -1;           ///< GDO0 interrupt GPIO
    int spiHost = SPI2_HOST; ///< SPI host (spi_host_device_t)
    bool configured = false; ///< true when the pin set is valid and ready to drive the radio

    /// Validate a pin set: all five pins distinct, valid for their direction, host valid.
    static bool pinsValid(const CC1101Pins &s);

    static void read(CC1101Pins &s, JsonObject &root)
    {
        root[CC1101_PINS_KEY_CSN] = s.csn;
        root[CC1101_PINS_KEY_MISO] = s.miso;
        root[CC1101_PINS_KEY_MOSI] = s.mosi;
        root[CC1101_PINS_KEY_SCK] = s.sck;
        root[CC1101_PINS_KEY_GDO0] = s.gdo0;
        root[CC1101_PINS_KEY_SPI_HOST] = s.spiHost;
        root[CC1101_PINS_KEY_CONFIGURED] = s.configured;
    }

    static StateUpdateResult update(JsonObject &root, CC1101Pins &s, const String &originId);

private:
    static constexpr const char *TAG = "CC1101Pins"; ///< Logging tag
};

/// Service for managing the runtime CC1101 pin configuration
class CC1101PinsService : public StatefulService<CC1101Pins>
{
public:
    CC1101PinsService(ESP32SvelteKit *sveltekit);

    /// Initialize the service (endpoints, persistence, profile endpoint)
    void begin();

    /// Get the currently configured pins as a driver-ready struct (thread-safe)
    cc1101_pins_t getPins()
    {
        beginTransaction();
        cc1101_pins_t p = {_state.csn, _state.miso, _state.mosi, _state.sck, _state.gdo0, _state.spiHost};
        endTransaction();
        return p;
    }

    /// Whether a valid pin set is configured (thread-safe)
    bool isConfigured()
    {
        beginTransaction();
        bool c = _state.configured;
        endTransaction();
        return c;
    }

private:
    HttpEndpoint<CC1101Pins> _httpEndpoint;   ///< REST API endpoint handler
    EventEndpoint<CC1101Pins> _eventEndpoint; ///< WebSocket event endpoint
    FSPersistence<CC1101Pins> _fsPersistence; ///< File system persistence handler
    PsychicHttpServer *_server;               ///< HTTP server instance
    SecurityManager *_securityManager;        ///< Security manager instance

    /// HTTP handler serving the read-only active board pin profile (catalog + capabilities)
    esp_err_t _handleGetProfile(PsychicRequest *request);

    /// HTTP handler serving the chip's assignable GPIO set (runtime-derived, for expert mode)
    esp_err_t _handleGetValidPins(PsychicRequest *request);
};

#endif // CC1101PinsService_h
