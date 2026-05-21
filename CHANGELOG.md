# v1.3.0
## Upgrade Notes
When upgrading from v1.2.x, three persisted-settings files are affected. All three migrate transparently — no manual reconfiguration is required:

- **Home Assistant settings are auto-migrated from `mqtt-settings.json` to a new `haSettings.json`.** The HA enable flag and discovery prefix used to live in `/config/mqtt-settings.json` under the keys `HAIntegrationEnabled` / `HAMQTTDiscoveryPrefix`. They now live in a new file `/config/haSettings.json` under renamed keys (`enabled`, `discovery_prefix`) alongside three new fields (`device_name`, `manufacturer`, `model`). On first boot after the upgrade, `HASettingsService` checks for the legacy keys and copies them into the new file, so HA integration stays enabled and your discovery prefix is preserved. The orphaned legacy keys in `mqtt-settings.json` are cleared the next time the MQTT settings are saved via the UI.
- **Smoke detector list is auto-migrated (v0 → v1).** Model enum values were renumbered (Genius Plus X: `0` → `3`, FM Basis X: `0` → `4`) and the obsolete `radioModule.productionDate` field is stripped on first load. New diagnostic fields introduced in v1.3.0 (drift state, dirt forecast, warranty flags, battery state, last self-test, radio interference, line/switch masks, etc.) default to zero/false and only populate after the next acoustic readout of each detector.
- **Alarm lines with reserved IDs are dropped.** Any alarm line previously stored with id `0` (None) or `0xFFFFFFFF` (Broadcast) is silently removed on load. Normally invisible; when `FT_ALLOW_BROADCAST` is enabled, the broadcast line is re-added automatically on boot.

All other persisted settings (WiFi, AP, NTP, MQTT broker connection, security, gateway flags, packet visualizer, WSLogger) keep their existing files and field names.

## Features

### Acoustic Smoke Detector Readout
- **Acoustic detection via microphone**: Read out a smoke detector's diagnostic data acoustically using the device running the web interface (phone, laptop, tablet)
  - A dedicated **Acoustic Readout** dialog guides you through the process step-by-step
  - The microphone captures and decodes the ultrasonic acoustic signal emitted by compatible Genius smoke detectors
  - Decoded data is shown live and imported directly into the detector configuration on confirmation
  - Includes audio test files and a WAV generator for development/testing

### Device Details & Diagnostics
- **Device Details dialog**: New per-device details view showing full smoke detector diagnostic data, radio module info, and alarm log in one place
- **Fault warnings after readout/import**: If a device has malfunctioning sensor values after a readout or import, a fault summary dialog is shown immediately, listing the affected detectors and faults
- **Smoke detector faults exposed as Home Assistant diagnostic entities** per smoke detector device:
  - Smoke chamber: drift value, drift defect, contamination forecast, coverage rate, warranty voided
  - Battery: voltage, state
  - Radio module: line number, radio state mask, radio switch mask, interference level, network fault
  - Last readout timestamp
  - All diagnostics published on a single shared state MQTT topic per detector

### Home Assistant Framework (Internal)
- Replaced the manual HA publishing approach with a full entity/device class hierarchy:
  `HADevice`, `HADeviceIdentity`, `HAEntityBase`, `HACommandEntity`, `HASensorEntity`, `HAActuatorEntity`,
  `HABinarySensor`, `HAButton`, `HASwitch`, `HASensor`, `HALight`, `HAGroupedSwitchPublisher`, `HAGroupedSensorPublisher`
- Sub-device support: each smoke detector and alarm line is now its own HA sub-device, enabling clean grouping in Home Assistant
- `HASettingsService`: HA configuration (enable/disable, MQTT prefix, device identity) is now a dedicated settings service with its own UI tab, separated from general MQTT settings
- Stable namespaced MQTT discovery topics — topic stays constant even when the device is renamed

### Home Assistant — General Improvements
- Gateway hostname and HA device name now include the unique device ID suffix by default, preventing collisions in multi-gateway setups
- Restart button correctly registered as a **Control** entity (not Diagnostic)

### Alarm Line Management UI
- **Live spinners reflect external trigger sources**: when a line test or fire alarm is triggered from Home Assistant or via MQTT, any open Web UI now shows the spinner on the corresponding alarm line for the duration of the transmission, instead of staying idle. Backed by a new `alarm-line-action-started` WebSocket event carrying `{lineId, action}`, emitted alongside the existing `alarm-line-action-finished` event

### Smoke Detector Management UI
- **Delete all detectors** button (trash-x icon) in the toolbar: removes all smoke detectors in one step after a confirmation dialog; disabled when no devices are configured
- **Import migration**: configuration files from older backup formats (v0) are automatically migrated to the current format (v1) on import, with a success notification and prompt to re-export; only files with a newer-than-current version are blocked
- **Alarm state handling on import**: if any device in an import file is marked as alarming, a dialog now asks whether to keep or clear the alarm state before sending to the backend
  - **Keep Alarm State**: imports as-is; connected integrations (e.g. Home Assistant) may trigger automations — useful for testing purposes
  - **Clear Alarm State**: resets `isAlarming` on all affected devices and closes any open alarm log entry with a new `ByImport` ending reason and the current timestamp
- **Alarming devices visually highlighted in the device list**: rows with `isAlarming === true` are rendered with the daisyUI `error` theme (`bg-error` + `text-error-content`) on both mobile cards and desktop rows, with a filled flame icon next to the location name to disambiguate "active alarm" from a generic error state; all inner status icons (OK, fault, readout age) inherit `text-current` on alarming rows so they remain legible on the red background
- **Top-bar health indicator** reworked with four states and is a link to the overview (`/`):
  - **Semitransparent heart** — no smoke detectors configured yet
  - **Green heart** — all detectors healthy (readout present and recent, no faults)
  - **Yellow `heart-exclamation`** — at least one detector has no readout, a stale readout (> 1 year), or any fault
  - **Red hexagon** — at least one detector is actively alarming
- **Overview device cards** are now links to the Smoke Detectors page
- **Overview device cards reworked with health-driven theming**:
  - **Blue (`bg-primary`)**: non-alarming, has acoustic readout, no faults, last readout ≤ 1 year ago
  - **Yellow (`bg-warning`)**: any of (a) no readout data, (b) one or more fault states, (c) last readout > 1 year ago
  - **Red (`bg-error`)**: alarming devices
  - Status icon stack in the top-right corner: flame icon on alarming cards only; on primary cards a "service ok" award icon; on warning cards a "no acoustic readout" microphone-off icon (case a), a fault icon (case b), and/or a stale-readout calendar icon (case c); foreign-detector indicator appended last
- **Fault status icon** changed from circle-x (ambiguous "close" appearance) to alert-circle, in both the device list and the overview cards
- Mobile-friendly card layout for the smoke detector and alarm line list pages

## Changed
- `ConfirmDialog`: added optional `onCancel` callback prop; cancel button now always closes the modal before invoking the callback (callers no longer need to call `modals.close()` themselves on cancel); added optional `cancelClass` prop; replaced `any` types with typed `IconComponent`/`Labels`; added `aria-modal` and `aria-labelledby` for screen reader support
- `InfoDialog`: added `variant` prop (`info`/`warning`/`error`) which drives the icon and button colour; variant icon is shown in the dialog title; replaced `any` types with typed `IconComponent`/`DismissDef`; added `aria-modal` and `aria-labelledby`
- `UriInput`: refactored from two-effect ping-pong (with fragile `writingValue` boolean flag) to a single-source-of-truth approach using `lastExternalValue`; initial field state is now seeded from the `value` prop at script init (was always `''` before the first effect ran); regex escaping hardened for scheme names that contain special characters; exposes a `dirty` bindable prop (tracks `lastSyncedAssembled`) for parent dirty-state detection
- `AcousticDetectionDialog`: retry button, 60 s live countdown, collapsible diagnostic log (`<details>`), ARIA live region (`role="status" aria-live="polite"`), `motion-reduce:animate-none` on animated elements
- MQTT settings page: dirty-state tracking now disables the **Apply** button when no changes are pending, including URI field changes fed via the new `UriInput.dirty` prop
- HA configuration tab: replaced `formErrors` state object with derived error flags; **Apply** button is disabled immediately when validation fails; `isValidDiscoveryPrefix` now checks `//` before stripping a trailing slash; inline error messages use slide-transition `<div>` elements
- Alarm indicator on the top bar now links to `/gateway/smoke-detectors` instead of `/` when no devices are configured
- Alarm log sorted newest-first
- Date/time formatting uses `navigator.language` with `'en-GB'` fallback throughout the UI
- Dead `bind:this` / `formField` binding removed from all settings forms — it was never read; inline `e.preventDefault()` replaces the `preventDefault()` wrapper function in all form `onsubmit` handlers
- `$state()` on typed settings objects now supplies explicit default values, eliminating svelte-check errors and undefined reads before the first REST fetch completes

## Bugfixes
- Fixed `AcousticDetectionDialog` crashing immediately on open with `this.callbacks.onLog is not a function` — the `onLog` no-op callback was missing from the `AcousticDetectionSession` constructor call
- Fixed alarm line with reserved ID `0xFFFFFFFF` (broadcast) being stored and published to HA when a readout returned that line ID: frontend `checkAndOfferAlarmLine` now guards against both `0x00000000` (unassigned) and `0xFFFFFFFF` (broadcast); backend `AlarmLines::update` silently skips both reserved IDs so a crafted PUT cannot persist them into `_state.lines`
- Fixed UTC timestamp parsing: `iso8601_to_time_t` was calling `mktime()` — which interprets a broken-down struct as local time — on what is actually a UTC struct, shifting all reconstructed dates by the device's UTC offset; replaced with a direct epoch calculation using the Hinnant civil-days O(1) formula (no platform `timegm()` dependency)
- Fixed device registration type being silently downgraded to `Manual`/`Packet` on plain edits (name, location change): the registration field is now only updated when the incoming PUT includes a `readoutTime`, preventing a frontend that omits the field from overwriting the stored registration type
- Fixed `HAGroupedSensorPublisher` callback leak: `HAService::onPublishAll` / `onUnpublishAll` now return a `CallbackId`; the publisher stores both IDs and removes them from `HAService` in its destructor, preventing stale lambda closures from accumulating in the callback vectors across repeated add/remove cycles (e.g. clear + import test runs). The `_alive` guard is kept as a belt-and-suspenders safety net for any in-flight invocation that races the removal
- Fixed HA device not removed from Home Assistant when HA integration is disabled: `HAGroupedSensorPublisher` and `HAGroupedSwitchPublisher` entities (gateway diagnostics, configuration switches) were not unpublished because there was no unpublish callback mechanism. Added `HAService::onUnpublishAll()` parallel to `onPublishAll()`; both publishers register an unpublish callback in `begin()`. Added `unpublishAll()` to `HAGroupedSwitchPublisher`. `HASettingsService` now calls `unpublishAll()` before `setEnabled(false)` so MQTT is still connected when the empty retained payloads are sent
- Fixed CC1101 initial reset sequence, improving startup reliability in some hardware setups
- Fixed HA discovery for dynamically added smoke detectors: `publishAll()` is now called so all diagnostic entities are announced immediately, not only on the next periodic publish cycle
- Fixed HA sub-device map being keyed on serial number instead of stable `device.id`, causing stale entries after device renames
- Fixed `iso8601_to_time_t` to accept ISO 8601 timestamps without a milliseconds component (e.g. from certain gateway exports)
- Fixed `radioInterference` being stored as a negative value when the radio module reports no signal; values are now clamped to ≥ 0
- Fixed simple alarm MQTT topic not being republished when `isAlarming` changes via REST (e.g. after importing alarming devices). Cached alarm state (`_isAlarming`/`_numAlarming`) is now derived from the device list inside `_updateAlarmingState()`, which detects any change and publishes — independent of the `ALARM_STATE_CHANGE` event that the previous gating relied on
- Fixed alarm log dialog showing "No data" for the End column on alarms ended by import: the end-time gate was hardcoded to `endingReason === 0 || === 1` and didn't recognize the new `ByImport` reason. The check now uses `endingReason !== AlarmActive`, and the ending-reason column shows a dedicated import icon for `ByImport`

# v1.2.1
## Bugfixes
- Fixed initial CC1101 reset routine, that caused CC1101 initialization to fail in some setups

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