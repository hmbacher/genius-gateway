/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2024 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <ESP32SvelteKit.h>
#include <PsychicHttpServer.h>
#include <GeniusGateway.h>

#define SERIAL_BAUD_RATE 115200

PsychicHttpServer server;

ESP32SvelteKit esp32sveltekit(&server, 150);

GeniusGateway geniusGateway = GeniusGateway(&esp32sveltekit);

void setup()
{
    // start serial and filesystem
    Serial.begin(SERIAL_BAUD_RATE);

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
