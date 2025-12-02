# MQTT Topics Documentation

## Introduction

This document describes the MQTT topics published by the Genius Gateway. The gateway supports both [Home Assistant auto-discovery :material-open-in-new:](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery){ target=_blank } and custom MQTT topics for integration with other home automation systems connecting to a MQTT broker.

### MQTT Configuration

MQTT functionality is mainly configured via [web frontend](../features/index.md) or HTTP API endpoints:

- **Framework MQTT Settings:** `/rest/mqttSettings` - Basic MQTT connection settings
- **Gateway MQTT Settings:** `/rest/mqtt-settings` - Genius Gateway specific publishing options

---

## Framework MQTT Topics (ESP32 SvelteKit)

### Device Status Topic (LWT)

**:material-message-outline: Topic**
```
genius-gateway/{device-mac}/status
```
!!! tip "`device-mac` format"
    `device-mac` is the device's MAC address with lowercase hex digits and without any delimiters.  
    E.g.: `1A:2B:3C:4D:5E:6F` :material-arrow-right: `1a2b3c4d5e6f`

**:material-information-outline: Description:** Framework device online/offline status using MQTT Last Will and Testament (LWT)

**:material-speedometer: QoS:** 1

**:material-content-save-outline: Retain:** true

**:material-code-json: Payloads**

Published when device connects to MQTT broker:
```json
"online"
```

Published automatically by broker when device disconnects (LWT message):
```json
"offline"
```
---

## Genius Gateway MQTT Topics

### Home Assistant Auto-Discovery

The gateway supports [Home Assistant's MQTT Discovery protocol :material-open-in-new:](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery){ target=_blank } for automatic device integration.

#### Configuration Topic

**:material-message-outline: Topic (Default)**
```
homeassistant/binary_sensor/genius-{smoke_detector_sn}/config
```

**:material-information-outline: Description:** Device configuration for Home Assistant auto-discovery

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Payload**
```json
{
  "~": "homeassistant/binary_sensor/genius-12345678",
  "name": "Genius Plus X",
  "unique_id": "12345678",
  "device_class": "smoke",
  "state_topic": "~/state",
  "schema": "json",
  "value_template": "{{value_json.state}}",
  "entity_picture": "http://192.168.1.100/hekatron-genius-plus-x.png",
  "device": {
    "identifiers": "12345678",
    "manufacturer": "Hekatron Vertriebs GmbH",
    "model": "Genius Plus X",
    "name": "Rauchmelder",
    "serial_number": "12345678",
    "suggested_area": "Living Room"
  }
}
```

**:material-format-list-bulleted: Payload Fields**

- `~` - Topic prefix (base path for relative references)
- `name` - Entity name shown in Home Assistant
- `unique_id` - Unique identifier (smoke detector serial number)
- `device_class` - Device class (`smoke` for smoke detectors)
- `state_topic` - Relative path to state topic (expands to `{prefix}{sn}/state`)
- `schema` - Payload format (`json`)
- `value_template` - Jinja2 template to extract state from JSON
- `entity_picture` - URL to device icon (only included if gateway has valid IP)
- `device` - Device information object
  - `identifiers` - Device identifier for grouping entities
  - `manufacturer` - Device manufacturer
  - `model` - Device model name
  - `name` - Device name
  - `serial_number` - Serial number
  - `suggested_area` - Suggested Home Assistant area/room

**:material-publish: Publishing Behavior**

- Published when smoke detector is first [created](../features/device-management.md#create) or [imported](../features/device-management.md#import)
- Re-published when MQTT connection is established
- Re-published when [MQTT settings change](../features/settings.md#mqtt-configuration)
- Published only if [device publishing](../features/settings.md#mqtt-configuration) is enabled

**:material-home-assistant: Home Assistant Integration**

- Automatically creates binary sensor entity
- Adds icon and plenty of metadata (manufacturer, model, serial number, etc.)
- Can be used in automations and scenes

---

#### State Topic

**:material-message-outline: Topic (Default)**
```
homeassistant/binary_sensor/genius-{smoke_detector_sn}/state
```

**:material-information-outline: Description:** Current alarm state of individual smoke detector

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Payload**

Published when device is *not* alarming:
```json
{
  "state": "OFF"
}
```
Published when device *is* alarming:
```json
{
  "state": "ON"
}
```

**:material-format-list-bulleted: Payload Fields**

- `state` - Alarm state
    - `"OFF"` - Smoke detector not alarming
    - `"ON"` - Smoke detector actively alarming

**:material-publish: Publishing Behavior**

- Published when device alarm state changes
- Re-published when MQTT connection is established
- Re-published when [device configuration changes](../features/device-management.md#change) (location, etc.)
- Published only if [device publishing](../features/settings.md#mqtt-configuration) is enabled

**:material-home-assistant: Home Assistant Integration**

- Automatically updates binary sensor entity
- Entity shows as "Clear" (OFF) or "Smoke detected" (ON)
- Can be used in automations and scenes

---

### Global Alarm State Topic

**:material-message-outline: Topic (Default)**
```
smarthome/genius-gateway/alarm
```

**:material-information-outline: Description:** Global alarm state aggregated from all smoke detectors

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Payload:**

Published if all devices are in standby:
```json
{
  "isAlarming": false,
  "numAlarmingDevices": 0
}
```

Published when two smoke detectors are alarming (example):
```json
{
  "isAlarming": true,
  "numAlarmingDevices": 2
}
```

**:material-format-list-bulleted: Payload Fields**

- `isAlarming` - Global alarm state (boolean)
    - `true` - At least one smoke detector is alarming
    - `false` - No smoke detectors alarming
- `numAlarmingDevices` - Number of smoke detectors currently in alarm state (integer)

**:material-publish: Publishing Behavior**

- Published whenever any device alarm state changes
- Re-published when MQTT connection is established
- Published only if [simple alarm publishing](../features/settings.md#mqtt-configuration) is enabled

**:material-home-automation: Integration**

This topic enables integration with *all* smart home systems that support MQTT.