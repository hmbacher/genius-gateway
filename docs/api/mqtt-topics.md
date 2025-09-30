# MQTT Topics

This page documents the MQTT topic structure and message formats for integration with Home Assistant and other automation platforms.

## Overview

The Genius Gateway publishes comprehensive data to MQTT topics for seamless integration with home automation systems. The topic structure follows best practices for organization and Home Assistant auto-discovery.

## Topic Structure

### Base Topic Prefix

All MQTT topics are prefixed with a configurable base topic:

```
{base_topic}/{device_type}/{gateway_id}/
```

**Default Configuration:**
- `base_topic`: `genius-gateway`
- `device_type`: `esp32-gateway`
- `gateway_id`: `{MAC address or configured ID}`

**Example Base:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/
```

## Home Assistant Auto-Discovery

### Discovery Topic Structure

Home Assistant auto-discovery messages are published to:

```
homeassistant/{component_type}/{gateway_id}_{entity_id}/config
```

### Gateway Discovery

**Topic:**
```
homeassistant/sensor/246F28ABCDEF_gateway_status/config
```

**Payload:**
```json
{
  "name": "Genius Gateway Status",
  "unique_id": "genius_gateway_246F28ABCDEF_status",
  "state_topic": "genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/status",
  "value_template": "{{ value_json.status }}",
  "json_attributes_topic": "genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/attributes",
  "device": {
    "identifiers": ["genius_gateway_246F28ABCDEF"],
    "name": "Genius Gateway",
    "model": "ESP32-S3",
    "manufacturer": "Custom",
    "sw_version": "1.2.3",
    "configuration_url": "http://192.168.1.100"
  },
  "availability": {
    "topic": "genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/availability"
  }
}
```

### Smoke Detector Discovery

**Topic:**
```
homeassistant/binary_sensor/246F28ABCDEF_detector_12345678/config
```

**Payload:**
```json
{
  "name": "Living Room Smoke Detector",
  "unique_id": "genius_gateway_detector_12345678",
  "state_topic": "genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/12345678/alarm",
  "device_class": "smoke",
  "value_template": "{{ 'ON' if value_json.alarm else 'OFF' }}",
  "json_attributes_topic": "genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/12345678/attributes",
  "device": {
    "identifiers": ["genius_detector_12345678"],
    "name": "Living Room Smoke Detector",
    "model": "Hekatron Genius Hx",
    "manufacturer": "Hekatron",
    "via_device": "genius_gateway_246F28ABCDEF"
  },
  "availability": {
    "topic": "genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/12345678/availability"
  }
}
```

## Gateway Topics

### Status and Availability

**Gateway Status:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/status
```

**Payload:**
```json
{
  "status": "online",
  "timestamp": "2024-01-15T10:30:00Z",
  "uptime": 3600,
  "free_heap": 180000,
  "wifi_rssi": -45,
  "devices_total": 15,
  "devices_online": 12
}
```

**Gateway Availability:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/availability
```

**Payload (Last Will):**
```
offline
```

**Payload (Online):**
```
online
```

### Gateway Attributes

**Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/attributes
```

**Payload:**
```json
{
  "firmware_version": "1.2.3",
  "build_date": "2024-01-15T10:00:00Z",
  "mac_address": "24:6F:28:AB:CD:EF",
  "ip_address": "192.168.1.100",
  "rf_frequency": 868420000,
  "rf_power": 10,
  "mqtt_client_id": "esp32_gateway_246F28ABCDEF",
  "configuration": {
    "auto_discovery": true,
    "retain_messages": true,
    "qos_level": 1
  }
}
```

### System Information

**Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/system/info
```

**Payload:**
```json
{
  "cpu_usage": 25.5,
  "memory_usage": 65.2,
  "flash_usage": 45.8,
  "rf_quality": 85,
  "last_restart": "2024-01-15T08:00:00Z",
  "restart_reason": "power_on",
  "diagnostics": {
    "cc1101_status": "ok",
    "wifi_status": "connected",
    "mqtt_status": "connected"
  }
}
```

### Gateway Commands

**Command Topic (Subscribed):**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/command
```

**Command Payload Examples:**
```json
{
  "command": "restart",
  "delay": 5000
}
```

```json
{
  "command": "scan_devices",
  "timeout": 30
}
```

```json
{
  "command": "update_config",
  "config": {
    "mqtt_publish_interval": 60
  }
}
```

## Device Topics

### Device Status

**Topic Pattern:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/{device_id}/status
```

**Example Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/12345678/status
```

**Payload:**
```json
{
  "device_id": "12345678",
  "status": "normal",
  "online": true,
  "last_seen": "2024-01-15T10:29:00Z",
  "signal_strength": -45,
  "battery_level": 85,
  "firmware_version": "2.1.0"
}
```

### Device Availability

**Topic Pattern:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/{device_id}/availability
```

**Payload:**
- `online` - Device is responding to communications
- `offline` - Device hasn't responded within timeout period

### Device Attributes

**Topic Pattern:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/{device_id}/attributes
```

**Payload:**
```json
{
  "name": "Living Room Detector",
  "location": "Living Room",
  "device_type": "smoke_detector",
  "model": "Hekatron Genius Hx",
  "installation_date": "2024-01-01T00:00:00Z",
  "last_test": "2024-01-14T09:00:00Z",
  "test_result": "pass",
  "maintenance_due": "2024-04-01T00:00:00Z",
  "custom_attributes": {
    "room_type": "living_area",
    "ceiling_height": 2.5
  }
}
```

### Alarm Status

**Topic Pattern:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/{device_id}/alarm
```

**Payload:**
```json
{
  "alarm": true,
  "alarm_type": "smoke_detected",
  "timestamp": "2024-01-15T10:35:00Z",
  "sensor_value": 850,
  "threshold": 500,
  "severity": "high"
}
```

### Battery Status

**Topic Pattern:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/{device_id}/battery
```

**Payload:**
```json
{
  "battery_level": 85,
  "battery_voltage": 3.2,
  "low_battery": false,
  "battery_type": "lithium",
  "estimated_life": 365
}
```

### Device Commands

**Command Topic Pattern (Subscribed):**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/{device_id}/command
```

**Test Command:**
```json
{
  "command": "test",
  "test_type": "full",
  "timeout": 30
}
```

**Configuration Command:**
```json
{
  "command": "configure",
  "parameters": {
    "sensitivity": "medium",
    "test_interval": 604800
  }
}
```

### Test Results

**Topic Pattern:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/{device_id}/test_result
```

**Payload:**
```json
{
  "test_id": "test_001",
  "timestamp": "2024-01-15T11:00:00Z",
  "test_type": "full",
  "result": "pass",
  "duration": 25,
  "details": {
    "smoke_chamber": "pass",
    "optical_sensor": "pass",
    "acoustic_alarm": "pass",
    "battery_check": "pass",
    "rf_communication": "pass"
  }
}
```

## Alert and Event Topics

### Alert Notifications

**Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/alerts/active
```

**Payload:**
```json
{
  "alert_id": "alert_001",
  "device_id": "12345678",
  "alert_type": "smoke_detected",
  "severity": "high",
  "timestamp": "2024-01-15T10:35:00Z",
  "location": "Living Room",
  "acknowledged": false,
  "details": {
    "sensor_reading": 850,
    "trigger_threshold": 500,
    "duration": 15
  }
}
```

### Event Log

**Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/events
```

**Payload:**
```json
{
  "event_id": "evt_001",
  "timestamp": "2024-01-15T10:40:00Z",
  "event_type": "device_discovered",
  "source": "rf_controller",
  "data": {
    "device_id": "87654321",
    "device_type": "smoke_detector",
    "signal_strength": -52
  }
}
```

## RF Communication Topics

### RF Activity

**Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/rf/activity
```

**Payload:**
```json
{
  "timestamp": "2024-01-15T10:45:00Z",
  "frequency": 868420000,
  "direction": "received",
  "device_id": "12345678",
  "packet_type": "status_update",
  "signal_strength": -48,
  "packet_size": 32,
  "crc_valid": true
}
```

### RF Statistics

**Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/rf/statistics
```

**Payload:**
```json
{
  "timestamp": "2024-01-15T10:50:00Z",
  "total_packets": 1250,
  "successful_packets": 1235,
  "failed_packets": 15,
  "success_rate": 98.8,
  "average_rssi": -47,
  "frequency": 868420000,
  "power_level": 10
}
```

## Configuration Topics

### Dynamic Configuration Updates

**Topic (Subscribed):**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/config/set
```

**MQTT Configuration Update:**
```json
{
  "section": "mqtt",
  "config": {
    "publish_interval": 30,
    "retain_messages": true,
    "qos_level": 1
  }
}
```

**RF Configuration Update:**
```json
{
  "section": "rf",
  "config": {
    "frequency": 868420000,
    "power_level": 10,
    "scan_interval": 5000
  }
}
```

### Configuration Status

**Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/config/status
```

**Payload:**
```json
{
  "timestamp": "2024-01-15T10:55:00Z",
  "config_version": "1.2.3",
  "last_update": "2024-01-15T10:30:00Z",
  "sections": {
    "mqtt": "applied",
    "rf": "applied",
    "wifi": "applied",
    "logging": "applied"
  }
}
```

## Diagnostic Topics

### System Diagnostics

**Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/diagnostics/system
```

**Payload:**
```json
{
  "timestamp": "2024-01-15T11:00:00Z",
  "system_health": "good",
  "components": {
    "wifi": {
      "status": "connected",
      "rssi": -45,
      "ip": "192.168.1.100"
    },
    "mqtt": {
      "status": "connected",
      "broker": "192.168.1.50",
      "last_message": "2024-01-15T10:59:55Z"
    },
    "rf": {
      "status": "active",
      "frequency": 868420000,
      "devices_responding": 12
    }
  }
}
```

### Error Reports

**Topic:**
```
genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/diagnostics/errors
```

**Payload:**
```json
{
  "timestamp": "2024-01-15T11:05:00Z",
  "error_type": "communication_timeout",
  "component": "rf_controller",
  "device_id": "12345678",
  "error_code": "COMM_TIMEOUT",
  "message": "Device failed to respond within timeout period",
  "retry_count": 3,
  "resolved": false
}
```

## MQTT Configuration

### QoS Levels

**Topic QoS Settings:**
- **Status Topics**: QoS 1 (At least once delivery)
- **Alert Topics**: QoS 2 (Exactly once delivery)
- **Diagnostic Topics**: QoS 0 (Best effort)
- **Command Topics**: QoS 1 (At least once delivery)

### Retained Messages

**Retained Topics:**
- Device availability topics
- Gateway availability topic
- Last known status messages
- Configuration status

### Message Throttling

*[Content to be added: Message rate limiting and batching]*

**Rate Limiting:**
- **Status Updates**: Maximum 1 message per minute per device
- **Battery Updates**: Only on significant changes (>5%)
- **RF Activity**: Configurable throttling for high-traffic scenarios

## Integration Examples

### Home Assistant Configuration

**configuration.yaml:**
```yaml
mqtt:
  broker: 192.168.1.50
  discovery: true
  discovery_prefix: homeassistant

# Binary sensors for smoke detectors
binary_sensor:
  - platform: mqtt
    name: "Living Room Smoke"
    state_topic: "genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/12345678/alarm"
    value_template: "{{ 'ON' if value_json.alarm else 'OFF' }}"
    device_class: smoke

# Sensors for battery levels
sensor:
  - platform: mqtt
    name: "Living Room Detector Battery"
    state_topic: "genius-gateway/esp32-gateway/24:6F:28:AB:CD:EF/devices/12345678/battery"
    value_template: "{{ value_json.battery_level }}"
    unit_of_measurement: "%"
    device_class: battery
```

### Node-RED Integration

*[Content to be added: Node-RED flow examples]*

### OpenHAB Integration

*[Content to be added: OpenHAB item definitions]*

---

*The MQTT topic structure provides comprehensive integration capabilities for the Genius Gateway with home automation platforms.*