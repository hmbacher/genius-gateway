# PR #24 rework - instructions

> **Temporary file.** It exists only so the rework can be finished in a later
> session without the original context. Delete it in the last commit before
> this branch is merged to `main`.

## 1. Status

- Branch: `feat/ha-rename-entity-ids` (integration branch, not `main`).
- Content, oldest first:
  1. `dccdd80` - tip of `main` when the branch was created
  2. `20d39d5` - `fix(ha)`: derive MQTT topic identity from firmware name (not on `main` yet)
  3. `97582ce` - `fix(build)`: never keep a stub `WWWData.h`
  4. `71eebe1` - PR #24 by an external contributor, squash-merged **into this branch**
- PR #24 is closed as merged. A comment on the PR lists the changes the
  maintainer announced; treat them as commitments:
  https://github.com/hmbacher/genius-gateway/pull/24#issuecomment-6928167390
- That comment asks the contributor one question (history loss, see 4.5).
  **Check the PR thread for an answer before starting.**
- Nothing below is implemented yet. The branch is pushed up to `71eebe1`;
  this file is committed locally only.
- Commits `20d39d5` and `97582ce` also sit at the bottom of
  `feat/rf-device-status` (which is untested and adds a v1.6.0 changelog
  section). Whichever branch reaches `main` first, the other one needs a
  rebase.

## 2. What the feature is for

Home Assistant builds an entity ID once, when an entity is first registered,
and never changes it afterwards. Current HA derives it from area + device
name + entity name. The gateway publishes each smoke detector as an HA device
whose name is the model name and whose `suggested_area` is the detector's
location, so an ID looks like `sensor.<location>_genius_plus_x_alarm_line`.
When the user later renames the location in the gateway, the IDs keep the old
location.

PR #24 adds an opt-in setting (`rename_entity_ids` in `/rest/haSettings`,
default off). With it the detector's HA device is named after the location,
every entity gets an explicit `default_entity_id`, and on a location change
the entities are removed from HA and published again so HA adopts the new ID.

## 3. Facts established during the review

Read in the HA core source (`dev` branch, 2026-11-16), not tested live:

- `helpers/entity_registry.py`, `_async_generate_entity_id`: the default ID
  parts are area, parent device, device, entity.
- `helpers/entity_registry.py`, `async_get_or_create`: when an entity with the
  same unique_id was deleted before, HA restores its old entity_id if it is
  still free.
- `components/mqtt/entity.py`, `_init_entity_registry` and
  `async_added_to_hass`: if a deleted registry entry exists and its entity_id
  differs from the `default_entity_id` in the new config, HA renames the
  restored entry to the new ID (`_update_registry_entity_id`). This is what
  makes delete + recreate work at all.
- `components/mqtt/discovery.py`: HA queues discovery messages per discovery
  topic. A config that arrives while the removal of the same topic is still
  running is processed after the removal has finished.
- MQTT discovery has no acknowledgement. The gateway cannot know when, or
  whether, HA has processed a removal.
- An existing entity ID cannot be changed over MQTT in any other way than
  delete + recreate.

Read in Zigbee2MQTT (`lib/extension/homeassistant.ts`, `onEntityRenamed`):

- Same approach: clear the retained configs, `sleep(2)`, discover again. The
  comment points to Z2M issue #12610 (HA created a `_2` entity without the
  pause).
- Z2M clears with QoS 1. This branch clears with QoS 0.
- Z2M always sends `default_entity_id` ("as of HA 2025.10 this replaces
  `object_id`", HA core PR 151775).

Not verified:

- Whether the recorder history follows the rename. A registry rename normally
  migrates it, which would contradict the "history is lost" warnings in this
  branch (see 4.5).

## 4. Problems found in the merged code

Line numbers are as of `71eebe1`.

### 4.1 Three-second delay under the service mutex (blocker)

`GeniusDevicesService::_syncSmokeDetectorSubDevices()`
(`src/GeniusDevicesService.cpp:1393`) calls `vTaskDelay(3000)` at line 1436
while holding the StatefulService mutex (`beginTransaction()`).

- `setAlarm()` in the RX path takes the same mutex, so alarm handling stalls
  for 3 s per renamed detector.
- The function runs in the HTTP task when a device is saved over REST (update
  handler at line 268), so the web server blocks as well.
- It is also reached from the packet RX path:
  `GeniusGateway.cpp:780` calls `mqttPublishAllDevices()` when a device is
  added from a packet.
- The existing code sleeps 1 ms under that mutex, not 3000.

### 4.2 Rename is lost while MQTT is disconnected

If `_haService->isReady()` is false when the location changes, the delete
step is skipped, the name is updated and `device.published` is set to true.
After the reconnect the new configs arrive without a preceding delete, and HA
keeps the old IDs. Nothing retries.

### 4.3 Toggling the option has no immediate effect

`HASettingsService::_applyToHAService()` only stores the flag and calls
`_haService->publishAll()`. Sub-device names are not recomputed. Device names
change after a reboot or the next edit of that detector; entity IDs of
existing detectors change only when their location is edited.

### 4.4 Clearing the location runs a useless cycle

With an empty location `setEntityIdFromName(false)` is set, so no
`default_entity_id` is sent. The entities are deleted and recreated, and HA
restores the old location-based ID.

### 4.5 "History is lost" may be wrong

See section 3. The claim appears in the UI hint (`HAConfig.svelte`), in
`docs/setup/connections.md` (warning admonition around line 153),
`CHANGELOG.md` (around line 57) and `docs/api/http-api.md:241`.

### 4.6 Slug

`haSlugify()` (`lib/framework/HomeAssistant/HADevice.cpp:36`) turns "Küche"
into `kueche`; HA itself would produce `kuche`. Other non-ASCII characters
become `_`. It is a second slug function next to `HAService::_slugify`
(`HAService.cpp:500`, dash-separated).

## 5. Decisions

### Confirmed by the maintainer

- The rework is done by the maintainer on this branch, not by the contributor.
- A rename that could not be sent is caught up later (reconnect or reboot).
- Applying the HA settings with the option newly switched on renames **all**
  detectors at once.
- While the option is on, every detector gets an explicit entity ID. Without a
  location the ID prefix falls back to the **smoke detector serial number**.
- Edit dialog: when the option is on and the location of an existing detector
  is changed, a warning slides in (same animation as the other warnings).
- When a rename runs, a small progress modal shows the three steps (remove,
  wait, publish), consistent with the other progress displays.

### Proposed in the session, not objected to, not explicitly confirmed

- The modal's final state reads "sent to Home Assistant", not "renamed",
  because HA does not confirm. (Project rule: never show an unconfirmed OK.)
- Without an MQTT connection there is no progress display, only a hint that
  the rename is caught up on the next connect.
- The same modal is shown after "Apply Settings" when all detectors are
  renamed. The cycle is batched: clear all, wait once, publish all.
- Fallback when the serial number is 0: the internal device ID.
- The two slug functions are consolidated.

Confirm these with the maintainer if they affect the design noticeably.

### Open

- Exact format of the fallback prefix. Suggestion: `genius_<serial>`.
- Which changelog section the entry belongs to (see 7.3).
- Wording of the history warning (depends on 4.5).
- HA being offline while the broker is reachable cannot be detected; the
  gateway does not evaluate HA's birth message anywhere. Plan: document it as
  a limitation, do not solve it.

## 6. Design: reconcile instead of an event flag

### 6.1 Desired prefix

- Option off or HA integration disabled: the detector is unmanaged. No
  `default_entity_id`, device name is the model name, no cycle. Leave the
  stored prefix untouched.
- Option on: prefix = slug(location) if the location is not empty, otherwise
  the serial-number fallback.
- Entity ID = `<component>.<prefix>_<slug(entity name)>`.
- Device display name stays as in the PR: location, or the model name when the
  location is empty. A display-name change alone is published in place and
  needs no cycle.

Because the fallback prefix differs from the display name, `HADevice` should
take the prefix explicitly. Replace `setEntityIdFromName(bool)`
(`HADevice.h:93`, used in `HADevice.cpp:143`) with something like
`setEntityIdPrefix(const String &)`, where an empty string disables it.

Two detectors with the same location get the same desired ID; HA then appends
`_2` in publish order. This is documented in the PR and stays as is.

### 6.2 Persisted state

Add one persisted string per detector: the prefix last announced to HA
(suggested name `haEntityIdPrefix`). Empty means "nothing announced under
management".

On every sync, for every managed detector: if desired prefix != stored prefix,
a rename is pending. This single comparison covers:

- a change made while offline,
- a reboot in between (a RAM flag would be lost),
- switching the option on (stored value is empty everywhere),
- no cycle when the prefix does not change (e.g. "Küche" -> "Kueche").

Points to handle:

- The comparison must run for all devices, not only inside the
  `!device.published` branch of the sync loop.
- `GeniusDevice::toJson` (`GeniusDevicesService.h:311`) serializes for both
  flash and REST, so the field will appear in the REST output and in exported
  backups. Do not trust a client-supplied value: when a device with that id
  already exists in RAM, keep its stored prefix (`mergeFromJson`,
  `GeniusDevicesService.cpp:673`). Take the value from JSON only when loading
  from flash. Check the chunked import path
  (`/rest/gateway-devices/import/*`) as well.
- Update the stored prefix only after the new configs were published
  successfully, then persist once for the whole batch. Persisting must not
  re-trigger the sync handler; `RSSI_SURVEY_UPDATE` is an existing origin id
  that the handler skips and can serve as a pattern.
- Config version: `GATEWAY_DEVICES_CONFIG_VERSION`
  (`src/GeniusDevicesService.h:78`) and `CURRENT_VERSION`
  (`interface/src/routes/gateway/smoke-detectors/+page.svelte:1162`) are both
  2 and must always be bumped together. An optional field that defaults to
  empty should not need a bump; verify against the import/export code.
- A detector added at runtime has an empty stored prefix and no entities in
  HA. Running the cycle for it is harmless but slow; it may be skipped by
  storing the prefix right after the first publish. Do not apply this shortcut
  at boot, where every detector is "added" to `_haDevices` again.

### 6.3 Triggers for the comparison

- Device saved over REST: update handler,
  `GeniusDevicesService.cpp:268` (skips the origins
  `GENIUS_DEVICE_ADDED_FROM_PACKET`, `ALARM_STATE_CHANGE`,
  `RSSI_SURVEY_UPDATE`).
- MQTT connect: `GeniusGateway::_mqttPublishTask()` (`GeniusGateway.cpp:681`)
  calls `mqttPublishAllDevices(false)`.
- Boot: `GeniusDevicesService::begin()` line 292. MQTT is not up yet, so this
  only marks renames as pending.
- HA settings applied: **missing today.** Add an update handler on
  `HASettingsService` (pattern: the handler on `_alarmPublishingSettings` in
  `GeniusDevicesService::begin()`). It has to run after
  `HASettingsService::_applyToHAService()` has pushed the new flag into
  `HAService`; verify the handler order. It must also refresh the display
  names of all detectors.

### 6.4 Deferred, batched rename

The cycle must never run in the HTTP task, in the RX path, or while the
StatefulService mutex is held. Use a worker task: either a dedicated one or
`GeniusGateway::_mqttPublishTask`, which already blocks on a notification.

Suggested sequence for all pending detectors together:

1. Emit "started" with the number of detectors.
2. Under the mutex: collect the pending detectors and write the new identity
   (name, prefix) into their `HADevice`. Release the mutex. Setting the
   identity first means a concurrent `HAService::publishAll()` during the wait
   already sends the new config, which is still correct for HA.
3. Clear the retained configs: `device->unpublishAll()` and
   `diagnostics->unpublishAll()` per detector (`SmokeDetectorHA` entries in
   `_haDevices`, `GeniusDevicesService.h:563`). Consider QoS 1 for the clear.
4. Wait 3 s without the mutex.
5. Publish: `device->publishAll()`, `diagnostics->publishAll()` and
   `_publishSmokeDetectorAttributes(id)`.
6. Under the mutex: store the new prefix, clear the pending marks, persist
   once.
7. Emit "sent".

Failure handling:

- Not connected at the start: do nothing, emit "deferred". The next connect
  runs the comparison again.
- Connection lost during the cycle: do not store the prefix. The next connect
  repeats the cycle; a second clear is harmless.

Concurrency note: `HADevice` identities are already mutated by the sync while
the `ha_publish` task may publish. Keep the mutations short; no new locking
model is required, but do not make it worse.

### 6.5 Events

Add a WebSocket event, e.g. `ha-entity-rename`, with a state
(`started`, `removing`, `waiting`, `publishing`, `sent`, `deferred`,
`failed`) and the detector count.

- Backend pattern: `AlarmLinesService.cpp:260-262` (`registerEvent`) and
  `:549`/`:557` (`emitEvent`), names defined in `AlarmLinesService.h:109`.
- Frontend pattern: `socket.on` / `socket.off` in
  `interface/src/routes/gateway/alarm-lines/+page.svelte:143-163`.

The REST save returns immediately. Open the modal when the `started` event
arrives instead of predicting on the client whether a cycle will run.

## 7. Work items

### 7.1 Backend

- [ ] 6.1: explicit prefix on `HADevice`, fallback from the serial number.
- [ ] 6.2: persisted prefix and comparison for all detectors.
- [ ] 6.3: trigger on HA settings change, refresh of display names.
- [ ] 6.4: worker task, batched cycle, no delay under the mutex. Remove the
      `vTaskDelay(3000)` at `GeniusDevicesService.cpp:1436`.
- [ ] 6.5: WebSocket event.
- [ ] 4.6: consolidate `haSlugify` and `HAService::_slugify` (one shared
      helper with a separator parameter is enough).
- [ ] Add `-D FACTORY_HA_RENAME_ENTITY_IDS=false` to `factory_settings.ini`
      next to the other `FACTORY_HA_*` flags (lines 53-57).
- [ ] Minor: `_entityIdFromName{false}` (`HADevice.h:196`) and
      `_renameEntityIds{false}` (`HAService.h:357`) use in-class initializers;
      match the style of the surrounding members.

### 7.2 Web UI

`interface/src/routes/connections/mqtt/HAConfig.svelte` (block at lines
175-190):

- [ ] `toggle toggle-primary` instead of `checkbox checkbox-primary`, as in
      every other settings form.
- [ ] Warning icon (`~icons/tabler/alert-triangle`) instead of the info icon
      inside the `alert-warning`.
- [ ] Indent the content of the `{#if}` block.
- [ ] Keep the option visible like the other fields; drop the
      `{#if f.current.enabled}` wrapper.
- [ ] Shorter hint text without capitals for emphasis. It must say that
      applying the setting renames all detectors. Wording on history depends
      on 4.5.
- [ ] Show the progress modal after "Apply Settings" when a rename starts.

`interface/src/routes/gateway/smoke-detectors/EditSmokeDetector.svelte`:

- [ ] Warning below the location field (line 403) when the option is on, the
      HA integration is enabled, the detector already exists, and the location
      differs from the saved one. Copy the pattern of the `isOldModule` block
      at line 351: `transition:slide|local={{ duration: 300, easing: cubicOut }}`,
      `alert alert-warning mt-2 gap-2 p-2`, `IconWarning` with
      `h-5 w-5 shrink-0 mt-0.5`, `text-sm`.
- [ ] The dialog needs the option state. `/rest/haSettings` requires admin,
      and so do the device endpoints, so fetching it on the smoke-detectors
      page is sufficient. Comparing the text instead of the slug is accepted;
      the warning may then appear in rare cases where no cycle follows.

Progress modal:

- [ ] Reuse `interface/src/lib/components/ProgressDialog.svelte` (phases
      `pending`, `progress`, `success`, `error`, `aborted`; `progress`,
      `message`, `stepLabel`). Usage examples: `DeviceImportDialog.svelte`,
      `PdfReportDialog.svelte`. Use `stepLabel` for "Step 2 of 3".
- [ ] Final text: "sent to Home Assistant".
- [ ] `deferred`: no progress, only a hint that the rename is caught up on the
      next connect.
- [ ] Modal dialogs in this project show a coloured icon left of the title
      (`text-primary`, `h-7 w-7`). `ProgressDialog` has no icon prop today;
      follow whatever `DeviceImportDialog` does.

### 7.3 Docs, changelog

- [ ] Replace the example IDs `sensor.julian_austausch_...` and
      `sensor.julian_alarm_line` (from the contributor's own setup) with
      neutral ones in `CHANGELOG.md` and `docs/setup/connections.md`.
- [ ] Move the changelog entry out of the `# v1.5.0` section
      (`CHANGELOG.md:56-62`). v1.5.0 is released (tag exists). This branch has
      an unreleased `# v1.5.1` section from the HA fix; `feat/rf-device-status`
      adds `# v1.6.0`. As a feature the entry most likely belongs to v1.6.0 -
      ask the maintainer.
- [ ] `docs/setup/connections.md`: place the option after
      "Manufacturer / Model", matching the UI order. Rewrite the description
      for the new behaviour (all detectors renamed on apply, catch-up after
      reconnect, serial-number fallback, HA-offline limitation).
- [ ] Update `docs/api/http-api.md:241` and `docs/api/mqtt-topics.md:639`
      accordingly; document the new WebSocket event where the others are
      documented.
- [ ] Fix or remove the history warning everywhere once 4.5 is settled.

### 7.4 Tests

`tests/test_ha_integration.py` is an automated suite (65 cases, documented in
`tests/Test-HaIntegration.md`) that can verify entity state in HA with a
long-lived token. Add cases for:

- [ ] option off: location change leaves the entity IDs untouched
- [ ] option on, location change: old IDs gone, new IDs present
- [ ] option switched on: all detectors renamed
- [ ] location cleared: IDs fall back to the serial-number prefix
- [ ] same prefix after edit (e.g. case change): no delete/recreate

Document manual checks in the appendix of `Test-HaIntegration.md`:

- [ ] location changed while MQTT is disconnected, then reconnect
- [ ] reboot between the change and the reconnect
- [ ] an alarm packet arriving during the 3 s wait is processed without delay
- [ ] history under the new ID (settles 4.5)
- [ ] upgrade from a pre-v1.5.1 firmware: watch for `_2` entities. The HA fix
      `20d39d5` clears the legacy config and publishes the new one back to
      back on different topics, where HA's per-topic queue does not help.

## 8. Constraints and conventions

- Commits: Conventional Commits (`feat(ha): ...`, `fix(ui): ...`), matching
  the history. No tool or AI attribution anywhere: no `Co-Authored-By`
  trailers, no "generated with" footers, also not in PR texts or comments.
- Hyphens, not em-dashes, in sources and docs.
- C++: Allman braces, 4 spaces, `_` prefix for private members, Doxygen
  comments, `// ====` section banners.
- Frontend: Prettier with tabs, single quotes, print width 100
  (`interface/.prettierrc`).
- The maintainer builds and flashes locally. Do not rely on running the
  PlatformIO build in an agent session; say clearly what was not built or
  tested.
- Do not push, retarget, merge or comment on GitHub without the maintainer's
  go-ahead.
- If shared components under `interface/src/lib/components` are changed, ask
  the maintainer whether the change has to be mirrored to the framework fork.
- If the primary checkout is busy with other work, use a separate git worktree
  for this branch.

## 9. Finishing

1. All items in section 7 done, test results reported to the maintainer.
2. Delete this file.
3. Rebase onto `main` if the HA fix has landed there in the meantime.
4. Open a PR to `main`. Optionally add a short note to PR #24 that the rework
   is complete.
