/// @file main.cpp
/// @brief Main entry point for Genius Gateway application32 SvelteKit
/// 
/// @copyright Copyright (c) 2024-2025 Genius Gateway Project
/// @license AGPL-3.0 with Commons Clause
/// 
/// This file is part of Genius Gateway.
/// 
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Affero General Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version, with the Commons Clause restriction.
/// 
/// "Commons Clause" License Condition v1.0
/// The Software is provided to you by the Licensor under the License,
/// as defined below, subject to the following condition:
/// Without limiting other conditions in the License, the grant of rights
/// under the License will not include, and the License does not grant to you,
/// the right to Sell the Software.
/// 
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
/// GNU Affero General Public License for more details.
/// 
/// See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.

#include <ESP32SvelteKit.h>
#include <PsychicHttpServer.h>
#include <GeniusGateway.h>
#include <migrations/MigrationService.h>
#include <migrations/GatewayMigrations.h>
#include <migrations/MigrationApi.h>
#include <nvs_flash.h>
#include <esp_http_server.h>

#define SERIAL_BAUD_RATE 115200 ///< Serial communication baud rate

PsychicHttpServer server;

ESP32SvelteKit esp32sveltekit(&server, 200);

GeniusGateway geniusGateway = GeniusGateway(&esp32sveltekit);

MigrationService migrations(esp32sveltekit.getFS());

MigrationApi migrationApi(&server, esp32sveltekit.getSecurityManager(), &migrations);

constexpr const char *TAG = "main"; ///< Log tag for main application

/// Initialize NVS flash storage with error handling
void init_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        ESP_LOGE(TAG, "NVS partition needs to be erased, re-initializing NVS flash storage.");
        err = nvs_flash_erase();
        if (err != ESP_OK)
        {
            // NVS partition does not exist, no need to erase
            ESP_LOGE(TAG, "NVS erasing failed: %s", esp_err_to_name(err));
        }
        else
        {
            err = nvs_flash_init();
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "NVS re-initialization failed: %s", esp_err_to_name(err));
            }
            else
            {
                ESP_LOGI(TAG, "NVS flash storage re-initialized successfully.");
            }
        }
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash storage initialization failed: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "NVS flash storage initialized successfully.");
    }
}

/// Setup ESP32 application - initialize serial, NVS, and services
void setup()
{
    // start serial and filesystem
    Serial.begin(SERIAL_BAUD_RATE);

    // Initialize NVS flash storage
    init_nvs();

    // Configure HTTP server for larger headers (needed when using Cloudflare)
    server.config.recv_wait_timeout = 60;
    server.config.send_wait_timeout = 60;
    // Increase stack size to handle larger headers
    server.config.stack_size = 8192;  // Default is 4096
    // Allow more simultaneous connections  
    server.config.max_open_sockets = 13;  // Default is 7, increased for SPA parallel loading
    // Enable LRU purge to automatically close idle connections when needed
    server.config.lru_purge_enable = true;  // Critical for handling multiple parallel requests
    // Increase max header length
    server.config.max_req_hdr_len = 2048; // Default is 1024
    // Increase max URI length
    server.config.max_uri_len = 1024; // Default is 512
    // Enable SO_LINGER so that close() on each session socket blocks
    // until the TCP send buffer is flushed (or the linger timeout expires).
    // This ensures HTTP responses and WebSocket frames are fully delivered
    // before sockets are torn down during httpd_stop() (called by RestartService).
    server.config.enable_so_linger = true;
    server.config.linger_timeout = 2;  // seconds

    // Wire migrations: pre-phase runs from within esp32sveltekit.begin() right
    // after the FS is mounted but before any settings service reads its file;
    // post-phase runs after both apps are up to drop legacy artefacts that
    // depend on successor files being present.
    registerGatewayMigrations(migrations);
    esp32sveltekit.setPreServiceHook([]
                                     { migrations.runPhase(MigrationPhase::PreServiceBegin); });

    // start ESP32-SvelteKit
    esp32sveltekit.begin();

    // start Genius Gateway
    geniusGateway.begin();

    migrationApi.begin();
    migrations.runPhase(MigrationPhase::PostServiceBegin);
}

/// Main loop - delete Arduino loop task as ESP32SvelteKit handles everything
void loop()
{
    // Delete Arduino loop task, as it is not needed in this example
    vTaskDelete(NULL);
}
