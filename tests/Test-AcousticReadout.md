# Acoustic Readout - Manual Test Suite

Covers all conditional paths in the acoustic device detection and per-device readout features.

**Requires:** Genius Gateway accessible via HTTPS, physical Genius smoke detectors capable of transmitting acoustic (SmartSonic) signals, browser with microphone permission granted.

**Notation:** `SD-SN` = smoke detector serial number, `FM-SN` = radio module serial number.

---

## Setup conventions

| Symbol | Meaning |
|--------|---------|
| `[new SD-SN / new FM-SN]` | Device not yet registered on the gateway |
| `[known SD-SN / known FM-SN]` | Device already registered under a specific entry |
| `[–]` | Component absent (no radio module fitted, or SN = 0 in readout) |

Start each test group from a **clean device list** unless stated otherwise. Use the export/import function to save and restore a known baseline.

---

## Section 1 - Connection guard

These tests apply to both **Acoustic Device Detection** (toolbar button) and the per-device **Readout** action button.

### TC-CG-01 - Detection blocked on plain HTTP - OK

**Precondition:** Access the smoke detectors page over `http://` (not HTTPS).

**Steps:**
1. Click the **Acoustic Device Detection** toolbar button.

**Expected:** Error toast appears - *"Acoustic device detection requires a secure (HTTPS) connection."* No dialog opens.

---

### TC-CG-02 - Readout blocked on plain HTTP - OK

**Precondition:** Access the smoke detectors page over `http://`. At least one device is registered.

**Steps:**
1. Open device details for any device.
2. Click the **Update** (readout) button.

**Expected:** Error toast appears - *"Acoustic device detection requires a secure (HTTPS) connection."* No dialog opens.

---

### TC-CG-03 - Readout button style on insecure context - OK

**Precondition:** Access over `http://`.

**Steps:**
1. Open device details for any device and inspect the **Update** button.

**Expected:** Button is rendered in warning style with a cancel/warning icon overlay instead of the normal microphone icon.

---

## Section 2 - Initial Acoustic Device Detection (`handleAcousticResult`)

Triggered via the **Acoustic Device Detection** toolbar button. The gateway has no prior knowledge of the device being detected unless stated.

---

### TC-AD-F1 - Completely unknown device → Add new - OK

**Precondition:** Neither `SD-SN` nor `FM-SN` of the detector to be detected matches any registered device.

**Steps:**
1. Click **Acoustic Device Detection**.
2. Hold detector microphone close to browser microphone; trigger transmission.
3. Wait for *"Data received successfully!"* in the detection dialog.
4. Click **Continue**.

**Expected:**
- `EditSmokeDetector` dialog opens pre-filled with the detected data, save button labelled **Add**.
- After saving, device appears in the list with correct SD and FM serial numbers.
- `checkAndOfferAlarmLine` runs (see Section 4).

---

### TC-AD-F2 - Known SD, unknown FM → Update existing device (RWM match) - OK

**Precondition:** A device is registered with `SD-SN=A`, FM either absent or set to a different SN that does **not** appear in the readout. Detect a unit whose `SD-SN=A` (same smoke detector body, FM not present or not matching any other device).

**Steps:**
1. Click **Acoustic Device Detection**.
2. Trigger transmission from the detector with `SD-SN=A`.
3. Click **Continue** when data is received.

**Expected:**
- `ConfirmDialog` appears asking to update the existing device that matched by smoke detector SN.
- After confirming, device entry is updated silently; toast *"Readout data updated successfully."*
- No EditSmokeDetector dialog is shown.

---

### TC-AD-F3 - Known SD and FM, both match the same device → Update existing (full match) - OK

**Precondition:** A device is registered with `SD-SN=A` and `FM-SN=B`. Detect the same physical unit.

**Steps:**
1. Click **Acoustic Device Detection**.
2. Trigger transmission from the detector with `SD-SN=A / FM-SN=B`.
3. Click **Continue**.

**Expected:** Same outcome as TC-AD-F2 - confirmation dialog to update, silent update after confirm, no EditSmokeDetector dialog.

---

### TC-AD-F4 - Unknown SD, known FM → FM replacement (new smoke detector body) - OK

**Precondition:** A device is registered with `SD-SN=X` (known) and `FM-SN=B`. The physical smoke detector head has been replaced: detect a unit with a **new** `SD-SN=Y` but **same** `FM-SN=B`.

**Steps:**
1. Click **Acoustic Device Detection**.
2. Trigger transmission from the unit with `SD-SN=Y / FM-SN=B`.
3. Click **Continue**.

**Expected:**
- `ConfirmDialog` warns that accepting will replace the existing device and its alarm history will be lost.
- After clicking **Replace**, `EditSmokeDetector` dialog opens for review; save button labelled **Replace**.
- After saving, the existing device entry is updated with the new `SD-SN=Y`; its alarm log is cleared.

**Edge - cancel at confirm:** Dismiss the ConfirmDialog → no changes made.

**Edge - cancel at EditSmokeDetector:** Close the editor dialog → no changes made.

---

### TC-AD-F5 - SD matches device A, FM matches device B → Cross-duplicate - OK

**Precondition:** Two devices are registered: device A with `SD-SN=A / FM-SN=B`, device B with `SD-SN=C / FM-SN=D`. Components have been swapped; detect a unit reporting `SD-SN=A / FM-SN=D`.

**Steps:**
1. Click **Acoustic Device Detection**.
2. Trigger transmission from the mixed unit.
3. Click **Continue**.

**Expected:**
- `ConfirmDialog` explains that the components belong to two different known devices and both will be removed.
- After clicking **Continue**, both device A and device B are removed from the list.
- `EditSmokeDetector` dialog opens to add the combined device as new; save button labelled **Add**.
- After saving, one new device entry appears with `SD-SN=A / FM-SN=D`.

**Edge - cancel at confirm:** Dismiss → both original devices remain, nothing is added.

---

### TC-AD-F5b - Cancel during detection dialog - OK

**Precondition:** Any device state.

**Steps:**
1. Click **Acoustic Device Detection**.
2. While the dialog shows *"Waiting for audio signal..."*, click **Cancel**.

**Expected:** Dialog closes, no changes, no crash.

---

## Section 3 - Per-device Readout (`handleDeviceReadout`)

Triggered via **Open details → Update** on a specific device card. `targetIndex` is the position of that device in the list.

---

### TC-RO-A1 - SD matches target, FM matches target (or no FM) → Silent update, no dialog - OK

**Precondition:** Device at index 0 has `SD-SN=A` and `FM-SN=B` (or no FM module, `FM-SN=0`).

**Steps:**
1. Open device details for device at index 0.
2. Click **Update**.
3. Trigger transmission from the same physical unit (`SD-SN=A / FM-SN=B` or `SD-SN=A / FM-SN=0`).
4. Click **Continue**.

**Expected:**
- No confirmation dialog shown.
- Readout data is applied directly; toast *"Readout data updated successfully."*
- `checkAndOfferAlarmLine` runs (see Section 4).

---

### TC-RO-A2 - SD matches target, FM present but mismatches stored FM → Confirm FM mismatch - OK

**Precondition:** Device at index 0 has `SD-SN=A` and `FM-SN=B` (both non-zero). Detect a unit that transmits `SD-SN=A` but `FM-SN=C` (different radio module, `C ≠ B`, `C` not registered on any other device).

**Steps:**
1. Open device details for device at index 0.
2. Click **Update**.
3. Trigger transmission with `SD-SN=A / FM-SN=C`.
4. Click **Continue**.

**Expected:**
- `ConfirmDialog` appears noting the radio module SN differs from the stored value and asks whether to update anyway.
- After clicking **Update**, readout data (including new `FM-SN=C`) is applied; toast appears.

**Edge - cancel:** Dismiss the ConfirmDialog → device data unchanged.

---

### TC-RO-A2b - SD matches target, FM SN is 0 in readout (no FM detected) → Silent update - OK

**Precondition:** Device at index 0 has `SD-SN=A` and `FM-SN=B` (non-zero stored). Trigger a readout that only produces `FM-SN=0` (radio module not present or not responding).

**Steps:**
1. Open device details for device at index 0.
2. Click **Update** and trigger transmission with `SD-SN=A / FM-SN=0`.
3. Click **Continue**.

**Expected:** Because `fmSN = 0`, the FM mismatch condition is not triggered. Silent update without confirmation dialog (Case A1). Toast appears.

---

### TC-RO-B - Both SD and FM match a different device - OK

**Precondition:** Two devices registered - device at index 0 (`SD-SN=A / FM-SN=B`) and device at index 1 (`SD-SN=C / FM-SN=D`). Open details for device at index 0 and trigger readout for the physical unit at index 1 (`SD-SN=C / FM-SN=D`).

**Steps:**
1. Open device details for device at **index 0**.
2. Click **Update**.
3. Trigger transmission from the unit belonging to device at index 1.
4. Click **Continue**.

**Expected:**
- `ConfirmDialog` appears noting the readout matches a **different** device (index 1, not the one whose details were opened).
- After confirming, device at index 1 is updated (not index 0); toast appears.
- Device at index 0 is unchanged.

**Edge - cancel:** Dismiss → neither device changed.

---

### TC-RO-C - SD and FM both unknown while doing a targeted readout → Add new - OK

**Precondition:** Device at index 0 has `SD-SN=A / FM-SN=B`. Trigger readout with a completely different, unregistered unit (`SD-SN=X / FM-SN=Y`).

**Steps:**
1. Open device details for device at index 0.
2. Click **Update**.
3. Trigger transmission from the unregistered unit.
4. Click **Continue**.

**Expected:**
- `ConfirmDialog` asks whether to add the detected unit as a new device.
- After confirming, `EditSmokeDetector` dialog opens; save button labelled **Add**.
- After saving, a new device entry is added. Device at index 0 is unchanged.

**Edge - cancel at confirm:** Dismiss → nothing added, index 0 unchanged.

---

### TC-RO-D - Partial / conflicting match → Delegate to initial detection flow - OK

**Precondition:** Device at index 0 has `SD-SN=A / FM-SN=B`, device at index 1 has `SD-SN=C / FM-SN=D`. Open details for device at index 0 and trigger a readout with `SD-SN=A / FM-SN=D` (SD matches index 0, FM matches index 1 - a conflict).

**Steps:**
1. Open device details for device at **index 0**.
2. Click **Update**.
3. Trigger transmission with `SD-SN=A / FM-SN=D`.
4. Click **Continue**.

**Expected:**
- `InfoDialog` (warning) appears explaining the readout does not unambiguously match the selected device.
- After dismissing, the flow is handed off to `handleAcousticResult` - the normal acoustic detection logic runs from that point (Falls 1–5 apply based on the SN matches).

---

## Section 4 - Alarm line auto-add (`checkAndOfferAlarmLine`)

Triggered automatically after any successful acoustic update or add. Requires the detected unit to carry a `lineId` (radio module line assignment).

---

### TC-AL-01 - No lineId in readout → No prompt - OK

**Precondition:** Detect a unit whose radio module reports no line assignment (`lineId = 0` or absent).

**Steps:**
1. Complete any successful TC-AD or TC-RO test where the unit has no lineId.

**Expected:** No alarm line dialog appears after the update/add succeeds.

---

### TC-AL-02 - lineId already configured → No prompt - OK

**Precondition:** An alarm line matching the detected unit's `lineId` already exists in the alarm lines configuration.

**Steps:**
1. Ensure the alarm line exists.
2. Complete a successful TC-AD-F1 or TC-RO-A1 with a unit reporting that `lineId`.

**Expected:** No alarm line dialog appears; existing alarm line is not duplicated.

---

### TC-AL-03 - New lineId → Offer to add alarm line, confirm - OK

**Precondition:** No alarm line for the detected unit's `lineId` exists yet.

**Steps:**
1. Complete a successful TC-AD-F1 or TC-RO-A1 with a unit reporting an unconfigured `lineId`.
2. `ConfirmDialog` appears offering to add an alarm line.
3. Click **Add alarm line**.

**Expected:**
- A new alarm line entry is created with id = `lineId`, name = `Line <lineId>`, acquisition = Acoustic.
- Toast *"Alarm line added."* appears.
- Alarm line is visible in the alarm lines configuration.

---

### TC-AL-04 - New lineId → Offer to add alarm line, cancel - OK

**Steps:**
1. Same as TC-AL-03 up to the ConfirmDialog.
2. Click **Cancel** (or dismiss).

**Expected:** No alarm line is added. No error shown.

---

### TC-AL-05 - Backend rejects alarm line add - SKIPPED

**Precondition:** Simulate or force a backend error on the alarm line POST (e.g., temporarily disconnect or reach the maximum alarm line limit).

**Steps:**
1. Trigger TC-AL-03 conditions and confirm adding.

**Expected:** Toast *"Failed to add alarm line."* appears. Alarm line list is unchanged.

---

## Section 5 - Edge cases

---

### TC-EC-02 - Device with no readout → status section hidden - OK

**Precondition:** A device has never been acoustically read out (`readoutTime = 0` / absent).

**Steps:**
1. Open device details.

**Expected:** The **Status** section (device fault, battery, dirt forecast, warranty, radio status) is not shown. Readout field shows "Never".

---

### TC-EC-03 - Warranty flags set - OK

**Precondition:** A device with one or more warranty bits set in `warrantyFlags` (requires a real detector with voided warranty, or importing a device with `warrantyFlags > 0`).

**Steps:**
1. Open device details.

**Expected:** Warranty status shows **Voided** in red. Individual flag names are listed beneath it. Flags with bit = 1 are shown in red; others in green.

---

### TC-EC-04 - Radio state flags set - OK

**Precondition:** A device with non-zero `radioStateMask`.

**Steps:**
1. Open device details.

**Expected:** Each of the 8 radio state flags is listed. Active flags (bit = 1) are shown as error/active; inactive flags as neutral.

---

### TC-EC-05 - No radio module fitted - OK

**Precondition:** A device whose `radioModule.model` is `GeniusRadioModule.None` (or null).

**Steps:**
1. Open device details.

**Expected:** Message *"No radio module installed."* is shown. The radio module details section and Radio Status section are not rendered.

---

### TC-EC-06 - Import v0 backup, then perform readout - OK

**Precondition:** Export a v0-format backup (or manually craft one with `version` field absent). Import it via the import function (which should migrate it to v1).

**Steps:**
1. Import the v0 backup. Confirm the migration info dialog.
2. Verify devices appear with correct model values (SD model = 3, RM model = 4 where v0 had 0).
3. Perform a TC-RO-A1 readout on one of the migrated devices.

**Expected:** Readout succeeds and updates the device normally. No regression from migration.

---

### TC-EC-07 - Import backup with alarm history, existing device on backend - OK

**Precondition:** At least one device is registered with no alarm history. Export a backup that contains the same device ID but with alarm log entries, then clear the device list and re-import.

**Steps:**
1. Clear the device list (or use a fresh gateway).
2. Import the backup containing alarm log entries.
3. Confirm migration dialog if version differs, otherwise observe success toast.
4. Open device details.

**Expected:** Alarm log entries from the backup are present on the device. (Backend now accepts alarms for both new and existing devices.)

---

### TC-EC-08 - Cancel acoustic detection mid-transmission (synced/decoding state) - OK

**Steps:**
1. Click **Acoustic Device Detection**.
2. Hold detector close; wait for *"Audio signal detected!"* (synced) or *"Receiving data..."* (decoding).
3. Click **Cancel** while still decoding.

**Expected:** Dialog closes cleanly; detection is stopped; no partial data is applied; no crash.

---

### TC-EC-09 - Detection fails (error state) - OK

**Precondition:** Trigger acoustic detection but produce an invalid or incomplete signal (e.g., hold detector too far away, let it time out).

**Steps:**
1. Click **Acoustic Device Detection**.
2. Let the dialog reach the **error** state (*"Detection failed."* or a specific error message).

**Expected:** Dialog shows a red error circle and a **Close** button. No success path is triggered. No device data is changed.
