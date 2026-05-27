# v1.4.0

## Frontend / Firmware Version Sync
- **Version mismatch banner**: after a firmware upgrade, a previously cached web UI may keep running against a backend whose REST and WebSocket contracts have moved on. Each build now stamps a combined `APP_VERSION_FULL = <semver>-<UTC YYYYMMDD.HHMMSS>` into both the frontend bundle (`interface/src/lib/version.ts`) and the firmware (`lib/framework/AppVersion.h`) via a new Vite plugin (`vite-plugin-app-version.ts`). The firmware announces its value on a new `app_version` WebSocket event emitted once per subscriber; if the running UI's baked-in value differs, an amber sticky banner appears at the top of the page with a **Reload** button. Suppressed during `vite dev` since mismatch is expected when the dev server runs against a flashed firmware
- **Static-asset cache control tightened**: the previous `Cache-Control: public, immutable, max-age=31536000` was applied to *every* embedded asset including `index.html`, which meant browsers could hold the old shell for up to a year after a firmware upgrade. Only content-hashed assets under `/_app/immutable/` keep the long-cache header now; everything else (notably `index.html`) is served with `Cache-Control: no-cache` so the shell is revalidated on every load
- **Reload button uses cache-busting query parameter**: for users upgrading *from* a pre-fix firmware whose `index.html` is still pinned as immutable in their browser cache, the banner's reload button appends `?v=<timestamp>` to force a fresh fetch of the shell. New hashed bundles are loaded as normal; old hashed bundles stay cached but are never re-referenced

## Device Details & Diagnostics
- **Radio Status badge realigned with the Hekatron Genius Home app**: in the Device Details dialog, the "Radio Status: OK / Fault" indicator is now derived from the FM module's own status bits — `FmFault` (bit 0) and `FmBatteryLowFault` (bit 3) of `radioStateMask` — matching what the vendor app surfaces. Previously the badge reflected the detector-reported `radioNetworkFault` flag from a separate byte, which could disagree with the per-bit flag list rendered below it. The list/table/card/MQTT views are unchanged (they already used `radioNetworkFault` only and never escalated `RemoteError`, matching vendor behaviour)
- **"FM Module Flags" subhead** added between the badge and the per-bit list, so the indented flags no longer read as if they composed the badge above
- **`RemoteBattLow` and `RemoteError` flags now styled as warnings** (amber triangle) instead of errors (red). These bits describe the state of *another* device on the radio line, not the inspected device, so they warrant attention but not a fault indication

## Smoke Detector List — Scaling to 50 Devices
The live device list now supports up to 50 smoke detectors on ESP32-S3 boards with PSRAM. Hitting that target required a layered memory strategy plus a new import transport so the existing single-POST path didn't run into the 16 KB body limit. The new **[Memory Considerations](https://hmbacher.github.io/genius-gateway/setup/memory/)** documentation page covers the full design.

- **Per-device REST endpoints replace the single bulk POST for routine CRUD**: `PUT /rest/gateway-devices/device` upserts one device, `POST /rest/gateway-devices/device/delete` removes one, `POST /rest/gateway-devices/reorder` reorders the list. Request bodies stay well below the PsychicHttp 16 KB cap regardless of total device count
- **Chunked import protocol for bulk operations**: a four-stage `begin → chunk × N → commit / abort` flow at `/rest/gateway-devices/import/*` lets arbitrarily large device lists land without holding the whole payload in heap. Single-slot session model with 60 s TTL expiry; client picks chunk size to fit the body budget (~6 KB ≈ 3 devices each at the current limits, so a 50-device import is ~17 chunks)
- **Commit deferred to a background task**: `_postCommitTask` runs the FS write plus the ~700 HA-discovery MQTT messages off the HTTP server task. The commit response now returns in milliseconds instead of blocking 15-20 s and starving the WebSocket keepalive
- **PSRAM strategy for the HA-framework working set**:
  - `lib/framework/PsramAllocator.h` provides a `std::allocator`-compatible template that prefers SPIRAM and falls back to the regular heap when PSRAM is absent. Applied explicitly to the live device list, the import staging buffer, and the upstream HA framework growth collections (`HAService::_subDevices`, `_publish/_unpublishCallbacks`, `HADevice::_entities`)
  - `custom_sdkconfig` sets `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=32` so any allocation > 32 bytes routes into PSRAM automatically (covers HA framework `String` content, vector backing stores, JSON documents). `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` pushes WiFi RX/TX and LWIP buffers (~50 KB) into PSRAM as well
  - `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` routes TLS session buffers (~30-50 KB working set) into PSRAM. Without this the GitHub release handshake fails with `MBEDTLS_ERR_SSL_ALLOC_FAILED (-32512)` once the internal heap is fragmented
  - CC1101 RX task stack forced to internal DRAM via `xTaskCreatePinnedToCoreWithCaps(... MALLOC_CAP_INTERNAL)` so the 100 Hz packet path is never penalised by PSRAM access latency
- **`GATEWAY_MAX_DEVICES` overridable per build env**: capped to **10** on the no-PSRAM `esp32-s3-devkitc-1` environment via a `-D GATEWAY_MAX_DEVICES=10` build flag. The codebase compiles and runs unchanged without PSRAM thanks to `PsramAllocator`'s `psramFound()`-gated fallback; only the practical device ceiling shifts
- **pioarduino #495 / #496 workarounds**: `build_unflags = -Wl,--wrap=log_printf` on PSRAM envs (custom_sdkconfig triggers a hybrid recompile in which the wrap symbol disappears); silent skip in `scripts/merge_bin.py` and `scripts/rename_fw.py` when `readFlag` returns `None` (outer SCons re-walks the build graph in a half-configured env and re-runs these post-actions). Both are documented in `platformio.ini` with tracking issue links

## Bulk Import Progress Dialog
The chunked-import flow takes 5-10 seconds for a 50-device file load and previously ran silently. A new progress dialog gives the user real feedback throughout, including a working cancel button.

- **`ProgressDialog` primitive** (`interface/src/lib/components/ProgressDialog.svelte`): prop-driven generic modal with the same visual idiom as `FirmwareUpdateDialog` — radial gauge when a percentage is known, indeterminate spinner otherwise, success / aborted / error icon states. Phase-specific actions (cancel, retry, success-close, auto-trigger countdown) configured purely via props. `FirmwareUpdateDialog` is deliberately untouched; the new primitive is available for any future long-running task
- **`DeviceImportDialog`** wraps `ProgressDialog` for the chunked-import flow specifically. Phases mapped: *starting* → indeterminate spinner, *uploading* → percentage gauge with "Chunk X of Y" step label, *committing* → 100% with "Finalizing" label, success / aborted / error → distinct terminal states with explicit Close (no auto-close — the user dismisses)
- **Real cancellation via `AbortController`**: the dialog owns the controller and threads its signal through `replaceAllDevices` into every fetch (`begin` / `chunk` / `commit`). Clicking Abort cancels the in-flight fetch with `AbortError`, short-circuits the chunk loop, and POSTs `/import/abort` with a fresh unbound fetch so the server-side session slot is freed even after the controller has fired
- **Aborted state has its own neutral visual** (muted `tabler/ban` icon on `bg-base-200`) so a deliberate user-cancel never reads as a celebratory success

## Packet Visualizer — Mobile-Responsive Redesign
The Packet Visualizer was desktop-only: a 37-byte Commissioning packet rendered as a single non-wrapping horizontal flex row needing ~1100 px of inline width, which made the page unusable on phones and tablets. The visualizer was rewritten as a viewport-aware component while preserving the protocol-analyzer feel on wide screens.

- **Semantic summary header per packet** (`GeniusPacketSummary.svelte`): a compact line of color-coded chips built from the already-decoded `generalInfo` / `specificInfo` — source location → destination location, line name, hop count, plus packet-type-specific extras (commissioning time, alarm origin / silencer location with the smoke-detector icon, discovery-response "to" radio chip). Wraps cleanly on narrow viewports and remains the scannable surface on mobile when the hex strip is collapsed
- **Byte strip collapsed by default, disclosed per packet**: every packet row has a chevron toggle. Per-packet expansion state is lifted to `+page.svelte` (`SvelteSet<number>` from `svelte/reactivity`, since `$state(new Set())` does not track `.add()` / `.delete()` mutations) so two new toolbar buttons — Expand All (`arrows-maximize`) and Collapse All (`arrows-minimize`) — drive every packet at once. The expanded view renders inside a contained `bg-base-300 rounded-box` panel with uniform 8 px inset for visual separation from the meta and summary rows above
- **Unlabeled filler ranges wrap byte-by-byte** via a new `GeniusPacketRawBytes` helper: semantic groups (Counter, Radio Module SN, Line ID, Hops, alarm origin SN, etc.) stay grouped through the existing `GeniusPacketDataBlock` grid, but filler ranges (bytes 3–9, 24–28, 35–37 in Commissioning, etc.) are emitted one block per byte so they wrap individually instead of an entire 6-byte field jumping to a new line as one unit
- **Card width reactive to expansion**: when every packet is collapsed the Genius Packets card sits at `max-w-2xl` to match the Vizualizer Settings card above; the moment any row is expanded it widens to `max-w-6xl` (the original byte-strip working size). Wide viewports keep visible margins on both sides in either state
- **Toolbar wraps in groups on narrow screens**: the three button clusters (Expand/Collapse · Copy/Clear · Load/Save) are wrapped in their own flex containers so a wrap break falls between groups rather than splitting a group mid-row. `SettingsCard`'s actions span was switched from `inline-flex shrink-0` to `flex flex-wrap justify-end` so this works for any page using the card's actions snippet; the packet visualizer drops the inline `divider divider-horizontal` separators to keep all wrap rows right-aligned
- **Dark-mode color sweep**: chips were using static Tailwind palette colors (`bg-cyan-200`, `bg-amber-400`, etc.) that don't switch with the DaisyUI `business` theme, leaving them as bright pastels with invisible light text on a near-black page. Every typed chip background gained a `dark:` variant — value/label rows go `bg-X-200 → dark:bg-X-900`, hex rows go `bg-X-400 → dark:bg-X-{600|700}` so the per-row contrast that's visible in light mode (e.g. Counter `stone-400` over `stone-300`) is mirrored in dark mode (`stone-600` over `stone-800`). A text sweep flips `text-slate-900 → dark:text-slate-100` on the meta-data, byte-block, and summary chip surfaces. The per-packet separator switches from `border-base-300/60` to `dark:border-base-content/20` so it stays visible against the dark background. Gated on a `[data-type]` attribute: `GeniusPacketDataBlock` now omits the attribute entirely for unlabeled fillers (previously emitted `data-type=""`), so the typed-chip text sweep applies only to coloured cells, while filler cells keep theme-aware default text on `bg-base-100`
- **V1 components removed**: the original `GeniusPacket.svelte` plus the seven `GeniusPacketContent*.svelte` files were kept alongside V2 during the redesign for side-by-side comparison behind a "Responsive view (preview)" toggle; with the new layout in place they were deleted and the V2 files renamed to canonical names (`GeniusPacket`, `GeniusPacketSummary`, `GeniusPacketRawBytes`, `GeniusPacketContent*`). The local-only preview toggle was removed from the Visualizer Settings card

## UI Polish
- **Toolbar action buttons disabled during initial list load** on both the **Smoke Detectors** and **Alarm Lines** pages. Previously the Add / Acoustic / Load / Save / Delete-all buttons were live from page mount — an early Save would download an empty backup, Add would stage against a pre-load empty list that the WebSocket refresh then overwrites. Each page now gates its toolbar on its own loaded flag; the Load `<label>` uses `class:btn-disabled` plus a `disabled` attribute on the hidden file input so keyboard activation through the `for=` attribute is blocked too
- **Disabled-state foreground unified across toolbar buttons** by dropping the redundant `text-primary-content` / `text-warning-content` / `text-error-content` classes. DaisyUI's `btn-primary` / `btn-warning` / `btn-error` already set the matching text colour when enabled and handle disabled-state foreground uniformly when not overridden. Previously the explicit content-colour classes left white icons on the muted-grey disabled background while the warning variant's dark icons stayed legible — an inconsistent strip where some buttons looked washed out

## Build
- **APP_VERSION_FULL frozen across SvelteKit's two-phase build**: the new version stamp is captured once on first emit and cached, so the second SvelteKit pass produces identical output and content hashes are stable
- **Enum and cert-bundle prebuild scripts skip unchanged writes**: prevents needless re-builds when the output would be byte-identical to what's already on disk
- **Stale Rollup bundles wiped before each interface rebuild** (`scripts/build_interface.py` now `rmtree(build_dir)` first): `@sveltejs/adapter-static` only cleans the files it emits, so orphaned `.js` from previous bundle-naming schemes were silently embedded into `WWWData.h` alongside the current bundle — inflating the firmware by ~1 MB per orphan. This fix is on the standalone `fix/build-interface-wipe` branch and will land separately

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