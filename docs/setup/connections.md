---
icon: tabler/topology-star-3
---

# Connections

The Connections pages configure external network services that the Genius Gateway can integrate with, including MQTT for smart home integration and NTP for accurate time synchronization.

## :tabler-topology-star-3: MQTT

MQTT (Message Queuing Telemetry Transport) is a lightweight messaging protocol ideal for IoT devices and smart home integration. The Genius Gateway can publish device states and alarm events to an MQTT broker for integration with home automation systems like Home Assistant.

![MQTT Status](../assets/images/software/gg-connections-mqtt-general.png)

### MQTT Status

The MQTT status section displays self-explaning real-time connection information.

### :tabler-adjustments-alt: General Settings

Click the **General Settings** collapsible section to configure MQTT connection parameters.

!!! info "Administrator Access Required"
    MQTT settings can only be modified by users with administrator privileges.

#### Connection Settings

**Enable MQTT**

Toggle to enable or disable MQTT functionality. When disabled, no MQTT connections or publishing occur.

**URI**

The MQTT broker URI including protocol and port. Supported formats:

- **Unencrypted**, for example
    - `mqtt://broker.hivemq.com` or
    - `mqtt://broker.hivemq.com:1883` or
    - `mqtt://192.168.1.100:1883`
- **Encrypted (TLS/SSL)**, for example
    - `mqtts://mqtt.example.com` or
    - `mqtts://mqtt.example.com:8883` or

Common MQTT ports:

- **1883** for unencrypted MQTT (use `mqtt://` protocol)
- **8883** for MQTT over TLS/SSL (use `mqtts://` protocol)

!!! info "Validation"
    The URI must include the protocol (`mqtt://` or `mqtts://`) and a valid hostname or IP address.

**Username**

Optional username for MQTT broker authentication. Leave empty if the broker doesn't require authentication.

**Password**

Optional password for MQTT broker authentication. Leave empty if the broker doesn't require authentication.

!!! warning "Password Storage"
    Passwords are stored unencrypted in the device's file system. Ensure physical access to the device is restricted.

**Client ID**

Unique identifier for this MQTT client. Each device connecting to the same broker must have a unique client ID.

**Keep Alive**

MQTT keep-alive interval in seconds (1-600). The device sends a ping to the broker if no other messages are sent within this interval to maintain the connection.

**Publish Message Interval**

Minimum interval between published MQTT messages in milliseconds (0-1000).

!!! info "Disable Rate Limiting"
    0 = no rate limiting. Messages will be sent immediately.

This rate limiting may be used to prevent overwhelming the broker with rapid updates, if necessary.

**Clean Session**

When enabled, the broker discards all information about previous sessions when the client connects.

- **Enabled** (checked): Start with a clean slate, no retained messages or subscriptions
- **Disabled** (unchecked): Resume previous session with retained messages and subscriptions

!!! tip "Session Persistence"
    Genius Gateway does not rely on the MQTT broker's persistence function, as it publishes the state of all smoke detectors to the broker on every reconnect.

    It is therefore recommended to enable this feature to ensure a consistent state and stable connection at all times.

**Applying Changes**

Click **Apply Settings** to save and apply the MQTT configuration. The device will reconnect to the MQTT broker with the new settings.

### :tabler-bell: Alarm Publishing

The **Alarm Publishing** collapsible card configures a simple, platform-agnostic alarm notification via MQTT.

![Alarm Publishing settings](../assets/images/software/gg-connections-mqtt-alarm-publishing.png)

!!! info "Administrator Access Required"
    Smart home integration settings can only be modified by users with administrator privileges.

**Enable simple alarm publishing**

Toggle to enable/disable publishing alarm events to a central topic.

**Alarm Topic**

MQTT topic where alarm state is published when *any* smoke detector triggers an alarm.

??? info "Valid MQTT Topic Syntax"
    - 1-64 characters
    - no wildcards `+` or `#`
    - cannot start or end with `/`
    - no double slashes `//`
    - no special characters except `/`, `-`, and `_`

!!! abstract "Global Alarm State Topic"
    See [Global Alarm State Topic](../api/mqtt-topics.md#global-alarm-state-topic) for the exact payload format.

### :tabler-smart-home: Home Assistant Integration {#home-assistant-integration}

The **Home Assistant Integration** collapsible card enables [MQTT Discovery :material-open-in-new:](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery){ target=_blank } and configures the gateway's device identity in Home Assistant.

!!! info "Administrator Access Required"
    Home Assistant settings can only be modified by users with administrator privileges.

**Enable Home Assistant Integration**

Toggle to enable/disable publishing with MQTT Discovery support for:

- Gateway device with diagnostic sensors and remote controls
- Individual detector configurations and states
- Alarm line controls (if configured)

**Discovery Prefix**

Home Assistant MQTT Discovery prefix. All device discovery messages will use this prefix followed by the component type (e.g., `binary_sensor`, `button`, `sensor`). Default: `homeassistant/`.

**Example:** Using the default prefix creates discovery topics like:

- `homeassistant/binary_sensor/genius-gateway-<id>/…/config` (smoke detector)
- `homeassistant/button/genius-gateway-<id>/…/config` (alarm line button)

!!! warning "Home Assistant compatibility"
    The prefix must match the `discovery_prefix` configured in your Home Assistant instance.

**Device Name**

Name shown in Home Assistant's device registry. Leave empty to use the firmware's compile-time name (`Genius Gateway`).

**Manufacturer / Model**

Optional labels shown in the Home Assistant device info panel. Useful for multi-gateway deployments to distinguish individual units.

**Applying Changes**

Click **Apply Settings** to save. Changes take effect immediately — if MQTT is connected, updated discovery messages are published automatically.

!!! abstract "MQTT Topics Reference"
    See [Home Assistant Auto-Discovery](../api/mqtt-topics.md#home-assistant-auto-discovery) for topic structure and payload formats.

#### Related Documentation

Integration with Home Assistant is described in detail in [Smart Home Integration](../features/smart-home-integration.md).

## :tabler-clock-check: Network Time (NTP)

Network Time Protocol (NTP) synchronizes the device's internal clock with internet time servers, ensuring accurate timestamps for logs, alarms, and event recording.

![NTP Status](../assets/images/software/gg-connections-ntp.png)

### NTP Status

The status section displays various time synchronization information, updated every 5 seconds.

### Change NTP Settings

Click the **Change NTP Settings** collapsible section to configure NTP parameters.

!!! info "Administrator Access Required"
    NTP settings can only be modified by users with administrator privileges.

#### Enable NTP

Toggle checkbox to enable or disable NTP time synchronization.

#### Server

NTP server hostname or IPv4 address.

- **Format**: Valid domain name or IPv4 address (3-64 characters)
- **Default**: `pool.ntp.org` (public NTP pool server)

#### Pick Time Zone

Select your local timezone from the dropdown list. The device uses this to convert UTC time to your local time.

!!! info "Daylight Saving Time"
    The selected timezone automatically handles daylight saving time transitions where applicable.

#### Applying Changes

Click **Apply Settings** to save and apply the NTP configuration. The device will apply the new timezone and NTP server settings immediately and begin time synchronization.
