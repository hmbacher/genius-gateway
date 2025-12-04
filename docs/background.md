# Background

## Motivation

Having multiple networked Hekatron Genius Plus X smoke detectors in a modern residential installation presents a unique challenge: while these devices communicate effectively with each other, they cannot be integrated into modern home automation systems without Hekatron's proprietary gateway [Genius Port :material-open-in-new:](https://www.hekatron-brandschutz.de/produkte/rauchmelder/produkte/genius-port){ target=_blank }.

Even with the Genius Port, control is only possible through the [Genius Control App :material-open-in-new:](https://play.google.com/store/apps/details?id=com.hekatron.geniuscontrol&pcampaignid=web_share){ target=_blank }, and smart home system integration seems limited exclusively to the [digitalSTROM :material-open-in-new:](https://www.digitalstrom.com/){ target=_blank } platform.

The core problem is the lack of integration options with state-of-the-art protocols like MQTT, REST APIs, or WebSocket connections that modern extensible smart home ecosystems rely on. This leaves users with an isolated fire safety network that cannot participate in comprehensive home monitoring and automation scenarios.

## Analyzed smoke detectors

This project focuses on [Hekatron Genius Plus X :material-open-in-new:](https://www.hekatron-brandschutz.de/produkte/rauchmelder/produkte/genius-plus-x){ target=_blank} smoke detectors equipped with [FM Basis X  :material-open-in-new:](https://www.hekatron-brandschutz.de/produkte/rauchmelder/produkte/funkmodul-basis-x) radio modules. These components formed the foundation of the reverse-engineered communication protocol.

!!! note "Compatibility with FM Pro X"
    The Hekatron radio module [FM Pro X :material-open-in-new:](https://www.hekatron-brandschutz.de/produkte/rauchmelder/produkte/funkmodul-pro-x) has not been tested, and its specific behaviour and features remain unexplored.
    The project currently targets the most common *FM Basis X* configuration.

## Hardware Platform

The following hardware was used for analyzing the smoke detector system and implementing the Genius Gateway.

### RF Module

[EBYTE E07-900MBL-01 :material-open-in-new:](https://ebyteiot.com/de/products/test-board-e07-900mbl-01-e07-900m10s-development-evaluation-kit-usb-interface-to-ttl-easy-use-main-control-mcu-stm8l151g4)

- Sub-GHz transceiver module
- 868 MHz operation compatible with Hekatron devices
- Integrated antenna and RF matching
- SPI interface for microcontroller communication

### Microcontroller

[ESP32-S3-DevKitC-1-N8R2 :material-open-in-new:](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)

- Dual-core Xtensa LX7 processor
- Built-in WiFi for network connectivity
- Sufficient memory and processing power (PSRAM not required)

## Software Architecture

The later Genius Gateway software builds upon the [ESP32SvelteKit :material-open-in-new:](https://theelims.github.io/ESP32-sveltekit/) framework, which provides a robust foundation for IoT applications.

- Beautiful UI powered by [DaisyUI :material-open-in-new:](https://daisyui.com/) and [TailwindCSS :material-open-in-new:](https://tailwindcss.com/)
- Low Memory Footprint and Easy Customization by Courtesy of [SvelteKit :material-open-in-new:](https://svelte.dev/docs/kit/introduction)
- Rich Communication Interfaces (like MQTT client, HTTP RESTful API, a WebSocket based Event Socket and a classic WebSocket Server)
- WiFi Provisioning and Management
- Secured API and User Management
- OTA Upgrade Service
- Automated Build Chain
- Compatible with all ESP32 Flavours
