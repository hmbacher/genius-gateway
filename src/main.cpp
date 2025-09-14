/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <ESP32SvelteKit.h>
#include <PsychicHttpServer.h>
#include <GeniusGateway.h>
#include <nvs_flash.h>

#define SERIAL_BAUD_RATE 115200

PsychicHttpServer server;

ESP32SvelteKit esp32sveltekit(&server, 150);

GeniusGateway geniusGateway = GeniusGateway(&esp32sveltekit);

constexpr const char *TAG = "main";

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

void setup()
{
    // start serial and filesystem
    Serial.begin(SERIAL_BAUD_RATE);

    // Initialize NVS flash storage
    init_nvs();

    // start ESP32-SvelteKit
    esp32sveltekit.begin();

    // start Genius Gateway
    geniusGateway.begin();
}

void loop()
{
    // Delete Arduino loop task, as it is not needed in this example
    vTaskDelete(NULL);
}
