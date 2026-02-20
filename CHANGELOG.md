# v1.2.1
## Bugfixes
- Fixed missing MQTT Discovery publishes (due to async-enqueued messages filling the outbox faster than it could drain)
- Fixed MQTT command topic parsing for Alarm Line IDs > 2147483647


# v1.2.0
## Features
- Home Assistant Integration for Alarm Lines
  - One device for each configured Alarm Line
  - Start/Stop buttons for Line Test
  - Start/Stop buttons for Fire Alarm
  - Transmission State Entity
  - Discovery: automatic adding/removal in Home Assistant
- Home Assistant Integration for Genius Dateway
  - Central Genius Gateway device
  - Diagnostic information (free heap, core temperature)
  - Restart button
  - Genius Gateway Settings as Switches
  - Fully featured Update Entity, including version numbers, release name and update progress
  - Discovery: automatic adding/removal in Home Assistant
- Firmware Download from GitHub  
  Available releases can be downloaded and flashed via Web Interface (System/Firmware update)

# v1.1.0
## Bugfixes
- Fixed reconnect loop (#5)
- Fixed missing smoke detectors reading on initial login (#6)

# v1.0.0
Initial release