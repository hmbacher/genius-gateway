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

### Gateway Device

The Genius Gateway itself is published as a Home Assistant device with diagnostic sensors, remote control buttons, and configuration switches.

**Requirements:**

- [Home Assistant Integration must be enabled](../setup/connections.md#device-publishing)
- MQTT broker must be connected

#### Overview

The gateway device publishes:

- **1 Status Sensor** - Registers the gateway device in Home Assistant
- **2 Diagnostic Sensors** - Free heap memory (%) and core temperature (°C)
- **1 Restart Button** - Remote gateway restart capability
- **4 Configuration Switches** - Remote control of gateway settings

**Topic Structure:**
```
{discovery_prefix}genius-gateway/{gateway_device_id}
```

!!! tip "Gateway Device ID Format"
    The `gateway_device_id` follows the format `genius-gateway-{device-mac}`, where `device-mac` is the device's MAC address with lowercase hex digits and without any delimiters.  
    E.g.: `1A:2B:3C:4D:5E:6F` :material-arrow-right: `genius-gateway-1a2b3c4d5e6f`

---

#### Device Status Sensor

**:material-message-outline: Topic**
```
{discovery_prefix}sensor/{gateway_device_id}/status/config
```

**Example:** `homeassistant/sensor/genius-gateway-1a2b3c4d5e6f/status/config`

**:material-information-outline: Description:** Device registration sensor that makes the gateway visible in Home Assistant

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Payload**
```json
{
  "~": "homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f",
  "name": "Status",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_status",
  "state_topic": "~/status/state",
  "value_template": "{{value_json.state}}",
  "icon": "mdi:heart-pulse",
  "entity_category": "diagnostic",
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"],
    "name": "Genius Gateway",
    "manufacturer": "Genius Gateway Project",
    "model": "Genius Gateway",
    "sw_version": "1.1.0",
    "configuration_url": "http://192.168.1.100"
  }
}
```

**:material-format-list-bulleted: Payload Fields**

- `~` - Topic prefix (base path for relative references)
- `name` - Entity name shown in Home Assistant
- `unique_id` - Unique identifier for this sensor
- `state_topic` - Relative path to state topic
- `value_template` - Jinja2 template to extract state from JSON
- `icon` - Material Design icon identifier
- `entity_category` - Category (`diagnostic` for system status)
- `device` - Device information object
  - `identifiers` - Device identifier (array with gateway device ID)
  - `name` - Device name shown in Home Assistant
  - `manufacturer` - Project name
  - `model` - Device model
  - `sw_version` - Current firmware version
  - `configuration_url` - Web interface URL (only included if gateway has valid IP)

**State Topic:** `{discovery_prefix}genius-gateway/{gateway_device_id}/status/state`

**:material-code-json: State Payload**
```json
{
  "state": "online"
}
```

**:material-publish: Publishing Behavior**

- Published when MQTT connection is established
- Re-published when MQTT settings change
- State published immediately after config
- Published only if [Home Assistant Integration is enabled](../setup/connections.md#device-publishing)

**:material-home-assistant: Home Assistant Integration**

- Automatically creates gateway device with all associated entities
- Provides device information panel with manufacturer, model, and firmware version
- Links to gateway web interface via configuration URL

---

#### Diagnostic Sensors

The gateway publishes two diagnostic sensors that share a common state topic for efficient updates.

##### Free Heap Sensor

**:material-message-outline: Config Topic**
```
{discovery_prefix}sensor/{gateway_device_id}/free_heap/config
```

**Example:** `homeassistant/sensor/genius-gateway-1a2b3c4d5e6f/free_heap/config`

**:material-information-outline: Description:** Monitors available heap memory as percentage of total heap

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Config Payload**
```json
{
  "~": "homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f",
  "name": "Free Heap",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_free_heap",
  "state_topic": "~/diagnostics/state",
  "value_template": "{{value_json.free_heap_percent|round(1)}}",
  "unit_of_measurement": "%",
  "state_class": "measurement",
  "icon": "mdi:memory",
  "entity_category": "diagnostic",
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"]
  }
}
```

##### Core Temperature Sensor

**:material-message-outline: Config Topic**
```
{discovery_prefix}sensor/{gateway_device_id}/core_temp/config
```

**Example:** `homeassistant/sensor/genius-gateway-1a2b3c4d5e6f/core_temp/config`

**:material-information-outline: Description:** Monitors ESP32 internal core temperature

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Config Payload**
```json
{
  "~": "homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f",
  "name": "Core Temperature",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_core_temp",
  "state_topic": "~/diagnostics/state",
  "value_template": "{{value_json.core_temp|round(1)}}",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "state_class": "measurement",
  "icon": "mdi:thermometer",
  "entity_category": "diagnostic",
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"]
  }
}
```

##### Diagnostic State Topic

**:material-message-outline: Topic**
```
{discovery_prefix}genius-gateway/{gateway_device_id}/diagnostics/state
```

**Example:** `homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f/diagnostics/state`

**:material-information-outline: Description:** Combined state topic for both diagnostic sensors

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** false

**:material-code-json: Payload**
```json
{
  "free_heap_percent": 73.5,
  "core_temp": 42.3
}
```

**:material-format-list-bulleted: Payload Fields**

- `free_heap_percent` - Available heap memory as percentage of total heap (float, 0-100)
- `core_temp` - ESP32 internal core temperature in degrees Celsius (float)

**:material-publish: Publishing Behavior**

- Published immediately after config messages (initial state)
- Published every 60 seconds via timer
- Re-published when MQTT connection is established
- Published only if [Home Assistant Integration is enabled](../setup/connections.md#device-publishing)

**:material-home-assistant: Home Assistant Integration**

- Automatically creates sensor entities with proper units and device classes
- Enables historical data tracking and graphing
- Can trigger automations based on memory or temperature thresholds
- Useful for monitoring gateway health and detecting potential issues

---

#### Restart Button

**:material-message-outline: Config Topic**
```
{discovery_prefix}button/{gateway_device_id}/restart/config
```

**Example:** `homeassistant/button/genius-gateway-1a2b3c4d5e6f/restart/config`

**:material-information-outline: Description:** Button entity for remotely restarting the gateway

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Config Payload**
```json
{
  "~": "homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f",
  "name": "Restart",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_restart",
  "command_topic": "~/restart/command",
  "payload_press": "PRESS",
  "icon": "mdi:restart",
  "entity_category": "config",
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"]
  }
}
```

**:material-format-list-bulleted: Key Fields**

- `command_topic` - Topic where button press commands are published
- `payload_press` - Payload sent when button is pressed (`"PRESS"`)
- `icon` - Restart icon for visual identification
- `entity_category` - Category (`config` for configuration entities)

**Command Topic:** `{discovery_prefix}genius-gateway/{gateway_device_id}/restart/command`

**:material-code-json: Command Payload**
```
"PRESS"
```

**:material-publish: Publishing Behavior**

- Config published when MQTT connection is established
- Config re-published when MQTT settings change
- Gateway subscribes to command topic automatically
- Trigger immediate gateway restart on command reception

**:material-home-assistant: Home Assistant Integration**

- Automatically creates button entity
- Press button to restart gateway remotely
- Useful for maintenance and troubleshooting without physical access
- Can be used in automations (e.g., scheduled restarts)

---

#### Configuration Switches

Four switches provide remote control of gateway configuration settings that affect device discovery and alert processing.

**:material-message-outline: Config Topic Pattern**
```
{discovery_prefix}switch/{gateway_device_id}/{switch_suffix}/config
```

**Switch Types:**

| Suffix | Display Name | Setting Controlled |
|--------|--------------|-------------------|
| `alert_unknown` | Alert on Unknown Detectors | Process alerts from unregistered smoke detectors |
| `line_commissioning` | Add Line from Commissioning | Auto-add alarm lines from commissioning packets |
| `line_alarm` | Add Line from Alarm | Auto-add alarm lines from alarm packets |
| `line_test` | Add Line from Line Test | Auto-add alarm lines from line test packets |

**:material-code-json: Config Payload Example (Alert on Unknown Detectors)**
```json
{
  "~": "homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f",
  "name": "Alert on Unknown Detectors",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_alert_unknown",
  "state_topic": "~/alert_unknown/state",
  "command_topic": "~/alert_unknown/set",
  "payload_on": "ON",
  "payload_off": "OFF",
  "state_on": "ON",
  "state_off": "OFF",
  "icon": "mdi:toggle-switch-off-outline",
  "entity_category": "config",
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"]
  }
}
```

**:material-format-list-bulleted: Key Fields**

- `state_topic` - Topic publishing current switch state
- `command_topic` - Topic accepting switch commands
- `payload_on` / `payload_off` - Payloads for ON/OFF commands
- `state_on` / `state_off` - State values indicating ON/OFF
- `entity_category` - Category (`config` for configuration entities)

**State Topics:**
```
{discovery_prefix}genius-gateway/{gateway_device_id}/{switch_suffix}/state
```

**Command Topics:**
```
{discovery_prefix}genius-gateway/{gateway_device_id}/{switch_suffix}/set
```

**:material-code-json: State/Command Payloads**

Turn setting ON:
```
"ON"
```

Turn setting OFF:
```
"OFF"
```

**:material-publish: Publishing Behavior**

- Config published when MQTT connection is established
- Config re-published when MQTT settings change
- State published immediately after config (initial state)
- State published when setting changes (via web interface or MQTT)
- Gateway subscribes to all command topics automatically

**:material-home-assistant: Home Assistant Integration**

- Automatically creates switch entities grouped under gateway device
- Toggle switches to modify gateway settings remotely
- State synchronization between Home Assistant and gateway web interface
- Can be used in automations for dynamic behavior control
- Useful for temporarily enabling/disabling features without accessing web UI

!!! info "Setting Synchronization"
    Changes made via Home Assistant switches are immediately reflected in the gateway web interface and vice versa. All settings are synchronized bidirectionally.

---

#### Firmware Update Entity

The gateway publishes an update entity that integrates with Home Assistant's update functionality to manage firmware updates.

**:material-message-outline: Config Topic**
```
{discovery_prefix}update/{gateway_device_id}/firmware/config
```

**Example:** `homeassistant/update/genius-gateway-1a2b3c4d5e6f/firmware/config`

**:material-information-outline: Description:** Update entity for firmware management with automatic version checking

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Config Payload**
```json
{
  "~": "homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f",
  "name": "Firmware Update",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_firmware_update",
  "state_topic": "~/update/state",
  "command_topic": "~/update/install",
  "payload_install": "INSTALL",
  "title": "Genius Gateway Firmware",
  "device_class": "firmware",
  "entity_category": "config",
  "icon": "mdi:cloud-download",
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"]
  }
}
```

**:material-format-list-bulleted: Key Fields**

- `state_topic` - Topic publishing firmware version information
- `command_topic` - Topic accepting update installation commands
- `payload_install` - Payload to trigger update installation (`"INSTALL"`)
- `device_class` - Device class (`firmware` for firmware updates)
- `title` - Update entity title
- `entity_category` - Category (`config` for configuration entities)

**State Topic:** `{discovery_prefix}genius-gateway/{gateway_device_id}/update/state`

**:material-code-json: State Payload**

When no update is available:
```json
{
  "installed_version": "1.1.0",
  "latest_version": "1.1.0",
  "title": "Genius Gateway Firmware 1.1.0",
  "release_url": "https://github.com/hmbacher/genius-gateway/releases/latest"
}
```

When update is available:
```json
{
  "installed_version": "1.1.0",
  "latest_version": "1.2.0",
  "title": "Genius Gateway Firmware 1.2.0",
  "release_url": "https://github.com/hmbacher/genius-gateway/releases/latest"
}
```

**:material-format-list-bulleted: State Fields**

- `installed_version` - Currently installed firmware version
- `latest_version` - Latest available firmware version from GitHub releases
- `title` - Human-readable update title
- `release_url` - URL to GitHub releases page

**Command Topic:** `{discovery_prefix}genius-gateway/{gateway_device_id}/update/install`

**:material-code-json: Command Payload**
```
"INSTALL"
```

**:material-publish: Publishing Behavior**

- Config published when MQTT connection is established
- Config re-published when MQTT settings change
- State published after GitHub version check (periodic and on-demand)
- State updated when new firmware version is detected
- Gateway subscribes to command topic automatically

**:material-cog: Update Process**

1. Gateway periodically checks GitHub for new releases
2. When update is available, state reflects new version
3. User can trigger installation via Home Assistant or web interface
4. Gateway downloads firmware from GitHub releases
5. Firmware is validated and installed
6. Gateway automatically restarts with new version

**:material-home-assistant: Home Assistant Integration**

- Automatically creates update entity with current and available versions
- Shows "Update Available" badge when new firmware is released
- Links to GitHub releases page for release notes
- Click "Install" in Home Assistant to trigger OTA update
- Progress tracking via web interface and event notifications

!!! warning "Internet Connection Required"
    Firmware updates require an active internet connection to download from GitHub. The gateway must be able to reach `github.com` and `objects.githubusercontent.com`.

---

### Smoke Detectors

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