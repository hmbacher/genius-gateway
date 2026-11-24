# v1.6.0

## Home Assistant: Entity IDs Follow the Detector Location
Home Assistant builds an entity ID once, when an entity is first registered, and never changes it afterwards - renaming a detector's location in the gateway left IDs such as `sensor.hallway_genius_plus_x_alarm_line` behind. A new opt-in setting makes the entity IDs follow the location (#24).

- **New opt-in setting in the Home Assistant Integration card**: "Update entity IDs when a detector's location changes" (`rename_entity_ids` in `/rest/haSettings`, off by default). With the setting off nothing changes
- **Detectors are named after their location** in Home Assistant, and every detector gets explicit entity IDs such as `sensor.living_room_alarm_line`. Without a usable location the serial number is used instead (`sensor.genius_12345678_alarm_line`), so clearing a location frees the location-based IDs
- **Changing a location renames the entities of that detector**: the gateway clears the detector's retained discovery configs, waits a few seconds and publishes them again with an explicit `default_entity_id`. An edit that does not change the resulting IDs triggers nothing
- **Switching the setting on renames all detectors at once**, as a single batch
- **Missed renames are caught up**: the gateway persists per detector which IDs it last announced to Home Assistant. A rename that could not be sent because MQTT was down is carried out on the next connect, also after a restart
- **Alarm handling is never held up**: the rename runs in its own task. The web server, the packet receiver and the alarm path are not blocked while the gateway waits for Home Assistant
- **Web interface**: the detector editor warns before a location change renames entities; a progress dialog follows the three steps (remove, wait, publish), lists the detectors and ends with "Sent to Home Assistant", since Home Assistant does not acknowledge the rename; without an MQTT connection a dialog explains that the rename is caught up later (new `ha-entity-rename` WebSocket event)
- **Import**: an import that changes the location of known detectors asks first and lists the changes, as it renames their entities. The import dialog then follows the gateway through all steps - upload, saving on the gateway, updating Home Assistant - and ends with one result: devices imported, file migrated, changes sent to Home Assistant or still to be sent on the next MQTT connect. Before, it reported success as soon as the upload was accepted, while the gateway was still saving (new `device-import` WebSocket event). The entity-ID update is one of these steps and no longer a dialog of its own after an import. The dialog can be hidden once the list is handed over and comes back with the result if something needs attention
- **References to the old entity IDs** in automations, scripts and dashboards must be updated. The recorded history and the long-term statistics follow the new IDs. Requires Home Assistant 2025.10 or newer

See **[Home Assistant Integration](https://hmbacher.github.io/genius-gateway/setup/connections/#home-assistant-integration)** for details and limitations.

## Overview Cards Open the Clicked Detector
Clicking a detector card on the Overview page now opens that detector directly, on top of the Overview, instead of switching to the device list.

- **Device Details / Alarm Log on click**: normal cards open the read-only **Device Details** dialog of the clicked detector, alarming cards open its **Alarm Log**. A **Details** button in the Alarm Log leads on to Device Details. Acoustic readout and changing the alarm line stay on the Smoke Detectors page
- **Edit from the Overview**: administrators get an **Edit** button in the footer of Device Details that switches to the **Edit smoke detector** dialog. Saving runs the same follow-ups as on the Smoke Detectors page (fault warning, manual line entry for old FM modules, offer to add an unknown alarm line) and shows the same warning before a location change renames Home Assistant entities
- **Read-only for other users**: users without administrator rights can open Device Details and the Alarm Log, but are not offered the edit action (the old link bounced them straight back to the Overview)
- **Overview works without administrator rights**: the detector list can now be read by every signed-in user (`GET /rest/gateway-devices`), so the Overview shows the cards to them as well - before, it stayed at "Loading devices..." forever. Changing detectors and ending alarms remain administrator actions
- **One editor for all flows**: adding, editing and replacing a detector now go through one shared editor and save path (`deviceSave.svelte.ts`). A failed save no longer opens the follow-up dialogs

## Battery Warnings and Fault Status over Radio
Smoke detectors announce more than alarms over the air: the 36-byte packet the gateway already received for alarms turned out to be a generic **status change** - a pair of masks over the detector's status flags (newly set / newly cleared) plus its serial number. Alarm start/stop is just the alarm-flag case. The gateway now evaluates every flag, so a **low battery**, **device faults**, **radio link faults** and **dismounting** are picked up live (issue #23).

- **One current status per smoke detector**: for every status flag the gateway keeps whether it is active or OK, since when, and where it learned it - from a status change received over the air or from an acoustic readout. The newest information wins per flag. Smoke detectors announce a flag only when it changes, so a flag the gateway has not heard about is **Unknown** rather than OK, and the status survives a restart. The readout no longer keeps a second copy of battery, detector fault and radio fault that ages next to the current status
- **Activity log per smoke detector**: one chronology of what the gateway recorded - the detector was added, acoustic readouts, status changes over the air, status resets and alarms (the newest 50 entries). The **Alarm Log** dialog became the **Activity Log**, with a switch "Alarms only"
- **Reset Status**: a button in the Device Details dialog discards the reported faults of a detector. They become **Unknown**, not OK - the gateway cannot check that a fault is gone. The overview card and the device list then read *Status unknown* until the detector reports again or is read out, also if its last readout is still current (`POST /rest/gateway-devices/device/reset-status`). Replacing a smoke detector (its serial number changes) starts status, activity log and alarm counter over by itself
- **Four conditions in the web interface**, used alike by the overview cards, the device list and the top-bar health indicator: alarm, fault, OK, unknown. Yellow now only means that a detector reports a fault; a detector without a current readout is grey instead of yellow. OK needs a current readout
- **Overview cards name the fault**: a status line shows one icon per fault and the fault in text, for a single fault with its date. A fault muted on the detector, chamber drift, a negative dirt forecast and warranty flags are hints and do not change the colour
- **Activity log for every detector**: the log button in the device list is always there, not only for detectors with alarms; without alarms the log opens unfiltered
- **Notification**: a toast names the detector and the fault whenever a detector reports a new fault (new `device-status` WebSocket event)
- **PDF report**: each device page carries the same *Current Status* block as the Device Details dialog - state, date and source per flag, *Unknown* for what was never reported. What only a readout reports is listed as *Readout Findings*. The alarm log at the end of a device page is now the full activity log, with the same entries and wording as in the dialog. In the overview table an alarm or a fault counts even for a detector without a readout
- **Home Assistant**: the existing **Battery**, **Smoke Detector State** and **Radio Module State** entities now show the current status and update live; a new **Dismount** entity (`tamper`) is added. Each of them is unavailable until its state is known instead of suggesting an unconfirmed OK
- **Simple fault publishing**: optional second topic (default `smarthome/genius-gateway/fault`) with the number of faulty and battery-low detectors and the faults per detector, configured alongside simple alarm publishing. Switching it off removes the retained message
- **Packets are classified by message family and type**: the type byte repeats across the message families, so the daily radio link supervision series of FM Pro X modules was shown as a CommissioningProbe request. Packets of a family the gateway does not know are shown as unknown
- **Packet visualizer**: status changes are decoded flag by flag - labelled *Start Alarm* / *Stop Alarm* for the alarm flag and *Status Change* otherwise
- **Device configuration file version 3**: the alarms moved into the activity log (`events`), `alarmCount` counts them, `status` holds the state per flag. Older files and exports are converted when they are loaded or imported. **Clients of `GET /rest/gateway-devices` that read the `alarms` list have to switch to `events`**

## Radio Link Supervision (FM Pro X)
FM Pro X radio modules with the DIP switch *radio link supervision* check their line once a day and report a module they no longer hear. The gateway now understands this: it shows which detector is missing, hours before the detectors start to beep, and when each supervised module was last heard. Contributed by danielrdt (#27), who captured and analysed the telegrams; rebuilt here on the status model above.

- **Reported missing**: a detector whose radio module is named in a "module missing" report gets this status. It counts as a fault on the overview, in Home Assistant (*Radio Module State*) and on the fault topic. The reporting detector gets an entry in its activity log; while the module stays missing the report comes again every four hours, which is counted on that entry (*3 times, last ...*) instead of filling the log. The radio link fault the reporting detector announces after the third failed check is the status flag already handled above
- **Ends when the module is heard again or confirmed present**: the status ends with the next packet of that module, with the answer of another module that still hears it (*Module Present* - the module is only out of range of the one that reported it), or with *Reset Status*. All three are in the activity log. Ending it does not make the radio module OK: *Radio Module State* in Home Assistant goes back to what the radio link itself reported, and stays unavailable if that was never reported
- **Last seen and signal**: for detectors with an FM Pro X module the device list and the Device Details dialog show when the module was last heard, the dialog also how strong. The values are kept in memory only: after a restart a module is *not heard* until its next packet (`GET /rest/gateway-devices/runtime`, new `device-runtime` WebSocket event)
- **Home Assistant**: two more diagnostic entities for detectors with an FM Pro X module, *Last Seen* and *Signal Strength*, on a state topic of their own. They are unavailable until the module was heard since the gateway started
- **Packet visualizer**: the telegrams of the supervision are named - *Supervision Series*, *Supervision Roll Call*, *Module Missing* and *Module Present* with the module they name, and the *Line Clock* with its time and day counter. The series was shown as a CommissioningProbe Request before - both have the same type byte and length
- **Documentation**: the protocol analysis describes the telegrams with their byte layout and captured examples, explains how the supervision works, and says how far each statement is verified by the manufacturer's product information and by captures. The two telegrams of family `0x11` that accompany a supervision round are only partly understood
- **API**: an activity log entry can carry `count` and `last` - a report that was repeated while its cause lasted
- **Not yet seen on a line with more than two modules**: the captures come from the contributor's line with two FM Pro X modules. *Module Present* has not been captured yet. A test of this version on a supervised line is pending. FM Basis X modules do not supervise their line, so nothing changes for installations without FM Pro X

## Line Test and Network Test Told Apart
The packet that starts a line test also starts a network test. The byte that was taken for a start/stop flag is a test mode: `0x04` starts a line test (about 15 minutes), `0x06` a network test (about 30 minutes), `0x00` stops either. The assignment follows from the durations of the two tests, 15 and 30 minutes, which are the ones the manufacturer states; it is not yet confirmed on air which of the two values is the line test.

- **Start Line Test now starts a line test**: the action of an alarm line - in the web interface, over the REST API and as the Home Assistant button - sends test mode `0x04`. Up to v1.5.0 it sent `0x06`, which by this assignment starts a network test of twice the duration
- **Both tests are recognised and named**: the gateway and the packet visualizer tell *Start Line Test*, *Start Network Test* and *Stop Line / Network Test* apart, each by its exact test mode value. A line test start with test mode `0x04`, as smoke detectors send it, was not named by the packet visualizer before; a test packet with an unknown mode is shown as unknown
- **Automatically added alarm lines are named after their ID**: `<ID> (automatically added)` instead of a sentence about the packet they were learned from - the name shows up wherever the line does, e.g. in the packet visualizer. Lines are also learned from a network test start. Lines that already exist keep their name
- **Documentation**: the protocol analysis describes the test mode byte, with the evidence and a captured line test start

## Home Assistant MQTT Topic Restructuring
The Home Assistant topic tree carried a redundant namespace level and derived its identity from the user-editable **Device Name**, which duplicated the gateway's MAC address in every state topic and made renaming the gateway a destructive operation. Both are fixed.

- **Redundant namespace level removed**: base topics collapse from `{prefix}{namespace}/{device_id}/…` to `{prefix}{device_id}/…`. The namespace was always a literal string prefix of the device ID that followed it, so it carried no information - nothing subscribed to it and Home Assistant never saw it, since discovery payloads always name their topics explicitly

    ```
    v1.5.0  homeassistant/genius-gateway-1a2b3c4d5e6f/genius-gateway-1a2b3c4d5e6f-1a2b3c4d5e6f/genius-123/diagnostics/state
    v1.6.0  homeassistant/genius-gateway-1a2b3c4d5e6f/genius-123/diagnostics/state
    ```

- **MAC address no longer duplicated**: the device ID was built as `slugify(deviceName) + "-" + MAC`, while the factory-default device name already ended in the MAC - producing `genius-gateway-<mac>-<mac>`. Topic identity is now derived from the compile-time firmware name, so the MAC appears exactly once
- **Renaming the gateway is now safe**: the **Device Name** is purely cosmetic and feeds only Home Assistant's display label. Previously it drove the topic namespace, the discovery `node_id`, and `device.identifiers`, so a rename silently re-registered the gateway as a *new* device in Home Assistant and abandoned the old one - along with all its retained discovery configs - as an unremovable ghost
- **Sub-devices no longer go stale on rename**: a sub-device captured its topic namespace once, when it was created, and never re-synced. After a rename the gateway published under the new tree while every smoke detector kept publishing under the old one. With a stable identity this cannot happen
- **Automatic cleanup of the old topic tree**: on the first MQTT connect after the upgrade, the gateway clears every retained discovery config and state message it previously published under the old paths, retiring each old config immediately before publishing its replacement so Home Assistant reclaims the entity ID instead of suffixing the new entity with `_2`. Smoke detectors and alarm lines are untouched by the migration - their topics never contained the device name and their `unique_id` is unchanged, so Home Assistant updates them in place and their history and automations survive

See **[Home Assistant Integration](https://hmbacher.github.io/genius-gateway/setup/connections/#home-assistant-integration)** for upgrade details and the two things worth verifying afterwards.

## Documentation
- **MQTT topic reference corrected**: the documented discovery config topic pattern (`{discovery_prefix}{component}/{topicNamespace}/{gateway_device_id}/…/config`) never matched the firmware, which has always published to a flat single-level `node_id` as the Home Assistant discovery spec requires. All topic examples now reflect the actual v1.6.0 layout
- **Status Change packet**: the protocol analysis describes the former *Alarming* packet as the generic status change, with the full flag table and a captured battery warning

## Bugfixes
- **Restart when a smoke detector or alarm line was added or removed while the gateway published to Home Assistant**: after every MQTT connect, and after applying the Home Assistant settings, the gateway republishes all entities in a background task - a few seconds with many detectors. Adding or deleting a detector or an alarm line during that time changed the lists that task was working on and could crash the gateway (present since v1.3.0). The Home Assistant registry is now locked per device while it is published: a change waits for the device in progress instead of pulling it away
- **Web interface held up for seconds after applying the Home Assistant settings**: the gateway republished every entity before it answered the request, so **Apply Settings** showed no reaction for about five seconds with a dozen detectors. The republish now runs in the background and the gateway answers at once. The button shows a spinner and is locked while a request is under way - switching the integration off still removes all entities before the gateway answers
- **Radio packets could be missed while a browser did not respond**: the gateway sent web interface events (alarm state, packet log, system values) straight from the task that emitted them and waited up to 60 seconds for every client that did not take the data - for example a browser tab on a phone in standby. The alarm state was sent by the task that receives the radio packets, so during that wait further packets were lost: in a test the all-clear of an alarm was never processed and the alarm event reached other browsers 56 seconds late. The first alarm message to MQTT / Home Assistant was not affected. A client now gets one second to take an event and is disconnected if it does not (browsers reconnect by themselves), and the alarm state is sent by a task of its own
- **Alarms reached Home Assistant up to ~15 seconds late after a detector was saved**: MQTT messages that the gateway queues (states and diagnostics) left at only one message per second. Saving a detector republishes the diagnostics of all detectors, so an alarm received right after it had to wait behind them - about one second per detector. The gateway itself always registered the alarm at once; only the way to MQTT was held up. The queue is now drained at up to 50 messages per second. Applies to the `gg-v1_0`, `seeed-xiao-esp32s3` and `esp32-s3-devkitc-1-n8r2` builds
- **Radio module status and DIP switches of an acoustic readout were shown under the wrong names**: two bit fields of the readout were decoded in the wrong bit order - the radio state flags (readout byte 20) were mirrored and the DIP switches (byte 30) misplaced, with different mappings in the readout parser, the Device Details dialog and the PDF report. A set **Radio Link Supervision** switch, for example, appeared as *Send Collective Alarm*, and a **Remote Alarm** flag as *FM Fault*. **Radio Status: Fault** in Device Details and in the PDF report was derived from the wrong two bits as well. The bit order now follows the Hekatron apps (Genius Control and Genius Home), from one shared definition for parser, dialog and report
    - Only readouts of radio modules that report one of these bits are affected; modules that report `0x00` in both fields (as an FM Basis X without set flags does) were always shown correctly
    - Stored readouts are corrected without a new readout: the gateway stores the two bytes unchanged, only their interpretation was wrong
    - The [acoustic readout documentation](https://hmbacher.github.io/genius-gateway/reverse-engineering/acoustic-readout/) lists the corrected bit tables

# v1.5.0

## Direct-Link Signal / Range Survey
Measure how well each radio module is heard **directly** by the gateway - bypassing the mesh - to help place devices and diagnose weak links. The gateway sweeps the network with a **ConfigCheckProbe** direct-range reachability probe and records the best signal it hears back from each module.

- **Range test from the device list and alarm-line editor**: broadcasts the ConfigCheckProbe and collects the direct responses; each responding module's best **RSSI** and the **time of measurement** are stored per device (`rssi` / `lastRangeTest`)
- **Signal indicator on the smoke-detector list**: a signal-strength icon shows each device's last measured direct-link quality at a glance
- **Progress and result dialogs**: live progress while probing, then a summary of which modules answered and how strongly
- **New "SignalProbe" alarm-line acquisition source**: a device's alarm line can be acquired/confirmed from a direct-range survey response, alongside the existing sources
- **PDF report "Direct Link (Signal / Range Test)" section**: exported reports now include a per-device signal/range-test section with the measured RSSI and timestamp

## Robust Packet Classification by Message-Type Byte
Packets are now identified by the radio module's on-air **message-type byte** (offset 27), with packet length used only as a validator - replacing the previous length-only heuristic.

- **Fixes a misclassification**: a 36-byte frame can be either an **Alarm** or a **ConfigCheckProbe response**, so a ConfigCheckProbe response from a group-A / line-1 detector could previously be misread as **ALARM_STOP**. Keying on the message type eliminates that false read
- **Recognizes the ConfigCheckProbe family**: ConfigCheckProbe request/response packets are now decoded (message type, group/line, status) in the backend and the packet visualizer

## Packet Visualizer
- **Header field `Counter` renamed to `Remaining TX Time`**: reverse-engineering the radio-module firmware confirmed that bytes 2-3 are not a packet counter but the **remaining transmission time** - a countdown of the packet's repetition window. The visualizer label and the underlying field naming now reflect that

## Modal Dialog Title Icons
Every modal dialog now shows an icon beside its title, so add/edit/replace and info dialogs are recognizable at a glance and visually consistent across the app.

- **Title icons on all modal dialogs**: add/edit/replace flows (smoke detectors, alarm lines, users, Wi-Fi networks) show a context icon supplied by the opener, and single-purpose dialogs (device details, alarm log, wiring test, manual line entry, network scan, alarm ending) carry their own icon

## Alarm-Line Commissioning Trigger
Alarm lines can now kick off the wireless commissioning procedure (*Funk-Inbetriebnahme*) directly from the gateway, without needing to hold a physical radio-module button.

- **New "Start Commissioning" alarm-line action**: broadcasts the Alarm-Line Commissioning packet (`0x03`) for the selected line - functionally identical to mounting a smoke detector whose radio-module button was held for several seconds. Every detector already on that line enters ~15 minutes of commissioning mode (triple-tone, green blink every 8 s), during which detectors can be added or re-enrolled
- **No separate stop action**: commissioning mode is a receiver-side timer with no corresponding stop frame in the protocol, so it always runs its full ~15-minute course; the action is unavailable for the *Broadcast* line
- **Home Assistant button**: each alarm line's HA sub-device now exposes a fifth button, "Start Commissioning", alongside the existing line-test/fire-alarm controls
- **Alarm Lines page: new "Line Actions" column**: commissioning, line test, and fire alarm actions now live in their own column, grouped with subtle dividers and separated from "Manage" (edit/delete)

## Bugfixes
- **"Revert change" tooltip no longer hides behind the dirty-field accent bar**: on the SPI Pin Configuration selectors (and other dirty-tracked fields), the revert tooltip rendered underneath the red dirty accent bar - daisyUI draws tooltips at `z-index: 2` while the accent bar sits at `z-10`. The revert button now establishes its own stacking context above the bar so the tooltip is fully visible

# v1.4.1

## Manual Alarm-Line Entry for Old FM Modules
Old-generation radio modules (**FM.Basis** / **FM.Pro**) cannot expose a trustworthy alarm line: they transmit `lineId=0` over SmartSonic and the line byte on the wire is unreliable. The Genius Gateway now mirrors the official Hekatron app's behavior for these modules instead of silently trusting bogus readout data.

- **Manual line entry required for old modules**: acoustic readout and radio-packet data for FM.Basis/FM.Pro modules no longer populate the alarm line automatically. Devices with an old module and no line yet are flagged **"Line required"** in the smoke-detector list and details dialog, with a one-click path into the new rotary-switch entry UI (major `A`–`H` / minor `0`–`9`, with `H` disabled for FM.Basis since Sammelalarm is Pro-only)
- **Device details dialog adapted for old modules**: radio status, DIP switches, and interference are hidden for FM.Basis/FM.Pro (these modules never report them); the alarm line section shows the manually-entered rotary line or a "Set Alarm Line manually" action instead
- **Rotary line follows the physical module on swap/replace**: when a radio module is matched by serial number across a readout/replace flow, its manually-entered line is carried over so installers don't have to re-enter it for the same hardware
- **Config migration v1 → v2**: existing configs with an old-module device that has a (untrustworthy) line carried over from previous firmware are migrated - the stale `lineId`/`lineCharacter`/`lineNumber` are purged unless they were already flagged as manually entered
- **"Smoke Detectors" column hidden when no compatible hardware is installed**: the Alarm Lines page's smoke-detector-assignment column (desktop table and mobile cards) is only shown when at least one device has a radio module capable of automatic line detection (FM.MCP, FM.Basis X, FM.Pro X) - installations with only old FM.Basis/FM.Pro hardware no longer see a column that's permanently empty
- **New `InfoPopover` component**: a click-to-open, rich-HTML tooltip (supports multi-line text, links, etc.) styled to match daisyUI's tooltip look - used to explain the smoke-detector-assignment restriction inline next to the column header / "No devices" state

## Bugfixes
- **Packet visualizer byte view now highlights serial numbers/line/hops for unrecognized packet types too**: the summary row above the byte view already extracted these fields for any packet (known or not), but the byte view itself fell back to a plain, untyped byte dump for unrecognized types - an inconsistency between what was described and what was shown. The common 24-byte header (counter, both radio-module serials, line, hops, sequence number) is now rendered with the same typed/colored blocks regardless of packet type
- **Device export always includes `radioInterference` and `lineId`**: both fields were previously omitted when zero. A genuine `radioInterference` reading of 0.0% would silently disappear from the export, indistinguishable from a device that had never been read out. Similarly, a `lineId` of 0 (unassigned) would be absent. Both fields are now always written so consumers can rely on their presence
- **SPI Pin Configuration dropdowns no longer clipped at card boundary**: the lower pin selectors in the SPI Pin Configuration dialog were cut off by the settings card's overflow boundary. The card now uses `overflow-x: clip` / `overflow-y: visible` so dropdown lists extend below the card as expected
- **Update indicator no longer shows incompatible releases**: the topbar firmware-update badge now only lights up when a newer release includes an asset that matches the current build target. Previously, any newer release triggered the indicator regardless of hardware compatibility, and clicking it immediately offered to install a binary that would fail or brick the device
- **GitHub OTA download no longer sends an empty URL to the device**: when no compatible binary was found for the build target, the backend returned an empty `download_url`. The device then attempted an OTA connection to an empty hostname, failing with a DNS error. The `update_available` flag is now only set when a matching binary exists, and the UI adds a defensive guard as a belt-and-suspenders check
- **Re-selecting the same firmware file now re-triggers the upload**: after clicking Abort on the confirmation dialog, the file input was not cleared. Because the browser suppresses `change` events when the same file is picked again, the upload could not be restarted without first picking a different file. The input is now reset on Abort so any file - including the same one - opens a fresh confirmation
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

## CC1101 - Runtime SPI/GDO Pin Configuration

The SPI and GDO signal pins for the CC1101 transceiver can now be changed at runtime via **System → CC1101 → Pin Configuration** - no reflashing required. The UI shows only valid GPIOs for the target board and warns about conflicts with pins claimed by other roles. Changes take effect immediately without a reboot. Existing installs without a saved config continue to use the compile-time defaults.

## Smoke Detector List - Scaling to 50 Devices
Up to 50 smoke detectors are now supported on ESP32-S3 boards with PSRAM. See the new **[Memory Considerations](https://hmbacher.github.io/genius-gateway/setup/memory/)** documentation page for details.

- **Per-device REST endpoints**: add, update, delete, and reorder operations now target individual devices, avoiding the payload size limits that the previous bulk-POST approach hit at higher device counts
- **Chunked import for bulk operations**: large device lists are uploaded in small chunks and committed in a background task, keeping the UI responsive and eliminating the 15–20 s blocking commit that previously starved the WebSocket keepalive
- **PSRAM used for large collections**: the device list, import buffer, and HA framework data structures are allocated from PSRAM when available, freeing internal heap for the rest of the system. Boards without PSRAM continue to work; only the practical device ceiling differs (`GATEWAY_MAX_DEVICES` defaults to 10 on non-PSRAM builds)

## Bulk Import Progress Dialog
Importing a large device list now shows a progress dialog with phase labels, a percentage gauge, and a working **Cancel** button. Cancelling cleans up the server-side session so a new import can start immediately.

## Packet Visualizer - Mobile-Responsive Redesign
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

## PDF Export - Smoke Detector Report

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
When upgrading from v1.2.x, four persisted-settings files are affected. All migrations run transparently on first boot - no manual reconfiguration is required:

- **`mqtt-settings.json` is split into two dedicated files and then removed.** The pre-v1.3.0 file mixed two unrelated concerns: Home Assistant integration and simple alarm publishing. On first boot, the gateway splits it into:
    - `/config/haSettings.json` - HA enable flag and discovery prefix migrate from `HAIntegrationEnabled` / `HAMQTTDiscoveryPrefix` into renamed keys (`enabled`, `discovery_prefix`), alongside three new fields (`device_name`, `manufacturer`, `model`).
    - `/config/alarm-publishing.json` - `alarmEnabled` and `alarmTopic` are copied across unchanged. The REST endpoint moves from `/rest/mqtt-settings` to `/rest/alarm-publishing`.

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
- Stable namespaced MQTT discovery topics - topic stays constant even when the device is renamed

### Home Assistant - General Improvements
- Gateway hostname and HA device name now include the unique device ID suffix by default, preventing collisions in multi-gateway setups
- Restart button correctly registered as a **Control** entity (not Diagnostic)

### Alarm Line Management UI
- **Live spinners reflect external trigger sources**: when a line test or fire alarm is triggered from Home Assistant or via MQTT, any open Web UI now shows the spinner on the corresponding alarm line for the duration of the transmission, instead of staying idle. Backed by a new `alarm-line-action-started` WebSocket event carrying `{lineId, action}`, emitted alongside the existing `alarm-line-action-finished` event

### Smoke Detector Management UI
- **Delete all detectors** button (trash-x icon) in the toolbar: removes all smoke detectors in one step after a confirmation dialog; disabled when no devices are configured
- **Import migration**: configuration files from older backup formats (v0) are automatically migrated to the current format (v1) on import, with a success notification and prompt to re-export; only files with a newer-than-current version are blocked
- **Alarm state handling on import**: if any device in an import file is marked as alarming, a dialog now asks whether to keep or clear the alarm state before sending to the backend
  - **Keep Alarm State**: imports as-is; connected integrations (e.g. Home Assistant) may trigger automations - useful for testing purposes
  - **Clear Alarm State**: resets `isAlarming` on all affected devices and closes any open alarm log entry with a new `ByImport` ending reason and the current timestamp
- **Alarming devices visually highlighted in the device list**: rows with `isAlarming === true` are rendered with the daisyUI `error` theme (`bg-error` + `text-error-content`) on both mobile cards and desktop rows, with a filled flame icon next to the location name to disambiguate "active alarm" from a generic error state; all inner status icons (OK, fault, readout age) inherit `text-current` on alarming rows so they remain legible on the red background
- **Top-bar health indicator** reworked with four states and is a link to the overview (`/`):
  - **Semitransparent heart** - no smoke detectors configured yet
  - **Green heart** - all detectors healthy (readout present and recent, no faults)
  - **Yellow `heart-exclamation`** - at least one detector has no readout, a stale readout (> 1 year), or any fault
  - **Red hexagon** - at least one detector is actively alarming
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
- Dead `bind:this` / `formField` binding removed from all settings forms - it was never read; inline `e.preventDefault()` replaces the `preventDefault()` wrapper function in all form `onsubmit` handlers
- `$state()` on typed settings objects now supplies explicit default values, eliminating svelte-check errors and undefined reads before the first REST fetch completes

## Bugfixes
- Fixed `AcousticDetectionDialog` crashing immediately on open with `this.callbacks.onLog is not a function` - the `onLog` no-op callback was missing from the `AcousticDetectionSession` constructor call
- Fixed alarm line with reserved ID `0xFFFFFFFF` (broadcast) being stored and published to HA when a readout returned that line ID: frontend `checkAndOfferAlarmLine` now guards against both `0x00000000` (unassigned) and `0xFFFFFFFF` (broadcast); backend `AlarmLines::update` silently skips both reserved IDs so a crafted PUT cannot persist them into `_state.lines`
- Fixed UTC timestamp parsing: `iso8601_to_time_t` was calling `mktime()` - which interprets a broken-down struct as local time - on what is actually a UTC struct, shifting all reconstructed dates by the device's UTC offset; replaced with a direct epoch calculation using the Hinnant civil-days O(1) formula (no platform `timegm()` dependency)
- Fixed device registration type being silently downgraded to `Manual`/`Packet` on plain edits (name, location change): the registration field is now only updated when the incoming PUT includes a `readoutTime`, preventing a frontend that omits the field from overwriting the stored registration type
- Fixed `HAGroupedSensorPublisher` callback leak: `HAService::onPublishAll` / `onUnpublishAll` now return a `CallbackId`; the publisher stores both IDs and removes them from `HAService` in its destructor, preventing stale lambda closures from accumulating in the callback vectors across repeated add/remove cycles (e.g. clear + import test runs). The `_alive` guard is kept as a belt-and-suspenders safety net for any in-flight invocation that races the removal
- Fixed HA device not removed from Home Assistant when HA integration is disabled: `HAGroupedSensorPublisher` and `HAGroupedSwitchPublisher` entities (gateway diagnostics, configuration switches) were not unpublished because there was no unpublish callback mechanism. Added `HAService::onUnpublishAll()` parallel to `onPublishAll()`; both publishers register an unpublish callback in `begin()`. Added `unpublishAll()` to `HAGroupedSwitchPublisher`. `HASettingsService` now calls `unpublishAll()` before `setEnabled(false)` so MQTT is still connected when the empty retained payloads are sent
- Fixed CC1101 initial reset sequence, improving startup reliability in some hardware setups
- Fixed HA discovery for dynamically added smoke detectors: `publishAll()` is now called so all diagnostic entities are announced immediately, not only on the next periodic publish cycle
- Fixed HA sub-device map being keyed on serial number instead of stable `device.id`, causing stale entries after device renames
- Fixed `iso8601_to_time_t` to accept ISO 8601 timestamps without a milliseconds component (e.g. from certain gateway exports)
- Fixed `radioInterference` being stored as a negative value when the radio module reports no signal; values are now clamped to ≥ 0
- Fixed simple alarm MQTT topic not being republished when `isAlarming` changes via REST (e.g. after importing alarming devices). Cached alarm state (`_isAlarming`/`_numAlarming`) is now derived from the device list inside `_updateAlarmingState()`, which detects any change and publishes - independent of the `ALARM_STATE_CHANGE` event that the previous gating relied on
- Fixed alarm log dialog showing "No data" for the End column on alarms ended by import: the end-time gate was hardcoded to `endingReason === 0 || === 1` and didn't recognize the new `ByImport` reason. The check now uses `endingReason !== AlarmActive`, and the ending-reason column shows a dedicated import icon for `ByImport`

# v1.2.1
## Bugfixes
- Fixed initial CC1101 reset routine, that caused CC1101 initialization to fail in some setups

# v1.2.0
## Features
- **Build-target-aware GitHub Firmware Manager in web interface**
  - GitHub releases are filtered client-side by build target - only `.bin` assets whose filename contains the device's build target (e.g. `seeed-xiao-esp32s3`) are considered compatible
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