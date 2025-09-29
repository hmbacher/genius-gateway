# MQTT & Home Assistant

This page describes how to set up MQTT broker connection and Home Assistant integration for the ESP32 Genius Gateway.

## Overview

Integrating the ESP32 Genius Gateway with Home Assistant via MQTT enables powerful home automation scenarios, centralized monitoring, and advanced alerting capabilities. This guide covers the complete setup process from MQTT broker configuration to Home Assistant device integration.

## MQTT Broker Setup

### Choosing an MQTT Broker

*[Content to be added: MQTT broker options and recommendations]*

#### Recommended Brokers

**For Home Users:**
1. **Mosquitto** - Lightweight, open-source MQTT broker
2. **Home Assistant Supervisor Add-on** - Integrated Mosquitto broker
3. **Eclipse Mosquitto** - Full-featured MQTT broker
4. **HiveMQ Community Edition** - Enterprise-grade features

**For Advanced Users:**
- **EMQX** - Scalable, cloud-native MQTT broker
- **VerneMQ** - High-performance, distributed MQTT broker
- **Cloud Brokers** - AWS IoT Core, Azure IoT Hub, Google Cloud IoT

### Mosquitto Broker Installation

*[Content to be added: Step-by-step Mosquitto installation]*

#### Using Home Assistant Add-on

**Installation Steps:**
1. **Open Home Assistant** - Navigate to Supervisor → Add-on Store
2. **Find Mosquitto Broker** - Search for official Mosquitto broker add-on
3. **Install Add-on** - Click Install and wait for completion
4. **Configure Add-on** - Set up users and access control
5. **Start Broker** - Enable and start the Mosquitto broker service

#### Configuration Example

*[Content to be added: Mosquitto broker configuration]*

```yaml
# Mosquitto Add-on Configuration
logins:
  - username: genius-gateway
    password: your_secure_password
anonymous: false
customize:
  active: false
  folder: mosquitto
certfile: fullchain.pem
keyfile: privkey.pem
require_certificate: false
```

#### Standalone Installation

*[Content to be added: Installing Mosquitto on separate server]*

**Linux Installation:**
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install mosquitto mosquitto-clients

# Start and enable service
sudo systemctl start mosquitto
sudo systemctl enable mosquitto
```

**Configuration File:**
```conf
# /etc/mosquitto/mosquitto.conf
listener 1883 0.0.0.0
allow_anonymous false
password_file /etc/mosquitto/passwd

# Optional: SSL/TLS configuration
listener 8883 0.0.0.0
cafile /etc/ssl/certs/ca-certificates.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
```

## Gateway MQTT Configuration

### Basic MQTT Setup

*[Content to be added: Screenshot of gateway MQTT configuration interface]*

#### Connection Parameters

**Required Settings:**
1. **Broker Address** - IP address or hostname of MQTT broker
2. **Port** - Usually 1883 (plain) or 8883 (SSL/TLS)
3. **Username** - MQTT broker username
4. **Password** - MQTT broker password
5. **Client ID** - Unique identifier for this gateway

#### Advanced Settings

*[Content to be added: Advanced MQTT configuration options]*
- **Keep Alive** - Connection keep-alive interval (default: 60 seconds)
- **Clean Session** - Whether to start with clean session
- **Last Will Testament** - Message sent when gateway disconnects unexpectedly
- **QoS Levels** - Quality of Service for different message types

### SSL/TLS Configuration

*[Content to be added: Secure MQTT connection setup]*

#### Certificate Configuration

**SSL Options:**
1. **Server Certificates** - CA certificate verification
2. **Client Certificates** - Mutual TLS authentication
3. **Self-Signed Certificates** - For testing environments
4. **Let's Encrypt Integration** - Automatic certificate management

#### Security Best Practices

*[Content to be added: MQTT security recommendations]*
- Use strong passwords for MQTT authentication
- Enable SSL/TLS for production deployments
- Implement proper access control lists (ACLs)
- Regular credential rotation
- Network segmentation for IoT devices

### Topic Structure Configuration

*[Content to be added: Configuring MQTT topic hierarchy]*

#### Default Topic Structure

```
genius-gateway/
├── status                     # Gateway online/offline status
├── devices/
│   └── {device_id}/
│       ├── state             # Device operational state
│       ├── battery           # Battery level percentage
│       ├── signal            # RF signal strength (RSSI)
│       ├── test              # Test button/status
│       └── availability      # Online/offline status
├── system/
│   ├── info                  # System information
│   ├── stats                 # Communication statistics  
│   └── logs                  # System logs (if enabled)
└── homeassistant/            # Home Assistant discovery
    └── binary_sensor/
        └── genius_gateway_*/
```

#### Custom Topic Configuration

*[Content to be added: Customizing MQTT topics]*
- **Base Topic Prefix** - Change default "genius-gateway" prefix
- **Device Topic Format** - Customize device-specific topics
- **Attribute Topics** - Additional device attributes
- **Command Topics** - Control command structure

## Home Assistant Integration

### MQTT Discovery Setup

*[Content to be added: Screenshot of Home Assistant MQTT discovery configuration]*

#### Enable MQTT Discovery in Home Assistant

**Configuration.yaml Entry:**
```yaml
mqtt:
  broker: localhost          # Your MQTT broker address
  port: 1883                # MQTT broker port
  username: homeassistant   # MQTT username
  password: your_password   # MQTT password
  discovery: true           # Enable MQTT discovery
  discovery_prefix: homeassistant  # Discovery topic prefix
```

#### Gateway Discovery Configuration

*[Content to be added: Gateway-side discovery configuration]*

**Gateway MQTT Settings:**
- **Enable HA Discovery** - Turn on Home Assistant auto-discovery
- **Discovery Prefix** - Should match HA configuration (usually "homeassistant")
- **Device Info** - Gateway device information for HA
- **Entity Categories** - How devices are categorized in HA

### Device Integration

#### Automatic Device Creation

*[Content to be added: Screenshot of HA devices created by discovery]*

After enabling discovery, Home Assistant automatically creates:

**Device Entities:**
1. **Binary Sensors** - Smoke detection status for each detector
2. **Sensors** - Battery level, signal strength, last seen
3. **Device Tracker** - Online/offline status tracking
4. **Buttons** - Test buttons for manual device testing
5. **Diagnostics** - System health and performance metrics

#### Device Configuration

*[Content to be added: HA device configuration options]*

**Entity Customization:**
- **Friendly Names** - Customize entity names in HA
- **Device Classes** - Proper device classification (smoke, battery, etc.)
- **State Classes** - For historical data and energy dashboard
- **Units of Measurement** - Proper units for sensor values

### Dashboard Integration

#### Lovelace Dashboard

*[Content to be added: Screenshot of HA dashboard with Genius Gateway devices]*

#### Recommended Cards

**Dashboard Elements:**
1. **Entities Card** - Overview of all smoke detectors
2. **Picture Elements** - Floor plan with device locations
3. **History Graph** - Battery levels and signal strength over time
4. **Alert Card** - Current alerts and system status
5. **Logbook Card** - Recent device events and changes

#### Custom Dashboard Example

*[Content to be added: Example Lovelace dashboard configuration]*

```yaml
type: vertical-stack
cards:
  - type: entities
    title: Smoke Detection System
    entities:
      - entity: binary_sensor.genius_gateway_living_room
        name: Living Room
      - entity: binary_sensor.genius_gateway_bedroom
        name: Master Bedroom
      - entity: binary_sensor.genius_gateway_kitchen
        name: Kitchen
        
  - type: horizontal-stack
    cards:
      - type: gauge
        entity: sensor.genius_gateway_system_health
        name: System Health
        min: 0
        max: 100
      - type: sensor
        entity: sensor.genius_gateway_devices_online
        name: Devices Online
```

## Automation Integration

### Basic Automations

*[Content to be added: Example Home Assistant automations]*

#### Fire Alert Automation

```yaml
automation:
  - alias: "Fire Alert Response"
    trigger:
      - platform: state
        entity_id: 
          - binary_sensor.genius_gateway_living_room
          - binary_sensor.genius_gateway_bedroom
          - binary_sensor.genius_gateway_kitchen
        to: 'on'
    action:
      - service: notify.mobile_app_phone
        data:
          title: "🔥 FIRE ALERT!"
          message: "Smoke detected in {{ trigger.to_state.attributes.friendly_name }}"
          data:
            priority: high
            tag: fire_alert
      - service: light.turn_on
        target:
          area_id: all
        data:
          brightness: 255
          color_name: red
```

#### Low Battery Notification

*[Content to be added: Battery monitoring automation]*

```yaml
automation:
  - alias: "Smoke Detector Low Battery"
    trigger:
      - platform: numeric_state
        entity_id: 
          - sensor.genius_gateway_living_room_battery
          - sensor.genius_gateway_bedroom_battery
          - sensor.genius_gateway_kitchen_battery
        below: 20
    action:
      - service: notify.homeassistant
        data:
          title: "Low Battery Warning"
          message: "{{ trigger.to_state.attributes.friendly_name }} battery is at {{ trigger.to_state.state }}%"
```

### Advanced Integration Scenarios

#### Integration with Security System

*[Content to be added: Security system integration examples]*
- **Armed/Disarmed States** - Different behavior based on security system state
- **Zone-Based Actions** - Actions based on detection zones
- **Emergency Protocols** - Coordinate with security system during alerts
- **Vacation Mode** - Adjust notifications when away from home

#### Climate System Integration

*[Content to be added: HVAC integration scenarios]*
- **Ventilation Control** - Activate exhaust fans on smoke detection
- **HVAC Shutdown** - Stop air circulation during fire events
- **Zone Isolation** - Close dampers to contain smoke
- **Emergency Ventilation** - Positive pressure in safe zones

## Troubleshooting Integration Issues

### Common MQTT Problems

*[Content to be added: Common MQTT integration issues]*

#### Connection Issues

**Symptoms and Solutions:**
1. **Gateway Not Connecting** - Check broker address, credentials, firewall
2. **Intermittent Disconnections** - Verify network stability, keep-alive settings
3. **Messages Not Received** - Check topic subscriptions, QoS levels
4. **SSL/TLS Errors** - Verify certificates, clock synchronization

#### Home Assistant Discovery Issues

*[Content to be added: HA discovery troubleshooting]*
- **Devices Not Appearing** - Check discovery prefix, MQTT broker logs
- **Duplicate Entities** - Clear MQTT retained messages
- **Entity Unavailable** - Verify topic structure, gateway online status
- **Missing Attributes** - Check JSON message format, entity configuration

### Diagnostic Tools

#### MQTT Monitoring

*[Content to be added: Tools for monitoring MQTT communication]*

**Command Line Tools:**
```bash
# Subscribe to all gateway topics
mosquitto_sub -h localhost -t "genius-gateway/#" -v

# Publish test message
mosquitto_pub -h localhost -t "genius-gateway/test" -m "test message"

# Monitor Home Assistant discovery
mosquitto_sub -h localhost -t "homeassistant/+/genius_gateway_+/config" -v
```

#### Home Assistant Debugging

*[Content to be added: HA debugging techniques]*
- **MQTT Integration Logs** - Enable debug logging for MQTT
- **Entity Registry** - Check entity registry for duplicate entries
- **Developer Tools** - Use HA developer tools for testing
- **MQTT Explorer** - Third-party tools for MQTT debugging

---

*This guide provides comprehensive setup instructions for integrating your ESP32 Genius Gateway with Home Assistant via MQTT, enabling powerful home automation capabilities.*