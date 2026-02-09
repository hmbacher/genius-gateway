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

## Home Assistant Auto-Discovery

The gateway supports [Home Assistant's MQTT Discovery protocol :material-open-in-new:](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery){ target=_blank } for automatic device integration.

### Genius Devices (Smoke Detectors)

The following topics are used for smoke detector integration with Home Assistant.

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

- Published when smoke detector is first [created](../features/device-management.md#adding-a-new-detector) or [imported](../features/device-management.md#importing-configuration)
- Re-published when MQTT connection is established
- Re-published when [MQTT settings change](../setup/connections.md#mqtt)
- Published only if [device publishing](../setup/connections.md#device-publishing) is enabled

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
- Re-published when [device configuration changes](../features/device-management.md#editing-a-detector) (location, etc.)
- Published only if [device publishing](../setup/connections.md#device-publishing) is enabled

**:material-home-assistant: Home Assistant Integration**

- Automatically updates binary sensor entity
- Entity shows as "Clear" (OFF) or "Smoke detected" (ON)
- Can be used in automations and scenes

---

### Alarm Lines

The following topics enable remote control of alarm line actions (line tests and fire alarm tests) via MQTT.

**Requirements:**

- [Alarm lines must be configured](../setup/configure-gateway.md#adding-alarm-lines) in Genius Gateway
- [Home Assistant Integration must be enabled](../setup/connections.md#device-publishing)
- MQTT broker must be connected

#### Overview

Each configured alarm line publishes:

- **4 Button entities** for triggering actions (line test start/stop, fire alarm start/stop)
- **1 Sensor entity** for transmission state monitoring

**Topic Structure:**
```
{discovery_prefix}genius-alarmline/{line_id}/{entity_type}/{command|state}
```

---

#### Button Entities Configuration

**:material-message-outline: Topic Pattern**
```
{discovery_prefix}button/genius-alarmline-{line_id}-{button_type}/config
```

**:material-information-outline: Description:** Configuration messages for Home Assistant button discovery

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**Button Types:**

- `linetest` - Start Line Test button
- `linetest-stop` - Stop Line Test button
- `firealarm` - Start Fire Alarm button
- `firealarm-stop` - Stop Fire Alarm button

**:material-code-json: Example Payload (Line Test Start)**
```json
{
  "~": "homeassistant/genius-alarmline/123456789",
  "name": "Start Line Test",
  "unique_id": "genius-alarmline_123456789_linetest_start",
  "command_topic": "homeassistant/genius-alarmline/123456789/linetest/command",
  "payload_press": "{\"action\":\"start\"}",
  "icon": "mdi:map-marker",
  "availability": [{
    "topic": "homeassistant/genius-alarmline/123456789/transmission/state",
    "value_template": "{% if value_json.state == 'Nothing' %}online{% else %}offline{% endif %}"
  }],
  "availability_mode": "all",
  "device": {
    "identifiers": "genius-alarmline-123456789",
    "name": "Alarm Line 'First Floor'",
    "manufacturer": "Hekatron Vertriebs GmbH",
    "model": "Genius Plus X Alarm Line"
  }
}
```

**:material-format-list-bulleted: Key Fields**

- `~` - Topic prefix (base path for relative references)
- `name` - Button name displayed in Home Assistant
- `unique_id` - Unique identifier for this button
- `command_topic` - Topic where button press commands are published
- `payload_press` - JSON payload sent when button is pressed
- `icon` - Material Design icon identifier
- `availability` - Button is only available when no transmission is active
- `device` - Groups all alarm line entities under one device

**:material-publish: Publishing Behavior**

- Published when alarm line is first created
- Re-published when MQTT connection is established
- Re-published when MQTT settings change
- Published only if [Home Assistant Integration is enabled](../setup/connections.md#device-publishing)

**:material-home-assistant: Home Assistant Integration**

- Automatically creates button entities
- Buttons are grouped under alarm line device
- Buttons become unavailable during active transmissions
- Can be used in automations and dashboards

---

#### Command Topics

**:material-message-outline: Topic Patterns**
```
{discovery_prefix}genius-alarmline/{line_id}/linetest/command
{discovery_prefix}genius-alarmline/{line_id}/firealarm/command
```

**:material-information-outline: Description:** Accepts commands to trigger line test or fire alarm actions on the specified alarm line

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** false

**:material-code-json: Payload Format**

Start action:
```json
{"action": "start"}
```

Stop action:
```json
{"action": "stop"}
```

**:material-format-list-bulleted: Payload Fields**

- `action` - Command to execute
    - `"start"` - Begin line test or fire alarm transmission
    - `"stop"` - End line test or fire alarm transmission

**:material-publish: Command Behavior**

- Commands are subscribed with wildcard: `{prefix}genius-alarmline/+/linetest/command`
- Gateway extracts alarm line ID from topic path
- Triggers RF transmission immediately if no other transmission is active
- Rejects commands if previous transmission is still in progress (503 internally)
- Updates transmission state sensor to reflect activity

!!! warning "Transmission Blocking"
    Only one transmission can be active at a time. Commands received during an active transmission are ignored until the current transmission completes (typically 3-10 seconds).

---

#### Transmission State Sensor

**:material-message-outline: Topic**
```
{discovery_prefix}genius-alarmline/{line_id}/transmission/state
```

**Example:** `homeassistant/genius-alarmline/123456789/transmission/state`

**:material-information-outline: Description:** Current transmission status of the alarm line

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Payload Examples**

Idle state (no active transmission):
```json
{"state": "Nothing"}
```

During line test start transmission:
```json
{"state": "Line Test Start"}
```

During fire alarm stop transmission:
```json
{"state": "Fire Alarm Stop"}
```

**:material-format-list-bulleted: State Values**

- `"Nothing"` - No active transmission, all buttons available
- `"Line Test Start"` - Line test start transmission in progress
- `"Line Test Stop"` - Line test stop transmission in progress
- `"Fire Alarm Start"` - Fire alarm start transmission in progress
- `"Fire Alarm Stop"` - Fire alarm stop transmission in progress

**:material-publish: Publishing Behavior**

- Published when transmission starts (state shows action type)
- Published when transmission completes (state resets to `"Nothing"`)
- Published when transmission times out (state resets to `"Nothing"`)
- Re-published when MQTT connection is established
- Controls button availability (buttons disabled when state ≠ `"Nothing"`)

**:material-home-assistant: Home Assistant Integration**

- Automatically creates sensor entity
- Used for button availability control via template
- Displays current transmission activity
- Can trigger automations based on transmission state changes
- Useful for monitoring scheduled automated line tests

---

## Global Alarm State Topic

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
- Published only if [simple alarm publishing](../setup/connections.md#simple-alarm-publishing) is enabled

**:material-home-automation: Integration**

This topic enables integration with *all* smart home systems that support MQTT.