---
icon: tabler/adjustments
---

# Gateway Settings

This page describes the gateway-specific settings that control how the Genius Gateway processes incoming radio packets and manages smoke detector devices.

!!! info "Access Required"
    Gateway settings require **administrator** privileges to view and modify.

## Overview

The Gateway Settings allow you to configure the automatic behavior of the Genius Gateway when processing radio communications from Hekatron Genius Plus X smoke detectors. These settings determine how the gateway responds to different types of packets and whether it automatically adds newly discovered devices or alarm line IDs.

![Gateway Settings](../assets/images/software/gg-gateway-settings.png)

## Settings Categories

### :tabler-alert-hexagon: Alarming

#### Process alerts from unknown smoke detectors

**Default:** Enabled (recommended)

When enabled, the gateway will automatically add smoke detectors to your device list when they trigger an alarm, even if you haven't manually added them before. This ensures that alarm notifications are never missed from detectors in your wireless network.

**When enabled:**

- Unknown detectors triggering an alarm are automatically added to the device list
- Alarm states from these newly-added detectors are immediately processed and displayed
- MQTT notifications are published for the new devices (if MQTT is enabled)
- You'll be notified of fire alarms from all detectors in range, even newly installed ones

**When disabled:**

- Only pre-configured smoke detectors in your device list can trigger alarms
- Alarm packets from unknown detectors are ignored
- You must manually add detectors before they can send alarm notifications
- Useful for strictly controlled environments where only specific detectors should be monitored

!!! tip "Recommended Setting"
    Keep this **enabled** for maximum safety. It ensures you never miss an alarm from a smoke detector in your network, even if you forgot to add it to the gateway.

!!! warning "Security Consideration"
    When enabled, any Genius Plus X detector in radio range can be automatically added during an alarm. If you need strict control over which detectors are monitored, disable this setting and manually add only authorized devices.

### :tabler-topology-ring-2: Alarm Lines

Alarm line IDs are unique identifiers that group smoke detectors into logical networks. These IDs are essential for [alarm line management functions](alarm-lines-management.md) such as line tests and test alarms. The gateway can automatically learn and store these IDs from various types of radio packets.

For detailed information about alarm line IDs and their role in the Hekatron Genius system, see [Protocol Analysis - General Packet Structure](../reverse-engineering/protocol-analysis.md#general-packet-structure).

#### Add alarm line ID of received commissioning packets automatically

**Default:** Enabled (recommended)

When enabled, the gateway automatically extracts and stores alarm line IDs from commissioning packets. Commissioning packets are sent when smoke detectors are initially set up or when new detectors are added to an existing alarm line.

**When enabled:**

- New alarm line IDs are discovered during detector commissioning
- Existing alarm line IDs are updated when detectors join a line
- Your alarm line list stays synchronized with your detector network
- No manual entry of alarm line IDs required after commissioning

**When disabled:**

- Alarm line IDs must be manually added via the [alarm lines interface](alarm-lines-management.md)
- Commissioning activities won't update your alarm line list

**Learn more:** [Alarm Line Commissioning Packets](../reverse-engineering/protocol-analysis.md#alarm-line-commissioning)

#### Add alarm line ID of received alarming/silencing packets automatically

**Default:** Enabled (recommended)

When enabled, the gateway automatically extracts and stores alarm line IDs from alarm start and alarm stop (silencing) packets. These packets are transmitted when a smoke detector triggers an alarm or when an alarm is silenced.

**When enabled:**

- Alarm line IDs are learned during actual fire alarm events
- Your alarm line list is updated when alarms occur
- Useful for discovering alarm lines in active environments

**When disabled:**

- Alarm packets don't update your alarm line list
- Alarm line IDs must be added through other means

**Learn more:** [Alarm Packets](../reverse-engineering/protocol-analysis.md#alarm-packets)

#### Add alarm line ID of received line test packets automatically

**Default:** Enabled (recommended)

When enabled, the gateway automatically extracts and stores alarm line IDs from line test packets. Line test packets are sent when the line test function is activated on a smoke detector to verify communication between all detectors on that alarm line.

**When enabled:**

- Alarm line IDs are learned during line tests
- Your alarm line list updates when detectors perform line tests
- Convenient discovery method without triggering actual alarms

**When disabled:**

- Line test packets don't update your alarm line list
- Alarm line IDs must be added manually

**Learn more:** [Line Test Packets](../reverse-engineering/protocol-analysis.md#line-test-packets)

## Related Documentation

- [Device Management](device-management.md) - Managing smoke detectors
- [Alarm Lines Management](alarm-lines-management.md) - Managing alarm line IDs and functions
- [Protocol Analysis](../reverse-engineering/protocol-analysis.md) - Technical details about packet types