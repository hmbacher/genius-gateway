# Spec: Manual Line Entry for Old FM Modules (FM.Basis / FM.Pro)

> **Status:** Phase 1 implemented (manual entry + device metadata). Phase 2 (rotary → Line-ID correlation) still pending the RE mapping.
> **Audience:** an agent (or developer) implementing the feature inside this repo.
> **Goal:** mirror the official Hekatron app behaviour — when a smoke detector carries an **old-generation** radio module (FM.Basis or FM.Pro), the alarm line **cannot** be derived from the device and **must be entered manually** by the user.

This document is a self-contained brief: background, the precise rules to replicate, the exact code touchpoints in this repo, the data model, validation rules, UI work, phasing, and acceptance criteria.

---

## 1. Why this is needed (background)

Old and new radio modules identify their alarm line in **fundamentally different, incompatible** ways. See [`reverse-engineering/protocol-analysis.md`](../reverse-engineering/protocol-analysis.md#line-addressing-fm-basis-x-vs-old-fm-basis) (section "Line Addressing") and [`reverse-engineering/acoustic-readout.md`](../reverse-engineering/acoustic-readout.md) for the full story.

| | **FM Basis X / FM Pro X (new)** | **FM Basis / FM Pro (old)** |
|---|---|---|
| Line identity | 32-bit **Identifikationscode**, randomly assigned at radio commissioning | **Rotary switch** address only (`A–H` + `0–9`); no dynamic ID |
| Rotary switch | FM Basis X: none (fixed `A.0`). FM Pro X: yes (≤70 lines) | Both have rotary; FM Basis factory `A.0` but changeable, ≤70 lines |
| Acoustic readout `lineId` (bytes 25–28) | non-zero once commissioned | **`0`** (old modules transmit only type + serial over SmartSonic) |
| Acoustic line byte 29 (char/number) | reliable | **unreliable** — the official app does **not** trust it |

**Consequence:** for an old module, neither the acoustic readout nor (currently) the radio packets give us a usable alarm-line identity we can trust. The official Hekatron Genius Control app handles this by **forcing the installer to enter the line by hand**. The Genius Gateway must do the same.

### What the official app does (the behaviour to replicate)

Reverse-engineered from the decompiled Genius Control app (`SmartSonicValidationService`, `AddDeviceManualLineEntryFragment`, `Funkmodul`):

- `isManualLineEntryRequired(tunerData)` returns **true** iff the radio module is **FM.Basis or FM.Pro** (the old ones). For these, the add-device wizard inserts a manual line-entry step instead of reading the line from the signal.
- `isOldFm(...)` = module ∈ { FM.Basis, FM.Pro }.
- In the manual line-entry screen: `setHLineEnabled(Funkmodul.FM_BASIS != funkmodul)` — the **"H" major line is disabled for FM.Basis** (collective-alarm `H.x` lines are a *Pro* feature; Basis cannot do Sammelalarm).
- `Funkmodul.lineChangeable`: `false` for FM.Basis X, `true` for FM.Basis / FM.Pro / FM.Pro X.

> ⚠️ Do **not** confuse the wire/enum numbering. Over-the-air and in this repo's enum: `FM.Basis = 1`, `FM.Pro = 2`, `FM.Basis X = 4`, `FM.Pro X = 5`. (The official app's internal `TUNRadioProduct` enum uses different ordinals — irrelevant here.)

---

## 2. Current state in this repo (what already exists)

**Backend (`src/GeniusDevicesService.h` / `.cpp`):**

- `enum GeniusRadioModule`: `GRM_NONE=0, GRM_FM_BASIS=1, GRM_FM_PRO=2, GRM_FM_MCP=3, GRM_FM_BASIS_X=4, GRM_FM_PRO_X=5` (`src/GeniusDevicesService.h:109`).
- `struct GeniusRadioModuleInfo` already carries the fields we need (`src/GeniusDevicesService.h:201`):
  - `uint32_t lineId;` `char lineCharacter; // 'A'–'J', 0 if unknown` `uint8_t lineNumber;`
  - `fromJson` already clamps `lineCharacter` to `'A'..'J'` (`src/GeniusDevicesService.h:238`).
- `enum genius_device_registration_t`: `GDR_GENIUS_PACKET, GDR_MANUAL, GDR_ACOUSTIC` (`src/GeniusDevicesService.h:248`).
- Device → MQTT `alarm_line` attribute is built as `lineCharacter + "." + lineNumber` (`src/GeniusDevicesService.cpp:1131`).
- Per-device CRUD endpoints exist: `PUT /rest/gateway-devices/device` (`upsertDevice`), etc.

**Alarm lines (`src/AlarmLinesService.h`):**

- `struct genius_alarm_line_t { uint32_t id; String name; time_t created; alarm_line_acquisition_t acquisition; ... }`.
- `enum alarm_line_acquisition_t`: `ALA_BUILT_IN, ALA_GENIUS_PACKET, ALA_MANUAL, ALA_ACOUSTIC`.
- `addAlarmLine(uint32_t id, String name, acquisition, toFront)`; lines keyed by the **32-bit radio Line-ID**; `ALARMLINES_ID_NONE=0`, `ALARMLINES_ID_BROADCAST=0xFFFFFFFF` reserved.

**Frontend:**

- Acoustic decode: `interface/src/lib/audio/tuner-parser.ts` — parses `lineCharacter = 'ABCDEFGHIJ'[idx]`, `lineNumber`, `lineId` (`tuner-parser.ts:113`). Types in `interface/src/lib/audio/tuner-types.ts`.
- Device model types: `interface/src/lib/types/models.ts:181` (`lineCharacter?`, `lineNumber?`).
- Readout → device creation: `interface/src/routes/gateway/smoke-detectors/+page.svelte:391`.
- Device display: `interface/src/routes/gateway/smoke-detectors/DeviceDetailsDialog.svelte:457` (shows `lineCharacter.lineNumber`).
- Alarm-lines UI: `interface/src/routes/gateway/alarm-lines/+page.svelte`, `EditAlarmLine.svelte`, `+page.ts`.

**Conclusion:** the data model is already sufficient. What's missing is (a) **detecting** old modules, (b) **refusing to trust** the readout line for them, (c) a **manual-entry UI** with the right validation, and (d) optionally **correlating** the rotary line to a 32-bit alarm-line entry.

---

## 3. The gap — what to build

### 3.1 Old-module detection helper (backend + frontend, shared semantics)

Add a single predicate, used everywhere:

```
isOldFmModule(model) := (model == GRM_FM_BASIS) || (model == GRM_FM_PRO)
isManualLineEntryRequired(model) := isOldFmModule(model)
```

Backend: a free/inline function near the `GeniusRadioModule` enum in `src/GeniusDevicesService.h`.
Frontend: a helper in `interface/src/lib/types/models.ts` (or a small `lib/genius/line.ts`) so the UI and validation share one definition. Mirror the enum values exactly.

### 3.2 Do not trust the acoustic line for old modules

In the acoustic readout path (`interface/src/lib/audio/tuner-parser.ts` + the consumer at `smoke-detectors/+page.svelte:391`):

- When the parsed `radioModule.model` is old (`GRM_FM_BASIS`/`GRM_FM_PRO`):
  - Treat `lineId`, `lineCharacter`, `lineNumber` from the signal as **unset/untrusted** (the byte-29 value is unreliable and `lineId` is `0`).
  - Mark the device as **"line entry required"** and route the user to manual entry before the device can be considered fully configured.
- For new modules (X), keep current behaviour (use the readout values).

### 3.3 Manual line-entry UI

Add a manual line-entry step/dialog (new component, e.g. `interface/src/routes/gateway/smoke-detectors/ManualLineEntry.svelte`, reused from both the acoustic-readout result flow and manual device add/edit). It must:

- Show **major** selector `A`–`H` and **minor** selector `0`–`9`.
- **Disable `H`** when the module is **FM.Basis** (`GRM_FM_BASIS`) — Sammelalarm lines are Pro-only (replicates `setHLineEnabled(FM_BASIS != funkmodul)`).
- Validate the chosen line against the line table in §4.
- On save, write `lineCharacter` (`'A'..'H'`) and `lineNumber` (`0..9`) onto the device's `radioModule` and persist via the existing per-device `PUT /rest/gateway-devices/device`.
- Default to `A.0` (factory default for FM.Basis) when nothing is set.

### 3.4 Provenance flag (recommended)

So the UI can distinguish "line came from a reliable signal" vs "user typed it", add an optional boolean to `GeniusRadioModuleInfo` and the JSON, e.g. `lineManual` (default false). Set it `true` when the line was entered via §3.3. This also lets the device list flag old-module devices whose line is still **unset** (needs attention). Keep it backward compatible in `fromJson`/`toJson` (omit when false).

### 3.5 Alarm-line correlation (phased — see §6)

The GG's alarm-line entity is keyed by the **32-bit radio Line-ID**. Old modules don't expose that via the readout. Handle in two phases (§6): Phase 1 lets the admin **manually associate** the device's rotary line with an alarm-line entry; Phase 2 derives the 32-bit Line-ID from the rotary once the mapping is known.

---

## 4. Line validity rules (from the Hekatron FM Basis X / Pro X manual)

A line = **major letter** + **minor digit**. Validation the UI must enforce:

| Major | Minors | Meaning | Allowed for |
|------|--------|---------|-------------|
| `A`–`G` | `0`–`9` | Normal alarm lines | FM.Basis **and** FM.Pro |
| `H` | `0,1,2,4,5,7` | **Sammelalarm** (collective) lines | **FM.Pro only** (disable `H` for FM.Basis) |
| `H` | `3,6,8,9` | No function (`H.8` only for Funkhandtaster Einzelbetrieb) | — (reject) |
| `I` | `1` | Reserved: **range test** (Reichweitentest), no normal traffic | — (reject for normal use) |
| `I` | `2`–`9` | No function | — (reject) |
| `J` | `0`–`9` | No function | — (reject) |

Notes:
- FM.Basis cannot do Sammelalarm → restrict its majors to `A`–`G`.
- FM.Pro may also use the `H.x` Sammelalarm lines listed above.
- The detailed Sammelalarm cross-line matrix (which `H.x` line talks to which normal lines) is in the manual; not required for entry validation, but useful context.
- Keep the model field range `'A'..'J'` as-is (already validated in `fromJson`), but the **entry UI** should only offer/accept the valid subset above.

---

## 5. Exact code touchpoints

**Backend**
- `src/GeniusDevicesService.h`
  - `enum GeniusRadioModule` (`:109`) — reference for old/new classification.
  - `struct GeniusRadioModuleInfo` (`:201`) — `lineId`/`lineCharacter`/`lineNumber`; add `lineManual` here + in `toJson`/`fromJson` (`:216`, `:230`).
  - Add `static inline bool isOldFmModule(GeniusRadioModule)` / `isManualLineEntryRequired(...)`.
- `src/GeniusDevicesService.cpp`
  - `~:1054` model→name strings (`"FM Basis"`, `"FM Basis X"`).
  - `~:1131` `alarm_line` MQTT attribute (`lineCharacter.lineNumber`) — ensure it reflects the manually entered line.
  - `~:697` config-version migration — bump `GATEWAY_DEVICES_CONFIG_VERSION` only if you change persisted shape; `lineManual` can be added without a bump if optional.
- `src/AlarmLinesService.h` — `addAlarmLine`, `genius_alarm_line_t`, `alarm_line_acquisition_t` (for Phase-2 correlation).

**Frontend**
- `interface/src/lib/audio/tuner-parser.ts` (`:113`) — gate line fields for old modules.
- `interface/src/lib/audio/tuner-types.ts`, `interface/src/lib/types/models.ts` (`:181`) — add `lineManual?`; shared old-module predicate.
- `interface/src/routes/gateway/smoke-detectors/+page.svelte` (`:391`) — readout→device: route old modules to manual entry; don't copy untrusted line.
- `interface/src/routes/gateway/smoke-detectors/DeviceDetailsDialog.svelte` (`:457`) — show "line set manually" / "line required".
- New: `ManualLineEntry.svelte` (major/minor pickers + validation from §4).
- `interface/src/routes/gateway/alarm-lines/EditAlarmLine.svelte` — for Phase-2 association UI.

---

## 6. Phasing

**Phase 1 — Manual entry + device metadata (do this first).**
Detect old modules, force manual line entry, persist `lineCharacter`/`lineNumber`(+`lineManual`), display correctly. The rotary line is stored as device metadata and shown in the UI / MQTT `alarm_line` attribute. No automatic radio correlation yet. This already matches what the official app does for the installer.

**Phase 2 — Rotary → 32-bit Line-ID correlation (depends on an open RE finding).**
Old modules emit a 32-bit `Line-ID` on the 868 MHz radio that is (hypothesis) a **deterministic function of the rotary position**. Known data point: rotary **`A.0` → `0xD7F75240` (3623309888)**. See the experiment + table in [`reverse-engineering/protocol-analysis.md`](../reverse-engineering/protocol-analysis.md#line-addressing-fm-basis-x-vs-old-fm-basis). Once the mapping `lineId = f(major, minor)` is known:
- Compute the 32-bit Line-ID from the entered rotary line.
- Auto-create / match the alarm-line entry (`addAlarmLine(id, name, ALA_MANUAL)`) so the GG can address old-module lines for line test / fire alarm exactly like commissioned X lines, and so incoming old-module radio packets correlate to the right line.
- Until the mapping is confirmed, allow the admin to **manually associate** the device's rotary line with an existing alarm-line entry (32-bit id).

Do **not** hardcode a single `A.0 → 0xD7F75240` mapping as if general — it's one sample. Gate Phase 2 behind the confirmed mapping.

---

## 7. Acceptance criteria

1. Reading out a Genius Plus X with an **FM.Basis** module (acoustic `lineId == 0`) does **not** silently accept a line from the signal; the UI requires manual entry.
2. The manual-entry UI offers major `A`–`H` / minor `0`–`9`, **disables `H` for FM.Basis**, and rejects invalid combinations per §4.
3. Saved line persists on `radioModule.lineCharacter`/`lineNumber`, survives reboot, and appears in the device details and the MQTT `alarm_line` attribute as `X.n`.
4. New modules (FM.Basis X / FM.Pro X) are **unaffected** — their line still comes from the readout/commissioned ID.
5. Devices with an old module and **no line yet** are visibly flagged as needing attention.
6. (Phase 2, when enabled) entering `A.0` for an old module correlates to the radio Line-ID `0xD7F75240`, and line test / fire alarm addressing works against it.

---

## 8. Canonical references

- [`reverse-engineering/protocol-analysis.md`](../reverse-engineering/protocol-analysis.md#line-addressing-fm-basis-x-vs-old-fm-basis) — old vs X line addressing; the rotary→Line-ID hypothesis, data point, and experiment.
- [`reverse-engineering/acoustic-readout.md`](../reverse-engineering/acoustic-readout.md) — SmartSonic payload; `lineId` bytes 25–28, line byte 29.
- [`features/alarm-lines-management.md`](../features/alarm-lines-management.md) — current alarm-lines model & acquisition methods.
- Official-app basis (decompiled, external): `SmartSonicValidationService.isManualLineEntryRequired/isOldFm`, `AddDeviceManualLineEntryFragment` (`setHLineEnabled(FM_BASIS != …)`), `Funkmodul.lineChangeable`. Hekatron manuals: *Funkmodul Basis X / Pro X* (7003145) §"Linienübersicht"/§7; *Genius Funksysteme im Mischbetrieb* (7050644).

> Reminder for the implementer: the byte-level acoustic decoder is **module-type agnostic** — the "old module ⇒ untrusted line" rule is an **application-layer** policy, not a parser change. The detector signals an unusable old FM module via the `FmFault` bit; the official app surfaces a re-seat/replace advice (`OldFMData` string) but still relies on **manual** line entry for the line itself.
