# PR #27 rework - instructions

> **Temporary file.** It exists only so the rework can be finished in a later
> session without the original context. Delete it in the last commit before
> this branch is merged to `main`.

## 1. Status

- Branch: `feat/radio-link-supervision` (integration branch, not `main`).
  **Local only, not pushed, no upstream configured.**
- Content: `dccdd80` (tip of `main` when the branch was created) plus
  `25d5c3a`, the single commit of PR #27, taken over unchanged by
  fast-forward (author: the contributor).
- **PR #27 is still open and targets `main`.** This is deliberate: the
  maintainer wants the contributor to be able to answer. Do not retarget,
  merge or close it without the maintainer's go-ahead.
- The maintainer's comment on the PR, with four questions to the contributor:
  https://github.com/hmbacher/genius-gateway/pull/27#issuecomment-6987908352
  **Read the PR thread before starting.** The answers affect sections 5 and 6.
- Nothing below is implemented yet.

To refresh the PR head:

```
git fetch origin pull/27/head:refs/remotes/pr/27
git log --oneline feat/radio-link-supervision..pr/27
```

## 2. Motivation of the PR

The contributor (GitHub user `danielrdt`, the same person as PR #28) runs
Symcon, not Home Assistant, and wants to check regularly that all detectors
are still reachable. Until now the gateway hears from a detector only on an
alarm, a line test or a manually started range test.

FM Pro X radio modules with the DIP switch *radio link supervision* switched
on send a daily telegram series as a heartbeat. The contributor captured
these telegrams on a test line with two FM Pro X modules over several days
and built three things on top. A large part of the code was written with an
AI assistant; the contributor states it was reviewed, built
(`pio run -e gg-v1_0`) and verified on hardware.

Without FM Pro X modules with supervision enabled there are, as far as known
today, no periodic frames, so "last seen" stays mostly empty on such
installations.

## 3. What the PR contains

One commit, 31 files, +2944 / -48. Three independent features:

### 3.1 Radio link supervision (runtime state, RAM only)

- Protocol: telegram family `0x33` - daily series `0x33/0x00` (28 B),
  "module missing" `0x33/0x02` (32 B, bytes 28-31 big endian = serial of the
  missing radio module); 36-byte type `0x00` frame with `0x08` in byte 28
  (radio network fault) and `0x10` (fault acknowledged). Further telegrams
  `0x11`, `0x22`, `0x33/0x01` documented as "function unknown".
  `docs/reverse-engineering/protocol-analysis.md:492`.
- Fixes a misclassification on `main`: the daily series has the same type
  byte and length as the CommissioningProbe Request and was shown as such.
- `src/GeniusDeviceRuntime.h` (306 lines): namespace `genius_rt`, frame
  classifier `classifySupervision()`, `RuntimeTable` with a fault state
  machine (None / Warning / Active / Acked, dropped after 48 h).
- `src/GeniusDevicesRuntime.cpp` (385 lines): `runtimeOnFrame()`,
  `runtimeTick()`, JSON serialisation, three HA entities per detector
  (`Last Seen`, `Signal Strength`, `Radio Link Fault`) on a state topic
  `link`.
- `src/GeniusGateway.cpp`: four new packet types (lines 675-702), a new
  task `gd-runtime` (line 189, 6144 bytes stack, 1 s tick), the call in the
  RX path (line 807).
- REST `GET /rest/gateway-devices/runtime` (admin), WebSocket event
  `device-runtime`.
- UI: store `deviceRuntime.svelte.ts`, `LinkFaultBadge.svelte`, last seen
  and live RSSI in list, row and details dialog.

### 3.2 Grouping by alarm line

- `interface/src/lib/genius/lineGroups.ts`,
  `interface/src/lib/stores/alarmLineNames.svelte.ts`.
- Smoke detector list: toggle in the toolbar, collapsible sections,
  reordering inside a section; the choice is stored in `localStorage`
  (`gg-group-by-line`, `smoke-detectors/+page.svelte:557`), on by default.
- Overview page: cards grouped under line headings.

### 3.3 Generic MQTT value tree

- Opt-in (`treeEnabled`, `treeBaseTopic` in `alarm-publishing.json`): one
  retained topic per value as plain text, no JSON, under
  `{root}/devices/{id}/...` and `{root}/lines/{lineId}/...`.
- `src/GeniusMqttTree.h` (495 lines, namespace `genius_tree`, planning
  logic), `src/GeniusDevicesMqttTree.cpp` (153 lines, publisher, runs on the
  `gd-runtime` task).
- Settings UI in `AlarmPublishingConfig.svelte`, docs in
  `docs/api/mqtt-topics.md`.

### 3.4 Other

- Host-side unit tests: `tests/native/test_runtime.cpp`, `test_tree.cpp`,
  runner `tests/native/run.ps1` (needs g++ or MSVC).
- `platformio.ini`: SCons pin (line 27), `APP_VERSION` 1.6.0 (line 34),
  `CONFIG_FMB_MASTER_MAX_API_BLOCKING_TIME_MS=15000` (line 169).
- `CHANGELOG.md`: a section "Unreleased".

## 4. Review findings

### 4.1 Overlap with `feat/rf-device-status` (the central point)

The maintainer's local branch `feat/rf-device-status` (commit `9878513`,
"battery warnings and fault status over radio", unpushed, at review time
untested) decodes the 36-byte type `0x00` frame as a **general status
change**: a mask of newly set and a mask of newly cleared flags plus the
detector serial. See `src/GeniusDeviceStatus.h` there:

| Flag | Meaning |
|---|---|
| `0x0001` | Alarm |
| `0x0002` | Device fault |
| `0x0004` | Battery low |
| `0x0008` | Radio link fault |
| `0x0010` | A radio link or dismount fault was muted |
| `0x0020` | Dismounted |

The PR's "codes" `0x08` and `0x10` in byte 28 are exactly the flags
`GENIUS_STATUS_RADIO` and `GENIUS_STATUS_MUTE` in the set mask; byte 30,
which the PR calls the silence flag, is the low byte of the cleared mask.
The contributor's capture is an independent on-air confirmation of these two
flags (the Genius Control showed "Störung der Funkvernetzung" and "quittiert"
at those times).

Consequences:

- The PR's packet types `HPT_RADIO_FAULT` and `HPT_RADIO_FAULT_ACK` and the
  fault part of its state machine duplicate what that branch does in a more
  general way (status per device, persisted, with clearing, HA entities,
  fault topic, UI).
- A test merge of the PR with `feat/rf-device-status` conflicts in nine
  files: `CHANGELOG.md`, `docs/api/mqtt-topics.md`,
  `docs/reverse-engineering/protocol-analysis.md`,
  `AlarmPublishingConfig.svelte`, `src/AlarmPublishingSettingsService.h`,
  `src/GeniusDevicesService.cpp/.h`, `src/GeniusGateway.cpp/.h`.
- Both bump the version to 1.6.0.

### 4.2 Unconfirmed OK state

- The HA entity `Radio Link Fault` is OFF whenever no fault is stored
  (`src/GeniusDevicesRuntime.cpp:359-361`): after every reboot (RAM only)
  and 48 h after the last frame. The value tree publishes
  `link_fault = false` likewise.
- The 48 h expiry is a guess. Status flags are edge-triggered: a detector
  announces a fault once and does not repeat it. The PR ignores frames with
  a non-zero cleared mask (`classifySupervision()` requires byte 30 == 0),
  so an actual "fault cleared" frame is not evaluated.
- Project rule: a status stays unknown/unavailable until it was actually
  reported; no default OK.

### 4.3 Structure and scope

- Three features in one commit; they should be separate changes that can be
  judged one by one.
- `platformio.ini`: the SCons pin and the Modbus setting have nothing to do
  with the features (the project does not use Modbus). The contributor was
  asked what they solve; leave them out unless there is a reason.
- Flash: the PR states about 92 % of the app partition on `gg-v1_0` after
  the change, without the maintainer's other open branches.
- RX path: `runtimeOnFrame()` takes the state mutex (`beginTransaction()`)
  for every non-duplicate frame. Update handlers hold that mutex while they
  run (including file writes), so the RX task can block on it more often
  than before. Check whether a lock-free lookup or a snapshot of the serial
  numbers is the better choice.
- Three more HA entities per detector on a third state topic; check against
  the limits in `docs/development/memory.md`.

### 4.4 Documentation

- `docs/api/mqtt-topics.md:960`: `---` directly below a text line turns that
  line into a heading (Setext). A blank line is missing.
- The new REST endpoint and the WebSocket event are not in
  `docs/api/http-api.md` / `docs/api/websocket-api.md`; nothing in
  `docs/features/` (device management, smart home integration, overview).
- The changelog uses a section "Unreleased"; the project has versioned
  sections only.

### 4.5 Style

- Namespaces, `std::string`, `std::function`, `std::map` and header-only
  logic do not occur in `src/` on `main`. They make the host-side tests
  possible, so this is a decision, not a defect (section 7).
- Shortened license headers; the project's files carry the full header (see
  any file in `src/`).
- File names easy to confuse: `GeniusDeviceRuntime.h` and
  `GeniusDevicesRuntime.cpp`. `GeniusDevicesService` is now implemented in
  three `.cpp` files.
- UI: card title changed to "Simple Alarm Publishing / Value Tree"
  (`AlarmPublishingConfig.svelte:97`), wrong indentation in the added block
  there, `title=` attributes instead of the DaisyUI tooltips used elsewhere,
  `authHeaders()` duplicated in `routes/+page.svelte`.

### 4.6 Not verified in the review

No firmware build, no `npm run check`, the host tests were not run (no g++
on the review machine), nothing tested on hardware. The contributor's
evidence is two modules and a single fault; whether fault frames are
forwarded through the mesh is unknown.

## 5. Decisions already taken by the maintainer

- The PR stays open for now.
- The rework is done by the maintainer's side, not by the contributor.
- Radio link supervision: wanted. The fault handling is to be rebuilt on the
  status-change model while keeping the supervision telegrams, the "module
  missing" cause, last seen and live RSSI.
- Grouping by alarm line: wanted, as its own change.
- **Generic value tree: the maintainer finds it interesting and would like
  to try to integrate it if it fits in.** It is treated as its own change
  after the supervision part. The points to sort out are flash space and how
  it sits next to Home Assistant discovery and the simple topics. Do not
  drop it without asking.

## 6. Work plan

### 6.0 Precondition

`feat/rf-device-status` must be finished, tested and merged to `main` first.
It is the base of the rework. Ask the maintainer for its state before
starting. Then bring `main` into this branch (or rebuild the three features
on fresh branches from `main`, which is likely easier than resolving nine
conflicting files by hand).

### 6.1 Radio link supervision

- [ ] Keep: classification of `0x33/0x00` and `0x33/0x02`, the new packet
      types for them, the fix of the CommissioningProbe misclassification,
      the protocol documentation including the "unknown" telegrams.
- [ ] Remove `HPT_RADIO_FAULT` / `HPT_RADIO_FAULT_ACK` and the special
      handling of bytes 28/30; the status-change handling of the base
      covers them. Regenerate `interface/src/lib/types/enums.ts`.
- [ ] Fault state: take it from the device status of the base (radio flag
      and mute flag, set and cleared). Add only what is new: the cause (the
      serial from `0x33/0x02`, with location if the module is known) and the
      early "warning" before the fault.
- [ ] No default OK: `Radio Link Fault` and the corresponding UI and topic
      values are unknown/unavailable until reported. Decide what the 48 h
      expiry becomes (section 7).
- [ ] Last seen and live RSSI stay RAM only. Check the interplay with the
      persisted range-test RSSI (`radioModule.rssi`, `lastRangeTest`) and
      the existing signal indicator.
- [ ] Protocol docs: describe `0x08` / `0x10` as flags of the status change,
      cite the contributor's capture as confirmation, and add the raw frames
      to `docs/reverse-engineering/protocol-captures.md` if the contributor
      supplies them.
- [ ] RX path locking (section 4.3).
- [ ] API docs for the REST endpoint and the WebSocket event; feature docs.

### 6.2 Grouping by alarm line

- [ ] Split into its own branch/commit. It needs nothing from 6.1 except the
      fault badge in the group headings; use the device status of the base
      for that.
- [ ] Decide the default (section 7).
- [ ] The overview page conflicts with the rework of PR #25
      (`feat/overview-card-dialogs`, `interface/src/routes/+page.svelte`);
      whichever lands second resolves it.
- [ ] Docs: device management and overview pages, screenshots.

### 6.3 Generic value tree

- [ ] Split into its own branch/commit, after 6.1.
- [ ] Measure the flash cost on `gg-v1_0` with and without it (the
      maintainer builds locally).
- [ ] Define how it relates to the simple alarm topic and to the fault topic
      of the base (`faultEnabled` / `faultTopic`): the tree publishes the
      same information in another shape. Keep the settings card readable;
      restore the card title or give the tree its own card.
- [ ] Publish device status from the base's status flags instead of the
      PR's own fault model; unknown stays an empty retained payload, as the
      tree already does for other values.
- [ ] Fix the Setext heading in `docs/api/mqtt-topics.md`; document the
      settings in `docs/setup/connections.md`.

### 6.4 Common

- [ ] `platformio.ini`: drop the SCons pin and the Modbus setting unless the
      contributor's answer gives a reason; set the version together with the
      maintainer.
- [ ] Changelog: versioned section in the project's style.
- [ ] License headers and file names (section 4.5).
- [ ] Host tests: keep and adapt them if the pure-C++ split is kept
      (section 7); document how to run them.
- [ ] `npm run check` in `interface/` must not get worse (PR #26 brings it
      to near zero on `chore/svelte-check-errors`).
- [ ] Hardware test. The supervision telegrams need FM Pro X modules with
      supervision switched on; the contributor was asked to test the
      reworked branch.

## 7. Open decisions for the maintainer

1. **Expiry of faults.** Keep a timeout at all, or rely only on the cleared
   mask and the manual reset of the base? If the contributor captured a
   "fault cleared" frame, the timeout is unnecessary.
2. **Persist the "module missing" cause** with the device status, or keep it
   RAM only?
3. **Grouping default**: on, off, or on only when more than one alarm line
   exists.
4. **Pure-C++ headers with namespaces and std types** for host-testable
   logic: accept as a new pattern in `src/`, or convert to the existing
   style and drop the host tests.
5. **Value tree**: own settings card or part of the simple publishing card;
   whether it should also carry commands later (the PR documents that it
   does not).
6. **How the PR ends** (section 9).

## 8. Constraints and conventions

- Commits: Conventional Commits, matching the recent history. No tool or AI
  attribution anywhere: no `Co-Authored-By` trailers, no "generated with"
  footers, also not in PR texts or comments.
- Hyphens, not em-dashes, in sources and docs.
- C++: Allman braces, 4 spaces, `_` prefix for private members, Doxygen
  comments. Frontend: Prettier with tabs, single quotes, print width 100;
  `toggle toggle-primary` for booleans.
- A status is never shown as OK before it was reported.
- If the persisted device configuration changes, bump
  `GATEWAY_DEVICES_CONFIG_VERSION` (backend) and `CURRENT_VERSION`
  (frontend) together.
- Do not run the PlatformIO build in an agent session; the maintainer builds
  and flashes locally. Say clearly what was not built or tested.
- Do not push, retarget, merge, close or comment on GitHub without the
  maintainer's go-ahead.
- The primary checkout is on `feat/rf-device-status` with uncommitted
  documentation changes. Work in this worktree and leave the primary
  checkout alone. Use `git -C <worktree>` or absolute paths; do not rely on
  `cd` in chained commands.
- Do not repeat the contributor's e-mail address anywhere.

## 9. Finishing

Ask the maintainer how PR #27 is to end once the contributor has answered:

**A. Same as PRs #24 to #26.** Push this branch up to `25d5c3a`, retarget
PR #27 to it and squash-merge; the PR then shows as merged with the
contributor as author. Squash message prepared in the review:

```
feat: radio link supervision, grouping by alarm line, MQTT value tree (#27)

Three independent features, taken over as contributed:

- Radio link supervision: classifies the 0x33 telegram family and the
  radio fault / acknowledged frames; keeps last seen, live RSSI and link
  faults per device in RAM (REST endpoint, device-runtime event, three
  HA entities per detector)
- Smoke detector list and overview grouped by alarm line
- Optional generic MQTT value tree, one plain-value topic per value

To be reworked on the integration branch: the fault handling moves to
the status-change model and the three features are split.
```

**B. Rebuild on fresh branches** from `main` after `feat/rf-device-status`
has landed, credit the contributor in the commits (for example as commit
author of the parts taken over), and close PR #27 with a comment that links
the new PRs.

Either way: the items in section 6 are done, the results are reported to the
maintainer, this file is deleted, and the branch is rebased onto `main`
before a PR to `main` is opened.
