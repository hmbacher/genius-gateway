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
{discovery_prefix}button/{topicNamespace}/{gateway_device_id}/restart/config
```

**Example:** `homeassistant/button/genius-gateway/genius-gateway-1a2b3c4d5e6f/restart/config`

!!! tip "`topicNamespace` format"
    `topicNamespace` is the slugified device name, e.g. `genius-gateway`. Together with the device ID, the config topic node becomes `genius-gateway/genius-gateway-1a2b3c4d5e6f`. This scoping ensures entities from different gateway instances never collide on a shared MQTT broker.

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
  "device": {
    "identifiers": ["genius-gateway-1a2b3c4d5e6f"]
  }
}
```

**:material-format-list-bulleted: Key Fields**

- `command_topic` - Topic where button press commands are published
- `payload_press` - Payload sent when button is pressed (`"PRESS"`)
- `icon` - Restart icon for visual identification
- No `entity_category` — Restart appears under **Controls** on the HA device page

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
{discovery_prefix}genius-gateway/{gateway_device_id}/gateway/state
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
{discovery_prefix}genius-gateway/{gateway_device_id}/gateway/{objectId}/set
```

**Examples:**
- `homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f/gateway/alert_unknown/set`
- `homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f/gateway/line_commissioning/set`
- `homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f/gateway/line_alarm/set`
- `homeassistant/genius-gateway/genius-gateway-1a2b3c4d5e6f/gateway/line_test/set`

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

Each configured smoke detector is published as an **HA sub-device** nested under the main Genius Gateway device. Each sub-device contains a single binary sensor entity reporting the alarm state.

**Requirements:**

- [Home Assistant Integration must be enabled](../setup/connections.md#device-publishing)
- MQTT broker must be connected

#### Overview

Each smoke detector sub-device publishes:

- **1 binary_sensor entity** — `smoke` device class, state `ON`/`OFF`

**Topic Structure:**

All smoke detector topics are nested under the main gateway base topic:
```
{discovery_prefix}{main_namespace}/{main_device_id}/genius-{smoke_detector_sn}/...
```

For example, with the default prefix and gateway device ID `genius-gateway-aabbcc`:
```
homeassistant/genius-gateway/genius-gateway-aabbcc/genius-12345678/...
```

---

#### Binary Sensor Configuration Topic

**:material-message-outline: Topic Pattern**
```
{discovery_prefix}binary_sensor/{topicNamespace}/{gateway_device_id}/genius-{smoke_detector_sn}/smoke/config
```

**Example:** `homeassistant/binary_sensor/genius-gateway/genius-gateway-aabbcc/genius-12345678/smoke/config`

**:material-information-outline: Description:** Configuration message for Home Assistant binary sensor discovery

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Payload**
```json
{
  "~": "homeassistant/genius-gateway/genius-gateway-aabbcc/genius-12345678",
  "name": "Genius Plus X",
  "device_class": "smoke",
  "state_topic": "~/smoke/state",
  "json_attributes_topic": "~/smoke/attributes",
  "entity_picture": "http://192.168.1.100/hekatron-genius-plus-x.png",
  "device": {
    "identifiers": ["genius-12345678"],
    "name": "Rauchmelder",
    "manufacturer": "Hekatron Vertriebs GmbH",
    "model": "Genius Plus X",
    "serial_number": "12345678",
    "suggested_area": "Living Room",
    "configuration_url": "http://192.168.1.100/gateway/smoke-detectors",
    "via_device": "genius-gateway-aabbcc"
  }
}
```

**:material-format-list-bulleted: Payload Fields**

- `~` - Base topic (sub-device path nested under main gateway device)
- `name` - Entity name shown in Home Assistant
- `device_class` - Device class (`smoke`)
- `state_topic` - Path to alarm state (plain string `ON`/`OFF`)
- `json_attributes_topic` - Path to entity attributes (production dates, FM Basis X info)
- `entity_picture` - URL to device icon (only included if gateway has valid IP)
- `device` - Device information block
  - `identifiers` - Device identifier (`genius-{sn}`)
  - `name` - Device name (`"Rauchmelder"`)
  - `manufacturer` / `model` / `serial_number` - Hardware metadata
  - `suggested_area` - Populated from the detector's configured location
  - `configuration_url` - Link to gateway smoke detector page (if IP available)
  - `via_device` - Links this sub-device to the main gateway device in HA

**:material-publish: Publishing Behavior**

- Published when smoke detector is first [created](../features/device-management.md#adding-a-new-detector) or [imported](../features/device-management.md#importing-configuration)
- Re-published on MQTT (re)connect via HA sub-device mechanism
- Published only if [device publishing](../setup/connections.md#device-publishing) is enabled

**:material-home-assistant: Home Assistant Integration**

- Each smoke detector appears as a separate HA sub-device under the gateway
- Provides device information panel with manufacturer, model, and serial number
- Links to gateway smoke detector configuration page

---

#### State Topic

**:material-message-outline: Topic**
```
{main_base_topic}/genius-{smoke_detector_sn}/smoke/state
```

**Example:** `homeassistant/genius-gateway/genius-gateway-aabbcc/genius-12345678/smoke/state`

**:material-information-outline: Description:** Current alarm state of the smoke detector (plain string)

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: State Values**

- `OFF` — Smoke detector not alarming (clear)
- `ON` — Smoke detector actively alarming

**:material-publish: Publishing Behavior**

- Published when device alarm state changes
- Re-published on MQTT (re)connect via HA sub-device mechanism
- Published only if [device publishing](../setup/connections.md#device-publishing) is enabled

---

#### Attributes Topic

**:material-message-outline: Topic**
```
{main_base_topic}/genius-{smoke_detector_sn}/smoke/attributes
```

**Example:** `homeassistant/genius-gateway/genius-gateway-aabbcc/genius-12345678/smoke/attributes`

**:material-information-outline: Description:** Additional device metadata displayed as entity attributes in Home Assistant

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: Payload**
```json
{
  "Production Date": "04.06.22",
  "FM Basis X - Serial": "87654321",
  "FM Basis X - Production Date": "01.03.22"
}
```

**:material-format-list-bulleted: Payload Fields**

- `Production Date` - Smoke detector production date (dd.mm.yy), omitted if unknown
- `FM Basis X - Serial` - Radio module serial number, omitted if unknown
- `FM Basis X - Production Date` - Radio module production date (dd.mm.yy), omitted if unknown

**:material-publish: Publishing Behavior**

- Published when sub-device is first registered (if MQTT connected)
- Re-published on every MQTT (re)connect
- Published only if [device publishing](../setup/connections.md#device-publishing) is enabled

---

### Alarm Lines

The following topics enable remote control of alarm line actions (line tests and fire alarm tests) via MQTT.

**Requirements:**

- [Alarm lines must be configured](../setup/configure-gateway.md#adding-alarm-lines) in Genius Gateway
- [Home Assistant Integration must be enabled](../setup/connections.md#device-publishing)
- MQTT broker must be connected

#### Overview

Each configured alarm line is published as an **HA sub-device** nested under the main Genius Gateway device. Each sub-device contains:

- **4 Button entities** for triggering actions (line test start/stop, fire alarm start/stop)
- **1 Sensor entity** for transmission state monitoring

**Topic Structure:**

All alarm line topics are nested under the main gateway base topic:
```
{discovery_prefix}{main_namespace}/{main_device_id}/genius-alarmline-{line_id}/...
```

For example, with the default prefix and a gateway device ID `genius-gateway-aabbcc`:
```
homeassistant/genius-gateway/genius-gateway-aabbcc/genius-alarmline-123456789/...
```

---

#### Button Entities Configuration

**:material-message-outline: Topic Pattern**
```
{discovery_prefix}button/{topicNamespace}/{gateway_device_id}/genius-alarmline-{line_id}/{object_id}/config
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
  "~": "homeassistant/genius-gateway/genius-gateway-aabbcc/genius-alarmline-123456789",
  "name": "Start Line Test",
  "unique_id": "genius-alarmline-123456789_linetest-start",
  "command_topic": "~/linetest-start/set",
  "icon": "mdi:map-marker",
  "availability": [{
    "topic": "homeassistant/genius-gateway/genius-gateway-aabbcc/genius-alarmline-123456789/transmission/state",
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
- Published only if [Home Assistant Integration is enabled](../setup/connections.md#device-publishing)

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
{discovery_prefix}sensor/{topicNamespace}/{gateway_device_id}/genius-alarmline-{line_id}/transmission/config
```

**Example:** `homeassistant/sensor/genius-gateway/genius-gateway-aabbcc/genius-alarmline-123456789/transmission/config`

**:material-message-outline: State Topic**
```
{main_base_topic}/genius-alarmline-{line_id}/transmission/state
```

**Example:** `homeassistant/genius-gateway/genius-gateway-aabbcc/genius-alarmline-123456789/transmission/state`

**:material-information-outline: Description:** Current transmission status of the alarm line (plain string, not JSON)

**:material-speedometer: QoS:** 0

**:material-content-save-outline: Retain:** true

**:material-code-json: State Values**

- `Nothing` — No active transmission, all buttons available
- `Line Test Start` — Line test start transmission in progress
- `Line Test Stop` — Line test stop transmission in progress
- `Fire Alarm Start` — Fire alarm start transmission in progress
- `Fire Alarm Stop` — Fire alarm stop transmission in progress

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
- Published when MQTT connection is established or re-established — overwrites any stale retained message left from before a restart

!!! info "Alarm state after restart"
    Genius Gateway does not persist the active alarm state across restarts. After a restart, `isAlarming` is always `false` and `numAlarmingDevices` is always `0`, regardless of the state before the restart. The retained message on the broker is updated immediately on MQTT connect.

!!! note "Independent of Home Assistant integration"
    Simple alarm publishing works independently of the [Home Assistant integration](../features/smart-home-integration.md#home-assistant-mqtt-discovery). It is controlled solely by the **Enable simple alarm publishing** toggle and does not require `HAIntegrationEnabled`.

- Published only if [simple alarm publishing](../setup/connections.md#simple-alarm-publishing) is enabled

**:material-home-automation: Integration**

This topic enables integration with *all* smart home systems that support MQTT.