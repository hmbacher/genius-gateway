# v1.4.1

## Manual Alarm-Line Entry for Old FM Modules
Old-generation radio modules (**FM.Basis** / **FM.Pro**) cannot expose a trustworthy alarm line: they transmit `lineId=0` over SmartSonic and the line byte on the wire is unreliable. The Genius Gateway now mirrors the official Hekatron app's behavior for these modules instead of silently trusting bogus readout data.

- **Manual line entry required for old modules**: acoustic readout and radio-packet data for FM.Basis/FM.Pro modules no longer populate the alarm line automatically. Devices with an old module and no line yet are flagged **"Line required"** in the smoke-detector list and details dialog, with a one-click path into the new rotary-switch entry UI (major `A`–`H` / minor `0`–`9`, with `H` disabled for FM.Basis since Sammelalarm is Pro-only)
- **Device details dialog adapted for old modules**: radio status, DIP switches, and interference are hidden for FM.Basis/FM.Pro (these modules never report them); the alarm line section shows the manually-entered rotary line or a "Set Alarm Line manually" action instead
- **Rotary line follows the physical module on swap/replace**: when a radio module is matched by serial number across a readout/replace flow, its manually-entered line is carried over so installers don't have to re-enter it for the same hardware
- **Config migration v1 → v2**: existing configs with an old-module device that has a (untrustworthy) line carried over from previous firmware are migrated — the stale `lineId`/`lineCharacter`/`lineNumber` are purged unless they were already flagged as manually entered
- **"Smoke Detectors" column hidden when no compatible hardware is installed**: the Alarm Lines page's smoke-detector-assignment column (desktop table and mobile cards) is only shown when at least one device has a radio module capable of automatic line detection (FM.MCP, FM.Basis X, FM.Pro X) — installations with only old FM.Basis/FM.Pro hardware no longer see a column that's permanently empty
- **New `InfoPopover` component**: a click-to-open, rich-HTML tooltip (supports multi-line text, links, etc.) styled to match daisyUI's tooltip look — used to explain the smoke-detector-assignment restriction inline next to the column header / "No devices" state

## Bugfixes
- **Packet visualizer byte view now highlights serial numbers/line/hops for unrecognized packet types too**: the summary row above the byte view already extracted these fields for any packet (known or not), but the byte view itself fell back to a plain, untyped byte dump for unrecognized types — an inconsistency between what was described and what was shown. The common 24-byte header (counter, both radio-module serials, line, hops, sequence number) is now rendered with the same typed/colored blocks regardless of packet type
- **Device export always includes `radioInterference` and `lineId`**: both fields were previously omitted when zero. A genuine `radioInterference` reading of 0.0% would silently disappear from the export, indistinguishable from a device that had never been read out. Similarly, a `lineId` of 0 (unassigned) would be absent. Both fields are now always written so consumers can rely on their presence
- **SPI Pin Configuration dropdowns no longer clipped at card boundary**: the lower pin selectors in the SPI Pin Configuration dialog were cut off by the settings card's overflow boundary. The card now uses `overflow-x: clip` / `overflow-y: visible` so dropdown lists extend below the card as expected
- **Update indicator no longer shows incompatible releases**: the topbar firmware-update badge now only lights up when a newer release includes an asset that matches the current build target. Previously, any newer release triggered the indicator regardless of hardware compatibility, and clicking it immediately offered to install a binary that would fail or brick the device
- **GitHub OTA download no longer sends an empty URL to the device**: when no compatible binary was found for the build target, the backend returned an empty `download_url`. The device then attempted an OTA connection to an empty hostname, failing with a DNS error. The `update_available` flag is now only set when a matching binary exists, and the UI adds a defensive guard as a belt-and-suspenders check
- **Re-selecting the same firmware file now re-triggers the upload**: after clicking Abort on the confirmation dialog, the file input was not cleared. Because the browser suppresses `change` events when the same file is picked again, the upload could not be restarted without first picking a different file. The input is now reset on Abort so any file — including the same one — opens a fresh confirmation
- **Clean LittleFS reformat after a flash erase no longer spams the log**: mounting a freshly erased LittleFS partition always fails once before the framework auto-formats and remounts it. Previously this surfaced as raw internal `esp_littlefs` errors ("Corrupted dir pair", "Failed to initialize LittleFS"). It's now reported as a single clear log line, and genuine future mount corruption is still surfaced the same way
- **No more spurious "Migrating device config from v0 to v2" on a fresh device**: applying default device-config state (no config file on disk yet) was mistaken for a v0 file needing migration, logging two no-op migration messages on every first boot. The migration logic now only runs when a `devices` array is actually present

# v1.4.0

## Frontend / Firmware Version Sync
- **Version mismatch banner**: when a cached web UI is running against a newer firmware, an amber banner appears with a **Reload** button. Prevents the UI and backend from getting out of sync after an upgrade
- **Browser cache control fixed**: `index.html` was previously cached as immutable for up to a year, meaning browsers could serve a stale UI shell long after a firmware upgrade. It is now served with `no-cache` so the shell is always revalidated

## Device Details & Diagnostics
- **Radio Status badge aligned with the Hekatron Genius Home app**: the "Radio Status: OK / Fault" badge in the Device Details dialog now uses the same source bits as the vendor app, fixing a disagreement between the badge and the per-bit flag list below it
- **"FM Module Flags" subhead** added to visually separate the badge from the flag list beneath it
- **`RemoteBattLow` and `RemoteError` shown as warnings, not errors**: these flags describe the state of another device on the radio line, so amber warnings are more appropriate than red fault indicators

## CC1101 — Runtime SPI/GDO Pin Configuration

The SPI and GDO signal pins for the CC1101 transceiver can now be changed at runtime via **System → CC1101 → Pin Configuration** — no reflashing required. The UI shows only valid GPIOs for the target board and warns about conflicts with pins claimed by other roles. Changes take effect immediately without a reboot. Existing installs without a saved config continue to use the compile-time defaults.

## Smoke Detector List — Scaling to 50 Devices
Up to 50 smoke detectors are now supported on ESP32-S3 boards with PSRAM. See the new **[Memory Considerations](https://hmbacher.github.io/genius-gateway/setup/memory/)** documentation page for details.

- **Per-device REST endpoints**: add, update, delete, and reorder operations now target individual devices, avoiding the payload size limits that the previous bulk-POST approach hit at higher device counts
- **Chunked import for bulk operations**: large device lists are uploaded in small chunks and committed in a background task, keeping the UI responsive and eliminating the 15–20 s blocking commit that previously starved the WebSocket keepalive
- **PSRAM used for large collections**: the device list, import buffer, and HA framework data structures are allocated from PSRAM when available, freeing internal heap for the rest of the system. Boards without PSRAM continue to work; only the practical device ceiling differs (`GATEWAY_MAX_DEVICES` defaults to 10 on non-PSRAM builds)

## Bulk Import Progress Dialog
Importing a large device list now shows a progress dialog with phase labels, a percentage gauge, and a working **Cancel** button. Cancelling cleans up the server-side session so a new import can start immediately.

## Packet Visualizer — Mobile-Responsive Redesign
The Packet Visualizer previously required ~1100 px of horizontal space, making it unusable on phones and tablets. It has been fully redesigned for any screen width while preserving the protocol-analyzer layout on wide displays.

- **Semantic summary header per packet**: a compact row of color-coded chips (source → destination, line, hop count, packet-type extras) gives a scannable overview without expanding the byte strip
- **Byte strip collapsed by default**: each packet row can be expanded individually via a chevron toggle; **Expand All** / **Collapse All** toolbar buttons control all rows at once
- **Dark-mode colors corrected**: static Tailwind palette colors that stayed as bright pastels on dark backgrounds have been replaced with proper `dark:` variants throughout
- **Old V1 components removed**: the pre-redesign component files are gone; the new components carry the canonical names

## UI Polish
- **Toolbar buttons disabled during initial load** on the Smoke Detectors and Alarm Lines pages, preventing premature saves, adds, or deletes against an empty list that hasn't loaded yet
- **Disabled button appearance unified**: explicit content-color overrides that caused inconsistent icon colors on disabled buttons have been removed; DaisyUI handles disabled state uniformly now

## Per-Field Dirty-State Tracking

Every settings form now tracks unsaved changes at the field level. Edited fields are highlighted with a red left-border accent and show an inline revert button to restore just that field. The save button is disabled when nothing has changed, and collapsible cards show a dirty indicator in the header with a one-click revert-all button.

Applies to: MQTT, Home Assistant, NTP, Access Point, WiFi, Gateway Settings, Report Settings, and the Users dialog.

## Form Validation

All settings forms now share a single validation layer, replacing ad-hoc per-page implementations.

- **Shared validators** used consistently across NTP, Access Point, WiFi STA, MQTT, Home Assistant, and all Genius-specific dialogs
- **Single `FieldError` component** replaces ~22 copy-pasted inline error blocks; all field error text is now consistently styled
- **Reactive validation**: the save button disables the moment input becomes invalid, rather than only on submit
- **Fixed regex anchoring**: IPv4 and hostname patterns previously allowed substring matches (e.g. `192.168.4.1---//` passed as valid); both are now fully anchored
- **DNS 2 correctly optional**: DNS 2 in WiFi static IP config is only validated when non-empty, matching firmware behavior

## Migration Service
The per-service migration hooks from v1.3.0 have been consolidated into a central **Migration Service** ([docs](https://hmbacher.github.io/genius-gateway/setup/system/#migrations)). Migrations are declarative records with configurable failure policies (`retryNextBoot`, `skipAfterRetries`, `abortBoot`). Upgrade behavior for existing devices is unchanged.

- **System → Migrations page**: shows every migration with its current state (applied / pending / not applicable / failed), including entries from older firmware versions that are no longer registered. Failed migrations can be retried from this page

## PDF Export — Smoke Detector Report

A new **Generate PDF Report** button on the Smoke Detectors page generates a printable audit document with an overview page and a per-detector page for each registered device. A progress dialog shows generation steps; the PDF opens or downloads on completion.

## Build
- **Auto-generated version files gitignored**: `AppVersion.h` and `version.ts` are stamped on every build, making the working tree permanently dirty. Both files are now gitignored; a pre-build script writes a stub when they are absent
- **Prebuild scripts skip unchanged writes**: avoids triggering unnecessary rebuilds when output would be byte-identical
- **Stale bundles wiped before interface rebuild**: orphaned JS files from previous builds were silently embedded into `WWWData.h`, inflating firmware size by ~1 MB per orphan. The build script now clears the output directory before rebuilding

## Bugfixes
- **Startup WDT from AnalyticsService cascade**: after an MQTT reconnect, the analytics loop could fire on every tick and trigger a watchdog reset. Fixed by updating the publish timestamp at the correct point
- **HA discovery republished on every MQTT reconnect**: the interval guard was bypassed on reconnect, compounding the above cascade. Fixed alongside the AnalyticsService patch
- **Topbar status icons stale after reconnect**: MQTT, WiFi, and CC1101 icons could show outdated state following a reconnect; now consistently derived from live WebSocket status
- **IconSelect dropdown clipped by viewport**: the option list overflowed off the right edge on narrow screens; now aligned to the button's right edge
- **Report settings field length limits corrected**: name fields (80 chars) and address fields (200 chars) now have distinct limits enforced in both firmware and UI
- **Duplicate device IDs in saved config corrupted the device list**: duplicate entries passed the startup deduplication check and both landed in the live list. Duplicates are now detected and skipped; the file is rewritten immediately to purge the corrupt entry
- **Device list cards unkeyed in `{#each}`**: Svelte could reuse stale DOM nodes when the list changed, causing ghost-state visual artifacts. Fixed by keying on `device.id`
- **WiFi connection mode select styled inconsistently**: the select is now wrapped consistently with the rest of the page's input fields and carries a dirty marker

# v1.3.0
## Upgrade Notes
When upgrading from v1.2.x, four persisted-settings files are affected. All migrations run transparently on first boot — no manual reconfiguration is required:

- **`mqtt-settings.json` is split into two dedicated files and then removed.** The pre-v1.3.0 file mixed two unrelated concerns: Home Assistant integration and simple alarm publishing. On first boot, the gateway splits it into:
    - `/config/haSettings.json` — HA enable flag and discovery prefix migrate from `HAIntegrationEnabled` / `HAMQTTDiscoveryPrefix` into renamed keys (`enabled`, `discovery_prefix`), alongside three new fields (`device_name`, `manufacturer`, `model`).
    - `/config/alarm-publishing.json` — `alarmEnabled` and `alarmTopic` are copied across unchanged. The REST endpoint moves from `/rest/mqtt-settings` to `/rest/alarm-publishing`.

    Once both new files exist, the legacy `/config/mqtt-settings.json` is deleted. Migration is order-independent: whichever service finishes its migration last performs the cleanup.
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