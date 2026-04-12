# v1.2.0
## Features
- **Build-target-aware GitHub Firmware Manager in web interface**
  - GitHub releases are filtered client-side by build target — only `.bin` assets whose filename contains the device's build target (e.g. `seeed-xiao-esp32s3`) are considered compatible
  - A **Hide incompatible build targets** toggle is displayed when at least one release contains an incompatible asset; it is checked by default and can be unchecked to reveal incompatible assets
  - Incompatible assets show a red install button with a warning indicator; attempting to install one shows a confirmation dialog to prevent accidental installs
  - The `GET /rest/github-release?all=true` response now wraps the release list in an envelope (`{ build_target, releases }`) so the frontend receives the device's build target in a single request
  - The `GET /rest/systemStatus` response now includes a `build_target` field

# v1.1.2
## Bugfixes
- Fixed [Simple Alarm Publishing](https://hmbacher.github.io/genius-gateway/features/smart-home-integration/#simple-alarm-publishing) via [Alarm Topic](https://hmbacher.github.io/genius-gateway/api/mqtt-topics/#global-alarm-state-topic) ([Issue #7](https://github.com/hmbacher/genius-gateway/issues/7))

# v1.1.1
## Bugfixes
- Fixed missing MQTT Discovery publishes (due to async-enqueued messages filling the outbox faster than it could drain)
- Fixed MQTT command topic parsing for Alarm Line IDs > 2147483647

# v1.1.0
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

# v1.0.1
## Bugfixes
- Fixed reconnect loop ([Issue #5](https://github.com/hmbacher/genius-gateway/issues/5))
- Fixed missing smoke detectors reading on initial login ([Issue #6](https://github.com/hmbacher/genius-gateway/issues/6))

# v1.0.0
Initial release