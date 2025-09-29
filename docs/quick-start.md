# Quick Start Guide

Get your ESP32 Genius Gateway up and running in under 30 minutes.

## Prerequisites

Before you begin, ensure you have:

- **Hardware**: ESP32-S3 board and CC1101 radio module (or complete gateway PCB)
- **Software**: Web browser for configuration
- **Network**: WiFi network for gateway connectivity
- **Optional**: MQTT broker for home automation integration

## Step 1: Hardware Setup

=== "Pre-built Gateway"
    
    If you have a complete Genius Gateway PCB:
    
    1. Connect the antenna to the CC1101 module
    2. Apply 5V power via USB-C or external supply
    3. The gateway will create a WiFi access point on first boot

=== "DIY Assembly"
    
    For custom hardware builds:
    
    1. Wire ESP32-S3 to CC1101 module according to [hardware schematic](hardware/schematics.md)
    2. Connect antenna to CC1101 ANT pin
    3. Power the ESP32 via USB or external 3.3V supply
    
    !!! tip "Wiring Quick Reference"
        | CC1101 | ESP32-S3 |
        |--------|----------|
        | VCC    | 3.3V     |
        | GND    | GND      |
        | MOSI   | GPIO11   |
        | MISO   | GPIO13   |
        | SCK    | GPIO12   |
        | CSN    | GPIO10   |
        | GDO0   | GPIO4    |
        | GDO2   | GPIO5    |

## Step 2: First Boot & WiFi Setup

1. **Connect to Gateway WiFi**
   - Network: `Genius Gateway`
   - Password: `geniusgateway` (default)

2. **Access Web Interface**
   - Open browser to `http://192.168.4.1`
   - You'll see the initial setup screen

3. **Configure WiFi Connection**
   - Navigate to **Settings** → **WiFi**
   - Scan for and select your home WiFi network
   - Enter WiFi password and save
   - Gateway will reboot and connect to your network

## Step 3: Network Configuration

1. **Find Gateway IP Address**
   - Check your router's DHCP client list
   - Or use network scanner to find device with hostname `genius-gateway`

2. **Access Web Interface**
   - Navigate to the gateway's IP address in your browser
   - You should see the main dashboard

3. **Verify Operation**
   - Check **System Status** for WiFi connectivity
   - Verify CC1101 radio module is detected and initialized

## Step 4: Device Discovery

The gateway will automatically discover nearby Hekatron Genius Plus X devices:

1. **Automatic Discovery**
   - Ensure smoke detectors are powered and operating normally
   - Gateway will detect devices when they transmit (alarms, tests, commissioning)
   - New devices appear in **Device Management** section

2. **Manual Device Addition**
   - If devices aren't auto-discovered, you can add them manually
   - Navigate to **Device Management** → **Add Device**
   - Enter smoke detector and radio module serial numbers

3. **Device Configuration**
   - Set meaningful location names for each device
   - Configure alarm preferences if needed

## Step 5: MQTT Integration (Optional)

For home automation integration:

1. **Configure MQTT Settings**
   - Navigate to **Settings** → **MQTT**
   - Enter your MQTT broker details:
     - Host: Your MQTT broker IP/hostname
     - Port: `1883` (default) or your custom port
     - Username/Password: If required by your broker

2. **Test Connection**
   - Save settings and verify connection status
   - Check MQTT broker logs for successful connection

3. **Home Assistant Auto-Discovery**
   - Enable MQTT discovery in settings
   - Devices will automatically appear in Home Assistant

## Verification Checklist

Once setup is complete, verify everything is working:

- [ ] Gateway connected to WiFi with stable connection
- [ ] CC1101 radio module detected and configured
- [ ] Web interface accessible from local network
- [ ] At least one Genius device discovered or configured
- [ ] MQTT connection established (if configured)
- [ ] Packet visualizer shows RF activity

## What's Next?

Now that your gateway is operational:

- **[Explore Features](features/index.md)** - Learn about all gateway capabilities
- **[Device Management](features/device-management.md)** - Configure and monitor your smoke detectors  
- **[Smart Home Integration](features/smart-home-integration.md)** - Set up advanced home automation
- **[Packet Visualizer](features/packet-visualizer.md)** - Monitor RF communication in real-time

## Troubleshooting

!!! question "Gateway not creating WiFi access point?"
    - Check power supply (needs stable 3.3V or 5V depending on board)
    - Verify ESP32 is flashed with correct firmware
    - Try factory reset by holding boot button during power-on

!!! question "Can't connect to home WiFi?"
    - Verify WiFi credentials are correct
    - Check if network uses 2.4GHz (ESP32 doesn't support 5GHz)
    - Try moving gateway closer to router during setup

!!! question "No devices discovered?"
    - Ensure Genius detectors are within range (typically 10-50m)
    - Check that detectors have FM Basis Radio Module installed
    - Try triggering a test alarm to generate RF traffic

For more detailed troubleshooting, see the [Troubleshooting Guide](troubleshooting.md).