/// @file main.cpp
/// @brief Main entry point for Genius Gateway application32 SvelteKit

#include <ESP32SvelteKit.h>
#include <PsychicHttpServer.h>
#include <GeniusGateway.h>
#include <nvs_flash.h>
#include <esp_http_server.h>

#define SERIAL_BAUD_RATE 115200 ///< Serial communication baud rate

PsychicHttpServer server;

ESP32SvelteKit esp32sveltekit(&server, 150);

GeniusGateway geniusGateway = GeniusGateway(&esp32sveltekit);

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
    server.config.max_open_sockets = 8;  // Default is 7
    // Increase max header length
    server.config.max_req_hdr_len = 2048; // Default is 1024
    // Increase max URI length
    server.config.max_uri_len = 1024; // Default is 512

    // start ESP32-SvelteKit
    esp32sveltekit.begin();

    // start Genius Gateway
    geniusGateway.begin();
}

/// Main loop - delete Arduino loop task as ESP32SvelteKit handles everything
void loop()
{
    // Delete Arduino loop task, as it is not needed in this example
    vTaskDelete(NULL);
}
