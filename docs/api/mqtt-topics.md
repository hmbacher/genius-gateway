# MQTT Topics Documentation

## Introduction

This document describes the MQTT topics published by the Genius Gateway. The gateway supports both [Home Assistant auto-discovery :material-open-in-new:](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery){ target=_blank } and custom MQTT topics for integration with other home automation systems connecting to a MQTT broker.

### MQTT Configuration

MQTT functionality is mainly configured via [web frontend](../features/index.md) or HTTP API endpoints:

- **Framework MQTT Settings:** `/rest/mqttSettings` - Basic MQTT connection settings
- **Simple Alarm Publishing:** `/rest/alarm-publishing` - Genius Gateway specific publishing options

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

Published by the gateway before it disconnects on purpose (a restart), and automatically by the broker when the gateway just disappears (LWT message):
```json
"offline"
```

The broker notices a gateway that disappeared - no power, no WiFi - after one and a half times the MQTT keep alive: about three minutes with the default of 120 seconds.

**:material-eye-outline: Who should watch it**

- Home Assistant does: every entity is unavailable while this topic says `offline`, see [Availability](#availability)
- Any other system that reads the [global alarm topic](#global-alarm-state-topic), the [global fault topic](#global-fault-state-topic) or the [MQTT value tree](#mqtt-value-tree): their retained values stay as they are when the gateway goes offline
---

## Home Assistant Auto-Discovery

The gateway supports [Home Assistant's MQTT Discovery protocol :material-open-in-new:](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery){ target=_blank } for automatic device integration.

### Availability {#availability}

Every discovery config - of the gateway, the smoke detectors and the alarm lines - refers to the [device status topic](#device-status-topic-lwt). While it says `offline`, Home Assistant shows the entity as *unavailable* instead of its last state. The config examples below do not repeat this block:

```json
{
  "availability": [
    { "topic": "genius-gateway/1a2b3c4d5e6f/status" }
  ],
  "availability_mode": "all"
}
```

- `online` and `offline` are the payloads Home Assistant expects by default, so they are not listed
- An entity with a condition of its own - a status value that is unknown until the detector reported it, a button that is blocked while a transmission runs - keeps it as a second entry of the list. With `availability_mode` `all` the entity is available only if the gateway is online **and** its own condition holds
- The one exception is the [Status sensor](#device-status-sensor) of the gateway: it shows the status topic itself and therefore says `offline`

### Gateway Device

The Genius Gateway itself is published as a Home Assistant device with diagnostic sensors, remote control buttons, and configuration switches.

**Requirements:**

- [Home Assistant Integration must be enabled](../setup/connections.md#home-assistant-integration)
- MQTT broker must be connected

#### Overview

The gateway device publishes:

- **1 Status Sensor** - Connection state of the gateway (`online` / `offline`)
- **3 Diagnostic Sensors** - Free heap memory (%), core temperature (°C) and the reason of the last reset
- **1 Restart Button** - Remote gateway restart capability
- **4 Configuration Switches** - Remote control of gateway settings

**Topic Structure:**
```
{discovery_prefix}{gateway_device_id}
```

!!! tip "Gateway Device ID Format"
    The `gateway_device_id` follows the format `genius-gateway-{device-mac}`, where `device-mac` is the device's MAC address with lowercase hex digits and without any delimiters.  
    E.g.: `1A:2B:3C:4D:5E:6F` :material-arrow-right: `genius-gateway-1a2b3c4d5e6f`

    It is derived from the **firmware name**, not from the **Device Name** configured under [Home Assistant Integration](../setup/connections.md#home-assistant-integration). The Device Name is purely cosmetic - it sets the label shown in Home Assistant and never affects topics or the device's identity in HA's registry, so renaming the gateway is always safe.

!!! warning "Changed in v1.6.0"
    Up to v1.5.0 the base topic carried a redundant namespace level and derived both that namespace and the device ID from the (user-editable) Device Name, producing paths like `homeassistant/genius-gateway-1a2b3c4d5e6f/genius-gateway-1a2b3c4d5e6f-1a2b3c4d5e6f/...`. Both levels collapsed into a single `{gateway_device_id}` in v1.6.0. See [Home Assistant Integration](../setup/connections.md#home-assistant-integration) for what happens on upgrade.

---

#### Device Status Sensor

**:material-message-outline: Topic**
```
{discovery_prefix}sensor/{gateway_device_id}/status/config
```

**Example:** `homeassistant/sensor/genius-gateway-1a2b3c4d5e6f/status/config`

**:material-information-outline: Description:** Connection state of the gateway: `online`, or `offline` when it has disconnected or disappeared. It reads the [device status topic](#device-status-topic-lwt) directly and is the one entity that stays available while the gateway is offline

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Payload**
```json
{
  "~": "homeassistant/genius-gateway-1a2b3c4d5e6f",
  "name": "Status",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_status",
  "state_topic": "genius-gateway/1a2b3c4d5e6f/status",
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
- `state_topic` - The [device status topic](#device-status-topic-lwt)
- `icon` - Material Design icon identifier
- `entity_category` - Category (`diagnostic` for system status)
- `device` - Device information object
  - `identifiers` - Device identifier (array with gateway device ID)
  - `name` - Device name shown in Home Assistant
  - `manufacturer` - Project name
  - `model` - Device model
  - `sw_version` - Current firmware version
  - `configuration_url` - Web interface URL (only included if gateway has valid IP)

**State Topic:** the [device status topic](#device-status-topic-lwt), with the plain text payload `online` or `offline`

**:material-publish: Publishing Behavior**

- Published when MQTT connection is established
- Re-published when MQTT settings change
- State published immediately after config
- Published only if [Home Assistant Integration is enabled](../setup/connections.md#home-assistant-integration)

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
  "~": "homeassistant/genius-gateway-1a2b3c4d5e6f",
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
  "~": "homeassistant/genius-gateway-1a2b3c4d5e6f",
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

##### Last Reset Reason Sensor

**:material-message-outline: Config Topic**
```
{discovery_prefix}sensor/{gateway_device_id}/reset_reason/config
```

**Example:** `homeassistant/sensor/genius-gateway-1a2b3c4d5e6f/reset_reason/config`

**:material-information-outline: Description:** Why the gateway last restarted - power-on, software restart, exception, watchdog, brownout and so on. The value is the same text as *Reset Reason* in the [System Status](../setup/system.md) of the web interface. The sensor is an enum: Home Assistant knows every possible value, so an automation can react to a certain reason.

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Config Payload**
```json
{
  "~": "homeassistant/genius-gateway-1a2b3c4d5e6f",
  "name": "Last Reset Reason",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_reset_reason",
  "state_topic": "~/diagnostics/state",
  "value_template": "{{value_json.reset_reason}}",
  "device_class": "enum",
  "options": [
    "Reset reason can not be determined",
    "Reset due to power-on event",
    "Reset by external pin (not applicable for ESP32)",
    "Software reset via esp_restart",
    "Software reset due to exception/panic",
    "Reset (software or hardware) due to interrupt watchdog",
    "Reset due to task watchdog",
    "Reset due to other watchdogs",
    "Reset after exiting deep sleep mode",
    "Brownout reset (software or hardware)",
    "Reset over SDIO",
    "Reset by USB peripheral",
    "Reset by JTAG",
    "Reset due to efuse error",
    "Reset due to power glitch detected",
    "Reset due to CPU lock up (double exception)",
    "Unknown reset reason"
  ],
  "icon": "mdi:restart-alert",
  "entity_category": "diagnostic",
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"]
  }
}
```

##### Diagnostic State Topic

**:material-message-outline: Topic**
```
{discovery_prefix}{gateway_device_id}/diagnostics/state
```

**Example:** `homeassistant/genius-gateway-1a2b3c4d5e6f/diagnostics/state`

**:material-information-outline: Description:** Combined state topic of the diagnostic sensors

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** false

**:material-code-json: Payload**
```json
{
  "free_heap_percent": 73.5,
  "core_temp": 42.3,
  "reset_reason": "Reset due to power-on event"
}
```

**:material-format-list-bulleted: Payload Fields**

- `free_heap_percent` - Available heap memory as percentage of total heap (float, 0-100)
- `core_temp` - ESP32 internal core temperature in degrees Celsius (float)
- `reset_reason` - Reason of the last reset (string), one of the `options` of the [Last Reset Reason Sensor](#last-reset-reason-sensor); it stays the same until the gateway restarts

**:material-publish: Publishing Behavior**

- Published immediately after config messages (initial state)
- Published every 60 seconds via timer
- Re-published when MQTT connection is established
- Published only if [Home Assistant Integration is enabled](../setup/connections.md#home-assistant-integration)

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

!!! tip "Discovery `node_id`"
    Home Assistant requires the discovery `node_id` to be a single topic level, so config topics always use the flat `{gateway_device_id}` (or, for sub-devices, `genius-{device_id}`) - never a nested path. Because the device ID already embeds the gateway's MAC address, entities from different gateway instances never collide on a shared MQTT broker.

**:material-information-outline: Description:** Button entity for remotely restarting the gateway

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Config Payload**
```json
{
  "~": "homeassistant/genius-gateway-1a2b3c4d5e6f",
  "name": "Restart",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_restart",
  "command_topic": "~/restart/command",
  "payload_press": "PRESS",
  "icon": "mdi:restart",
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"]
  }
}
```

**:material-format-list-bulleted: Key Fields**

- `command_topic` - Topic where button press commands are published
- `payload_press` - Payload sent when button is pressed (`"PRESS"`)
- `icon` - Restart icon for visual identification
- No `entity_category` - Restart appears under **Controls** on the HA device page

**Command Topic:** `{discovery_prefix}{gateway_device_id}/restart/command`

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
  "~": "homeassistant/genius-gateway-1a2b3c4d5e6f",
  "name": "Alert on Unknown Detectors",
  "unique_id": "genius-gateway-1a2b3c4d5e6f_alert_unknown",
  "state_topic": "~/gateway/state",
  "value_template": "{{ value_json.alert_unknown }}",
  "command_topic": "~/gateway/alert_unknown/set",
  "icon": "mdi:toggle-switch-off-outline",
  "entity_category": "config",
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"]
  }
}
```

**:material-format-list-bulleted: Key Fields**

- `state_topic` - Shared JSON topic publishing all switch states
- `value_template` - Template to extract specific switch state from JSON
- `command_topic` - Individual topic accepting `ON` / `OFF` commands
- `entity_category` - `config` for configuration entities

**Central State Topic:**
```
{discovery_prefix}{gateway_device_id}/gateway/state
```

**:material-code-json: State Payload (JSON with all switches)**
```json
{
  "alert_unknown": "ON",
  "line_commissioning": "OFF",
  "line_alarm": "OFF",
  "line_test": "ON"
}
```

**Individual Command Topics:**
```
{discovery_prefix}{gateway_device_id}/gateway/{objectId}/set
```

**Examples:**
- `homeassistant/genius-gateway-1a2b3c4d5e6f/gateway/alert_unknown/set`
- `homeassistant/genius-gateway-1a2b3c4d5e6f/gateway/line_commissioning/set`
- `homeassistant/genius-gateway-1a2b3c4d5e6f/gateway/line_alarm/set`
- `homeassistant/genius-gateway-1a2b3c4d5e6f/gateway/line_test/set`

**:material-code-json: Command Payloads**

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
- **Combined JSON state published immediately after configs (initial state)**
- **Single JSON state published when any setting changes** (via web interface or MQTT)
- Gateway subscribes to all individual command topics automatically

**:material-home-assistant: Home Assistant Integration**

- Automatically creates switch entities grouped under gateway device
- Toggle switches to modify gateway settings remotely
- State synchronization between Home Assistant and gateway web interface
- Can be used in automations for dynamic behavior control
- Useful for temporarily enabling/disabling features without accessing web UI
- **Efficient state updates** - All switches updated with a single MQTT message

!!! info "Setting Synchronization"
    Changes made via Home Assistant switches are immediately reflected in the gateway web interface and vice versa. All settings are synchronized bidirectionally.

!!! tip "Efficient State Updates"
    All four configuration switches share a single JSON state topic. When any setting changes (even just one), all states are published in a single MQTT message, reducing network traffic and ensuring atomic updates.

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
  "~": "homeassistant/genius-gateway-1a2b3c4d5e6f",
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

**State Topic:** `{discovery_prefix}{gateway_device_id}/update/state`

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

**Command Topic:** `{discovery_prefix}{gateway_device_id}/update/install`

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

Each configured smoke detector is published as an **HA sub-device** nested under the main Genius Gateway device.

**Requirements:**

- [Home Assistant Integration must be enabled](../setup/connections.md#home-assistant-integration)
- MQTT broker must be connected

#### Overview

Each smoke detector sub-device publishes:

- **1 binary_sensor** (Control) - `smoke` device class, alarm state `ON`/`OFF`
- **4 binary_sensor** (Diagnostic) - battery low, smoke detector fault, radio module fault, dismount; the **current status**, each available once its state is known
- **10 sensor** (Diagnostic) - deinstallation count, last service timestamp, radio module model, alarm line ID, alarm line, production date, radio module serial, total alarm count, 3-month alarm count, radio interference; available after acoustic readout

The status entities reflect the newest of: a [status change](../reverse-engineering/protocol-analysis.md#status-change) received over the air (live), the last acoustic readout, or a [manual status reset](../features/device-management.md#current-status). Each one reports as **unavailable** until the gateway knows the state of its flag - a freshly commissioned gateway cannot tell "OK" from "never announced", since smoke detectors announce a status flag only when it changes. The readout entities report as **unavailable** until the first acoustic readout is performed via the gateway's [SmartSonic interface](../features/device-management.md).

**Topic Structure:**

All smoke detector topics are nested under the main gateway base topic. The sub-device identifier is based on the **stable internal device ID** (not the serial number), so topics remain stable across serial-number corrections:
```
{discovery_prefix}{main_device_id}/genius-{device_id}/...
```

For example, with the default prefix and gateway device ID `genius-gateway-aabbcc`:
```
homeassistant/genius-gateway-aabbcc/genius-1746234159/...
```

!!! tip "Device ID vs Serial Number"
    `device_id` is a stable internal identifier assigned when the detector is first registered. It is independent of the smoke detector serial number and does not change if the serial number is corrected later.

---

#### Smoke Alarm Binary Sensor

**:material-message-outline: Config Topic Pattern**
```
{discovery_prefix}binary_sensor/{device_id}/smoke/config
```

**Example:** `homeassistant/binary_sensor/genius-1746234159/smoke/config`

**:material-information-outline: Description:** Main alarm state sensor for the smoke detector

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Config Payload**
```json
{
  "~": "homeassistant/genius-gateway-aabbcc/genius-1746234159",
  "name": "Smoke Detector",
  "unique_id": "genius-1746234159_smoke",
  "device_class": "smoke",
  "state_topic": "~/smoke/state",
  "entity_picture": "http://192.168.1.100/hekatron-genius-plus-x.png",
  "device": {
    "identifiers": ["genius-1746234159"],
    "name": "Genius Plus X",
    "manufacturer": "Hekatron Vertriebs GmbH",
    "model": "Genius Plus X",
    "serial_number": "12345678",
    "suggested_area": "Living Room",
    "configuration_url": "http://192.168.1.100/gateway/smoke-detectors",
    "via_device": "genius-gateway-aabbcc"
  }
}
```

**:material-format-list-bulleted: Key Fields**

- `~` - Base topic (sub-device path nested under main gateway device)
- `device_class` - `smoke`
- `state_topic` - Plain string `ON` / `OFF`
- `entity_picture` - Detector icon URL (omitted if no valid IP)
- `device.identifiers` - Stable device ID (`genius-{device_id}`)
- `device.model` - Smoke detector model name (e.g. `"Genius Plus X"`)
- `device.name` - Smoke detector model name; the detector's location instead if the *Update entity IDs when a detector's location changes* option is enabled
- `default_entity_id` - Only sent while that option is enabled (not part of the example above): explicit entity ID `<domain>.<prefix>_<entity>`, e.g. `binary_sensor.living_room_smoke_detector`. The prefix is the detector's location reduced to `[a-z0-9_]` or, without a usable location, `genius_<serial number>`. All entities of the detector, including the diagnostic ones, carry it. When the prefix changes, the gateway clears the retained config topics of the detector (empty payload, QoS 1), waits 3 s and publishes them again
- `device.suggested_area` - Populated from the detector's configured location
- `device.via_device` - Links this sub-device to the main gateway device

**State Topic:** `{base_topic}/genius-{device_id}/smoke/state`

**:material-code-json: State Values**

- `OFF` - Not alarming
- `ON` - Actively alarming

**:material-publish: Publishing Behavior**

- Config published when detector is first [registered](../features/device-management.md#adding-a-new-detector) or on MQTT (re)connect
- State published when alarm state changes and on MQTT (re)connect

---

#### Diagnostic Entities (Status and Readout)

All 14 diagnostic entities share a **single state topic** that carries a JSON object with all values. Availability is folded into the same JSON payload - no separate availability topic is needed.

**:material-message-outline: Shared State Topic**
```
{base_topic}/genius-{device_id}/diagnostics/state
```

**:material-speedometer: QoS:** 0 &nbsp;&nbsp; **:material-content-save-outline: Retain:** true

**JSON Payload Example:**
```json
{
  "available": true,
  "battery_low": false,
  "device_fault": false,
  "radio_fault": false,
  "dismounted": false,
  "deinstall_count": 2,
  "last_readout": "2026-04-30T14:22:00",
  "radio_module_model": "FM Basis X",
  "alarm_line_id": 123456789,
  "alarm_line": "A.0",
  "production_date": "01.03.22",
  "radio_module_serial": 987654321,
  "alarm_count_total": 5,
  "alarm_count_3m": 1,
  "radio_interference": 12.5
}
```

Each entity's discovery config uses `availability_template` on this same topic, in two flavours:

- **Status entities** - a field is present only once the state of its flag is known (from a status change received over the air, a readout or a manual reset). The entity is available while its own field is defined:

    ```yaml
    availability_topic: "{base_topic}/genius-{device_id}/diagnostics/state"
    availability_template: "{{ 'online' if value_json.battery_low is defined else 'offline' }}"
    ```

- **Readout entities** - `available` is `false` (and the readout fields absent) until the first successful acoustic readout:

    ```yaml
    availability_topic: "{base_topic}/genius-{device_id}/diagnostics/state"
    availability_template: "{{ 'online' if value_json.available else 'offline' }}"
    ```

**Discovery Config Topic Pattern:**
```
{discovery_prefix}{component}/genius-{device_id}/{object_id}/config
```

**Status binary_sensor entities** (`value_template` extracts a boolean as `ON`/`OFF`):

| Object ID | Name | Device Class | Status flag | `value_template` |
|-----------|------|-------------|-------------|-----------------|
| `battery_low` | Battery | `battery` | `0x0004` BattLow | `{{ 'ON' if value_json.battery_low else 'OFF' }}` |
| `device_fault` | Smoke Detector State | `problem` | `0x0002` Error | `{{ 'ON' if value_json.device_fault else 'OFF' }}` |
| `radio_fault` | Radio Module State | `problem` | `0x0008` Radio | `{{ 'ON' if value_json.radio_fault else 'OFF' }}` |
| `dismounted` | Dismount | `tamper` | `0x0020` Dismount | `{{ 'ON' if value_json.dismounted else 'OFF' }}` |

An acoustic readout reports battery, smoke detector fault and radio module fault as well, so a readout makes these three known. The dismount flag is only ever reported over the air.

`radio_fault` is also `true` while the radio module is [reported missing](../features/device-management.md#radio-link-supervision) by another detector. When that ends, the field goes back to what the radio link flag says - and is left out again if that flag is not known.

**Diagnostic sensor entities** (`value_template` extracts the relevant field):

| Object ID | Name | Unit / Class | JSON field |
|-----------|------|-------------|-----------|
| `deinstallation_count` | Deinstallation Count | `total_increasing` | `deinstall_count` |
| `last_readout` | Last Service | `timestamp` | `last_readout` |
| `radio_module_model` | Radio Module Model | - | `radio_module_model` |
| `alarm_line_id` | Alarm Line ID | - | `alarm_line_id` |
| `alarm_line` | Alarm Line | - | `alarm_line` |
| `production_date` | Production Date | - | `production_date` |
| `radio_module_serial` | Radio Module Serial | - | `radio_module_serial` |
| `alarm_count_total` | Alarms (Total) | `total_increasing` | `alarm_count_total` |
| `alarm_count_3m` | Alarms (3 Months) | `measurement` | `alarm_count_3m` |
| `radio_interference` | Radio Interference | `%` / `measurement` | `radio_interference` |

**:material-publish: Publishing Behavior**

- Configs published on MQTT (re)connect alongside the main smoke sensor config
- State published immediately after a received status change, a manual status reset and a successful acoustic readout
- State re-published on MQTT (re)connect; status fields absent until known, `available: false` until first readout

!!! note "Migration from pre-1.x firmware"
    Firmware upgrading from a version that used per-entity state topics will automatically clear the 13 stale retained state topics and the old `readout/avail` topic on the first MQTT connect after upgrade. Entity history and customizations in Home Assistant are preserved because all object IDs remain unchanged.

---

#### Radio Link Supervision Entities (FM Pro X)

Smoke detectors with an FM Pro X radio module get two more diagnostic entities. Such modules send a daily series when their radio link supervision is switched on, so the gateway can tell when it last heard them - see [Radio link supervision](../features/device-management.md#radio-link-supervision). The entities are created and removed with the stored radio module model.

**:material-publish: State Topic** (shared, retained)
```
homeassistant/genius-gateway-1a2b3c4d5e6f/genius-1/link/state
```

```json
{
  "last_seen": "2026-11-23T17:14:12",
  "signal": -87
}
```

| Field | Entity | Device class / unit | Available |
|:------|:-------|:--------------------|:----------|
| `last_seen` | Last Seen | `timestamp` | once a packet of the module was received since the gateway started |
| `signal` | Signal Strength | `signal_strength` / dBm | once a packet the module transmitted itself was received |

**:material-publish: Publishing Behavior**

- Configs published on MQTT (re)connect alongside the other entities of the detector
- State published whenever a packet of the module is received
- The values are kept in the gateway's memory only. After a restart the state is published empty, and both entities are **unavailable** until the module is heard again - a module is never assumed to be there

---

### Alarm Lines

The following topics enable remote control of alarm line actions (line tests and fire alarm tests) via MQTT.

**Requirements:**

- [Alarm lines must be configured](../setup/configure-gateway.md#adding-alarm-lines) in Genius Gateway
- [Home Assistant Integration must be enabled](../setup/connections.md#home-assistant-integration)
- MQTT broker must be connected

#### Overview

Each configured alarm line is published as an **HA sub-device** nested under the main Genius Gateway device. Each sub-device contains:

- **4 Button entities** for triggering actions (line test start/stop, fire alarm start/stop)
- **1 Sensor entity** for transmission state monitoring

**Topic Structure:**

All alarm line topics are nested under the main gateway base topic:
```
{discovery_prefix}{main_device_id}/genius-alarmline-{line_id}/...
```

For example, with the default prefix and a gateway device ID `genius-gateway-aabbcc`:
```
homeassistant/genius-gateway-aabbcc/genius-alarmline-123456789/...
```

---

#### Button Entities Configuration

**:material-message-outline: Topic Pattern**
```
{discovery_prefix}button/genius-alarmline-{line_id}/{object_id}/config
```

**:material-information-outline: Description:** Configuration messages for Home Assistant button discovery

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**Button Object IDs:**

- `linetest-start` - Start Line Test
- `linetest-stop` - Stop Line Test
- `firealarm-start` - Start Fire Alarm
- `firealarm-stop` - Stop Fire Alarm

**:material-code-json: Example Payload (Start Line Test)**
```json
{
  "~": "homeassistant/genius-gateway-aabbcc/genius-alarmline-123456789",
  "name": "Start Line Test",
  "unique_id": "genius-alarmline-123456789_linetest-start",
  "command_topic": "~/linetest-start/set",
  "icon": "mdi:map-marker",
  "availability": [{
    "topic": "homeassistant/genius-gateway-aabbcc/genius-alarmline-123456789/transmission/state",
    "value_template": "{% if value == 'Nothing' %}online{% else %}offline{% endif %}"
  }],
  "availability_mode": "all",
  "device": {
    "identifiers": ["genius-alarmline-123456789"],
    "name": "Alarm Line 'First Floor'",
    "manufacturer": "Genius Gateway Project",
    "model": "Genius Plus X Alarm Line",
    "via_device": "genius-gateway-aabbcc"
  }
}
```

**:material-format-list-bulleted: Key Fields**

- `~` - Base topic (sub-device path nested under main gateway device)
- `name` - Button name displayed in Home Assistant
- `unique_id` - Unique identifier for this button
- `command_topic` - Topic where HA sends button press commands
- `icon` - Material Design icon identifier
- `availability` - Button unavailable while a transmission is active
- `device.via_device` - Links this sub-device to the main gateway device in HA

**:material-publish: Publishing Behavior**

- Published when alarm line is first created
- Re-published on MQTT (re)connect via HA sub-device mechanism
- Published only if [Home Assistant Integration is enabled](../setup/connections.md#home-assistant-integration)

**:material-home-assistant: Home Assistant Integration**

- Each alarm line appears as a separate HA sub-device under the gateway
- All 4 buttons and the sensor are grouped on that device's page
- Buttons become unavailable during active transmissions

---

#### Command Topics

**:material-message-outline: Topic Pattern**
```
{main_base_topic}/genius-alarmline-{line_id}/{button_object_id}/set
```

**:material-information-outline: Description:** HA sends to these topics when a button is pressed in the UI or via automation

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** false

**Button Object IDs → Actions:**

- `linetest-start/set` → Start Line Test RF transmission
- `linetest-stop/set` → Stop Line Test RF transmission
- `firealarm-start/set` → Start Fire Alarm RF transmission
- `firealarm-stop/set` → Stop Fire Alarm RF transmission

**:material-publish: Command Behavior**

- Each button has its own dedicated command topic (no wildcard, no JSON payload parsing)
- Triggers RF transmission immediately if no other transmission is active
- Ignored if a transmission is currently in progress (buttons shown as unavailable via availability topic)
- Updates transmission state sensor on start; resets to `"Nothing"` on completion

!!! warning "Transmission Blocking"
    Only one transmission can be active at a time. Buttons are automatically shown as unavailable in HA while a transmission is in progress.

---

#### Transmission State Sensor

**:material-message-outline: Config Topic**
```
{discovery_prefix}sensor/genius-alarmline-{line_id}/transmission/config
```

**Example:** `homeassistant/sensor/genius-alarmline-123456789/transmission/config`

**:material-message-outline: State Topic**
```
{main_base_topic}/genius-alarmline-{line_id}/transmission/state
```

**Example:** `homeassistant/genius-gateway-aabbcc/genius-alarmline-123456789/transmission/state`

**:material-information-outline: Description:** Current transmission status of the alarm line (plain string, not JSON)

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: State Values**

- `Nothing` - No active transmission, all buttons available
- `Line Test Start` - Line test start transmission in progress
- `Line Test Stop` - Line test stop transmission in progress
- `Fire Alarm Start` - Fire alarm start transmission in progress
- `Fire Alarm Stop` - Fire alarm stop transmission in progress

**:material-publish: Publishing Behavior**

- Published when transmission starts (state shows action type)
- Published when transmission completes (state resets to `Nothing`)
- Published when transmission times out (state resets to `Nothing`)
- Re-published on MQTT (re)connect via HA sub-device mechanism
- Controls button availability (buttons shown as unavailable when state ≠ `Nothing`)

**:material-home-assistant: Home Assistant Integration**

- Appears under the alarm line sub-device as a Diagnostic sensor
- Used by button availability templates
- Can trigger automations based on transmission state changes

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

- Published when an alarm starts on any smoke detector
- Published when an alarm ends on any smoke detector (silenced by detector or manually via the API)
- Published when all alarms are reset at once (via the end alarms API)
- Published when MQTT connection is established or re-established - overwrites any stale retained message left from before a restart

!!! info "Alarm state after restart"
    Genius Gateway does not persist the active alarm state across restarts. After a restart, `isAlarming` is always `false` and `numAlarmingDevices` is always `0`, regardless of the state before the restart. The retained message on the broker is updated immediately on MQTT connect.

!!! note "Independent of Home Assistant integration"
    Simple alarm publishing works independently of the [Home Assistant integration](../features/smart-home-integration.md#home-assistants-mqtt-discovery). It is controlled solely by the **Enable simple alarm publishing** toggle and does not require `HAIntegrationEnabled`.

- Published only if [simple alarm publishing](../setup/connections.md#simple-alarm-publishing) is enabled

**:material-home-automation: Integration**

This topic enables integration with *all* smart home systems that support MQTT.
---

## Global Fault State Topic

**:material-message-outline: Topic (Default)**
```
smarthome/genius-gateway/fault
```

**:material-information-outline: Description:** Status summary of all smoke detectors - battery low, device faults, radio link faults, dismounting

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Payload:**

Published if no smoke detector reports a fault:
```json
{
  "numFaulty": 0,
  "numBatteryLow": 0,
  "devices": []
}
```

Published when one smoke detector reports a low battery (example):
```json
{
  "numFaulty": 1,
  "numBatteryLow": 1,
  "devices": [
    {
      "sn": 2191914555,
      "location": "Kitchen",
      "status": ["battery_low"]
    }
  ]
}
```

**:material-format-list-bulleted: Payload Fields**

- `numFaulty` - Number of smoke detectors with at least one active fault
- `numBatteryLow` - Number of smoke detectors reporting a low battery
- `devices` - The smoke detectors counted in `numFaulty`:
    - `sn` - Smoke detector serial number
    - `location` - Configured location
    - `status` - The active faults:

        | Value | Flag | Meaning |
        |-------|------|---------|
        | `battery_low` | `0x0004` | Battery low |
        | `device_fault` | `0x0002` | Device fault |
        | `radio_fault` | `0x0008` | Radio link fault |
        | `dismounted` | `0x0020` | Dismounted from its base |
        | `reported_missing` | - | Set by the gateway: another detector reports this detector's radio module as missing |

    - `info` - Only present if the detector has further active flags that are no fault: `fault_muted` (`0x0010`, a radio link or dismount fault was muted on the detector) or `flag_0xNNNN` (a flag not known yet, reported by value so nothing is lost)

The flags reflect each detector's [current status](../features/device-management.md#current-status): received over the air or taken over from an acoustic readout - whichever is newest.

!!! warning "No fault listed does not mean all is well"
    Smoke detectors announce a status change once. A gateway that has not heard about a flag - because it was put into operation later, or missed the announcement - does not know its state, and an unknown state is not listed. `numFaulty: 0` therefore means that no fault is known, not that every detector was confirmed to be fine.

**:material-publish: Publishing Behavior**

- Published whenever the payload changes - a fault is reported or cleared, a status is reset manually, a readout changes the status, or a listed detector is renamed or deleted
- When fault publishing is switched off or its topic is changed, the retained message on the old topic is removed (an empty retained payload is published)
- Published when MQTT connection is established or re-established, and when the publishing settings change

!!! info "Status after restart"
    Unlike the alarm state, the status of each smoke detector is persisted - smoke detectors announce a flag only when it changes, so a restart must not forget a pending battery warning. A flag stays active until the smoke detector clears it or it is [reset manually](../features/device-management.md#current-status).

!!! note "Independent of Home Assistant integration"
    Simple fault publishing works independently of the [Home Assistant integration](../features/smart-home-integration.md#home-assistants-mqtt-discovery). It is controlled solely by the **Enable simple fault publishing** toggle.

- Published only if [simple fault publishing](../setup/connections.md#simple-alarm-publishing) is enabled

---

## MQTT Value Tree {#mqtt-value-tree}

For smart home systems other than Home Assistant (openHAB, ioBroker, Node-RED, FHEM, shell scripts, ...) the gateway can publish every value of every smoke detector and of every alarm line to a **topic of its own, as plain text**: `true` / `false`, a number or text - no JSON to parse. The idea and the planning logic were contributed by danielrdt (#27).

**:material-message-outline: Root topic**
```
genius-gateway/{device-mac}
```
The root can be changed in the [settings](../setup/connections.md#mqtt-value-tree) (`treeBaseTopic`; empty = the default above, with the MAC address in lowercase and without colons). A smoke detector is addressed by its **internal device ID** (`{id}`, the number also used in the Home Assistant sub-device name `genius-{id}`), which stays the same when its location is changed or its smoke detector is replaced.

**:material-speedometer: QoS:** 0 &nbsp;&nbsp; **:material-content-save-outline: Retain:** true

| Topic | Payload |
|:------|:--------|
| `{root}/alarm` | `true` / `false`: at least one smoke detector is alarming |
| `{root}/alarming_count` | integer: smoke detectors that are alarming |
| `{root}/fault_count` | integer: smoke detectors with at least one fault |
| `{root}/devices/{id}/alarm` | `true` / `false` |
| `{root}/devices/{id}/location` | text |
| `{root}/devices/{id}/serial/detector` | integer: serial number of the smoke detector |
| `{root}/devices/{id}/serial/radio` | integer: serial number of the radio module |
| `{root}/devices/{id}/line_id` | integer: alarm line ID of the radio module |
| `{root}/devices/{id}/battery_low` | `true` / `false`, empty while not known |
| `{root}/devices/{id}/device_fault` | `true` / `false`, empty while not known |
| `{root}/devices/{id}/radio_fault` | `true` / `false`, empty while not known: radio link fault |
| `{root}/devices/{id}/dismounted` | `true` / `false`, empty while not known |
| `{root}/devices/{id}/reported_missing` | `true` / `false`: another radio module reports this one as missing - see [Radio link supervision](../features/device-management.md#radio-link-supervision) |
| `{root}/devices/{id}/last_seen` | ISO 8601 (UTC): last packet of the radio module. FM Pro X only, empty until the module was heard since the gateway started |
| `{root}/devices/{id}/rssi` | integer, dBm: signal strength of the last packet the radio module transmitted itself. FM Pro X only |
| `{root}/lines/{lineId}/name` | text: name of the alarm line (empty if the line has no name) |
| `{root}/lines/{lineId}/alarm` | `true` / `false`: at least one detector of the line is alarming |
| `{root}/lines/{lineId}/alarming_count` | integer: detectors of the line that are alarming |
| `{root}/lines/{lineId}/fault_count` | integer: detectors of the line with at least one fault |
| `{root}/lines/{lineId}/device_count` | integer: detectors on the line |

`{lineId}` is the alarm line ID as a decimal number, the same value as `devices/{id}/line_id`. A line is published if it is in the list of [alarm lines](../features/alarm-lines-management.md) or if a detector is on it; detectors without a known line and the broadcast line are not lines.

The four status topics follow the [current status](../features/device-management.md#current-status) of the detector: a flag has a value once the detector reported it over the air or an acoustic readout delivered it, and none before - the gateway does not publish an OK that nobody confirmed.

**:material-publish: Publishing Behavior**

- **Only changes are published.** `last_seen` at most once a minute per detector, `rssi` on a change of 3 dB or after 10 minutes at the latest. Alarms and faults go out with the next check, within about a second.
- **A value that is not known has no payload**: its retained topic is cleared (empty payload). An empty `battery_low` or `last_seen` therefore means "not known", never a stale value.
- After every (re)connect all topics are sent again, the ones without a value as clears, so no stale retained value survives a restart of the gateway. This takes a few seconds: the messages go out in batches of 25 per second. The alarm topics go out first, and a value that changes meanwhile is sent ahead of the rest.
- Deleting a detector clears all its topics. Switching the tree off, or changing the root, clears everything published under the old root.
- **Leftovers are removed.** With every full publish - after a (re)connect, and when the tree is switched on - the gateway looks under its root for retained topics of smoke detectors and alarm lines that are not part of the installation any more, and clears them. Such topics are left behind when the gateway restarts before it could clear them, for example when a detector is deleted right after a restart. Only topics of the tree's own layout are touched (`devices/<id>/...` and `lines/<line id>/...` with the names listed above); anything else under the root is left alone. Do not let another system keep retained topics of this layout under the same root.
- Topics under a root that is no longer configured are not found by this. After changing the root or switching the tree off, give the gateway about ten seconds to clear the old topics before restarting it.
- The values are separate topics, so two of them cannot be read atomically.
- The values stay as they are when the gateway goes offline. Watch the [device status topic](#device-status-topic-lwt) to notice an outage; with the default root it sits right next to the tree (`genius-gateway/<mac>/status`).
- The tree only publishes. Commands (line test, fire alarm) are available as [Home Assistant buttons](#button-entities-configuration) and over the [REST API](http-api.md).

!!! note "Independent of the other MQTT features"
    The value tree works independently of the [Home Assistant integration](../features/smart-home-integration.md#home-assistants-mqtt-discovery) and of the [simple alarm and fault topics](../setup/connections.md#simple-alarm-publishing); any of them can be switched on together.
