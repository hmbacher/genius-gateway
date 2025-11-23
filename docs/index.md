# Genius Gateway

!!! danger "IMPORTANT DISCLAIMER"
    This is a reverse-engineered implementation **NOT AFFILIATED** with Hekatron. 

    **You are solely responsible for ensuring compliance with all local regulations regarding RF communication and fire safety systems. The authors assume no liability for any damages or safety issues.**
    
    **USE AT YOUR OWN RISK!** 

## What is the Genius Gateway?

![Genius Gateway Rendering](./assets/images/hardware/gateway/gg-3d-v1.1.png)

The **Genius Gateway** is a reverse-engineered interface for [:octicons-arrow-right-24: Hekatron Genius Plus X](background.md) smoke detection systems. It enables real-time monitoring, MQTT integration, and web-based management of Genius smoke detectors without requiring proprietary Hekatron gateways.

<div class="grid cards" markdown>

-   :material-smoke-detector:{ .lg .middle } __Hekatron Integration__

    ---

    Reverse-engineered gateway for Hekatron Genius Plus X smoke detection systems with real-time monitoring and control

    [:octicons-arrow-right-24: Getting started](quick-start.md)

-   :material-wifi:{ .lg .middle } __MQTT & Home Assistant__

    ---

    Seamless integration with Home Assistant and other MQTT-based home automation systems

    [:octicons-arrow-right-24: MQTT Setup](setup/mqtt-homeassistant.md)

-   :material-web:{ .lg .middle } __Web Interface__

    ---

    Modern responsive web interface for device management, packet visualization, and system configuration

    [:octicons-arrow-right-24: Features overview](features/index.md)

-   :material-chip:{ .lg .middle } __Hardware Design__

    ---

    Complete open-source hardware design with schematics, PCB layout, and assembly documentation

    [:octicons-arrow-right-24: Hardware docs](hardware/index.md)

</div>

## Key Features

- :material-smoke-detector: **Manage smoke detectors** - Real-time monitoring and control
- :material-view-list: **Manage alarm lines** - Configure and monitor alarm communications  
- :material-chart-line: **Capture, visualize and analyze packets** - Real-time packet inspection tools
- :material-cog: **Manage gateway settings** - Complete configuration interface
- :material-message-processing: **MQTT and WebSocket support** - Modern API interfaces for integration
- :simple-homeassistant: **Home Assistant integration** - Native support with automatic discovery

## Getting started

Get your Genius Gateway up and running in minutes:

1. **[Hardware Setup](setup/hardware.md)** - Set up test devices or specific gateway hardware
2. **[Firmware Installation](setup/firmware.md)** - Prepare, build, and install firmware
3. **[Configuration](setup/configuration.md)** - Configure the gateway
4. **[MQTT Integration](setup/mqtt-homeassistant.md)** - Integrate gateway into home automation systems

## Source Code

[View on GitHub :fontawesome-brands-github:{ .github }](https://github.com/hmbacher/genius-gateway){ .md-button .md-button--primary }
