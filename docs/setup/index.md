# Setting up Genius Gateway

This guide walks you through the complete setup process for Genius Gateway, from initial hardware assembly and software installation to network configuration and integration with your smart home system.

## Setup Pages

### [General Setup](general-setup.md)

The foundation of your Genius Gateway deployment. This page covers:

- **Hardware Assembly**: Experimental breadboard setup or final PCB assembly
- **Development Environment**: Installing VSCode with Pioarduino IDE
- **Software Build**: Cloning repository, configuring target board, building firmware
- **Initial Flash**: Uploading firmware and accessing the web interface for the first time
- **Basic connectivity**: Connecting to WiFi network and to MQTT broker, setting up user authorization

### [Configure Gateway](configure-gateway.md)

Smoke detector and alarm line configuration for full functionality:

- **Integration Levels**: Basic alarm detection (no configuration), advanced device integration (Home Assistant), alarm line functions (line tests, fire alarm tests)
- **Adding Smoke Detectors**: Import from backup, manual entry with serial numbers, automatic discovery
- **Alarm Line Configuration**: Import from backup, manual registration, automatic discovery via line test

### :tabler-topology-star-3: [Connections](connections.md)

External service integration for smart home and time synchronization:

- **MQTT Configuration**: Connect to MQTT broker (encrypted/unencrypted), configure authentication and connection parameters, set up simple alarm publishing and Home Assistant MQTT Discovery
- **NTP Setup**: Configure time server, select timezone with automatic DST handling, ensure accurate timestamps for logging

### :tabler-wifi: [WiFi](wifi.md)

Network connectivity configuration for both normal operation and fallback scenarios:

- **WiFi Station Mode**: Connect as a client to your existing WiFi network, configure hostname (mDNS), manage multiple networks with priority or signal strength selection
- **Access Point Mode**: Provide fallback WiFi network for initial setup or when station connection fails, configure SSID, password, channel, and IP addressing

### :tabler-users: [Users](users.md)

Security and access control configuration:

- **User Accounts**: Create, edit, and delete users with different privilege levels (admin/guest)
- **JWT Security**: Configure JWT secret for authentication token generation
- **Authentication Flow**: Understand login process, session management, and token-based authentication

### :tabler-adjustments: [System](system.md)

Ongoing monitoring, maintenance, and firmware management:

- **System Status**: Real-time monitoring of hardware, memory, network, and radio module status
- **Diagnostics**: System metrics, check radio module connectivity, download core dump
- **Firmware Updates**: OTA updates, upload new firmware
- **Device Actions**: Restart, factory reset
