# HA Integration Test Suite - Detailed Reference

**Script:** `tests/test_ha_integration.py`  
**Total automated tests:** 65 (TC01–TC65)  
**Manual verification appendix:** see [Section 10](#section-10--manual-ha-ui-verification)

---

## How to run

```bash
# Install the one required dependency (once)
pip install requests

# REST-only (no HA verification, no token needed)
python tests/test_ha_integration.py

# Full run with HA entity state verification
python tests/test_ha_integration.py --ha-token "eyJ..."

# Filter to a specific category or test
python tests/test_ha_integration.py --ha-token "eyJ..." --filter "Fault"
python tests/test_ha_integration.py --filter "TC(07|08|09)"
python tests/test_ha_integration.py --filter "TC5[89]|TC6[0-5]"   # ordering only
```

## Parameters

| Flag | Default | Description |
|------|---------|-------------|
| `--gg-host` | _(prompted)_ | Genius Gateway IP or hostname |
| `--gg-password` | `admin` | Admin password |
| `--ha-host` | `homeassistant.local` | Home Assistant hostname |
| `--ha-port` | `8123` | Home Assistant port |
| `--ha-token` | _(empty)_ | Long-lived HA access token - HA assertions are skipped if omitted |
| `--filter` | `.*` | Regex against test names; only matching tests execute |

## Obtaining a Home Assistant long-lived access token

A long-lived access token authorises the test script to query entity states from the HA REST API. The token is tied to a specific HA user account and does not expire unless revoked.

**Steps:**

1. Open Home Assistant in a browser and sign in.
2. Click your **profile picture / username** in the bottom-left corner of the sidebar to open your profile page.
3. Scroll to the **Long-lived access tokens** section at the bottom of the page.
4. Click **Create token**, enter a descriptive name (e.g. `gg-integration-tests`), and confirm.
5. Copy the token immediately - it is shown **only once** and cannot be retrieved later.

**Use the token:**

```bash
python tests/test_ha_integration.py --ha-token "eyJ0eXAiOiJKV1Q..."
```

Or store it in an environment variable to avoid pasting it on every run:

```bash
# Linux / macOS
export HA_TOKEN="eyJ0eXAiOiJKV1Q..."
python tests/test_ha_integration.py --ha-token "$HA_TOKEN"

# Windows PowerShell
$env:HA_TOKEN = "eyJ0eXAiOiJKV1Q..."
python tests/test_ha_integration.py --ha-token $env:HA_TOKEN
```

If `--ha-token` is omitted entirely, the script will prompt for one interactively at startup. Press **Enter** without typing to skip HA verification and run REST-only.

> **Security:** Treat the token like a password. It grants full API access to your HA instance under the account that created it. Do not commit it to source control.

## Test device ID range

All test devices use IDs `900001–900200`. These IDs must not be used by real devices in the gateway. The cleanup function `reset_test_devices` identifies real devices as those **not** in this range and always restores the gateway to that baseline.

---

## State restoration guarantee

Every test that creates devices calls `reset_test_devices()` (no arguments) at the end. This function:

1. Calls `GET /rest/gateway-devices` and filters out all IDs in `900001–900200` → the _real device list_.
2. POSTs the real device list back, which removes all test devices from gateway memory and LittleFS storage.

Tests that skip early (e.g. TC47 when cap headroom is too small) do so _before_ any devices are added, so no cleanup is necessary.

**Result:** After every test completes (pass, fail, or exception), the gateway device list is identical to what it was before the test suite started - no test device survives into the next test.

> **Note for HA:** Device deletion triggers null-retained MQTT payloads on all discovery topics for that device. HA removes the corresponding entities automatically within seconds. There is no manual HA cleanup required between tests.

---

## Category 1 - Basic CRUD

### TC01 - Add acoustic device - round-trip

**What is tested:** A fully-populated acoustic device (all fields set) can be added via the REST API and all fields survive the POST→GET round-trip without loss or corruption.

**How the test is performed:**
1. Build a device fixture with `id=900001`, `location="Wohnzimmer"`, `registration=3` (Acoustic), a `readoutTime`, SD serial, and RM serial.
2. Call `reset_test_devices([d])` → POST the real device list plus this fixture.
3. Call `GET /rest/gateway-devices` and locate device 900001.
4. If `--ha-token` is provided: poll `binary_sensor.genius_900001_smoke` until it reaches `off` (up to 5 s).
5. Call `reset_test_devices()` to clean up.

**Expected result:**
- Device 900001 is present in the GET response.
- `location` = `"Wohnzimmer"`.
- `registration` = `3`.
- `readoutTime` is non-null.
- `smokeDetector.sn` and `radioModule.sn` match the posted values.
- HA: `binary_sensor.genius_900001_smoke` = `off`.

---

### TC02 - Add manual device (no readout)

**What is tested:** A manually-created device (no SmartSonic readout) is stored correctly and `readoutTime` is absent from the response.

**How the test is performed:**
1. Build a minimal manual device fixture with `id=900002`, `registration=2` (Manual), no `readoutTime`.
2. POST via `reset_test_devices([d])`.
3. GET and locate 900002.
4. HA check: `binary_sensor.genius_900002_smoke` = `off`.
5. Cleanup.

**Expected result:**
- `registration` = `2`.
- `readoutTime` is `null` / absent in the response.
- HA smoke sensor exists and is `off`.

---

### TC03 - Edit location of existing device

**What is tested:** Changing only the `location` field of an existing device is persisted correctly without side effects on other fields.

**How the test is performed:**
1. Add device 900003 with `location="OldName"` via `reset_test_devices`.
2. GET the full device list.
3. Mutate `location` to `"NewName"` in the Python dict.
4. POST the modified list via `set_devices`.
5. GET and verify the new location.
6. Cleanup.

**Expected result:**
- `location` = `"NewName"` in the GET response after the edit POST.

---

### TC04 - Edit readoutTime triggers full SD/RM update

**What is tested:** Firmware only merges updated SD status fields (e.g. `driftState`) when `readoutTime` is changed. This test verifies that changing `readoutTime` together with a status field causes the new value to be persisted.

**Background:** The firmware uses `readoutTime` as a version key. If the new `readoutTime` equals the stored one, status fields are not overwritten (prevents stale readout data from overwriting a newer readout).

**How the test is performed:**
1. Add device 900004 with default values (including `readoutTime = "2026-05-04T10:00:00.000Z"`, `driftState = 0`).
2. GET the full list, locate 900004, set `readoutTime = "2026-05-04T12:00:00.000Z"` and `smokeDetector.driftState = 5` in the Python dict.
3. POST via `set_devices`.
4. GET and verify `driftState`.
5. Cleanup.

**Expected result:**
- `smokeDetector.driftState` = `5` after the update.

---

### TC05 - Delete single device

**What is tested:** Removing one device from the list causes it to disappear from the GET response and its HA entities to become unavailable.

**How the test is performed:**
1. Add device 900005.
2. Verify it is present in GET.
3. Build the device list without ID 900005 and POST it.
4. GET and verify 900005 is absent.
5. HA check: `binary_sensor.genius_900005_smoke` = `unavailable` (null-retained MQTT payload sent by firmware causes HA to remove the entity; `unavailable` is how HA reports a removed or undiscovered entity).
6. Cleanup.

**Expected result:**
- Device 900005 absent in GET.
- HA entity `unavailable` (or absent if the entity was never known to HA).

---

### TC06 - Delete all test devices - state is empty

**What is tested:** POSTing only the real device list (no test devices) removes all test devices in one operation.

**How the test is performed:**
1. Add device 900006.
2. POST `get_real_devices()` (real devices only, no test devices).
3. GET and verify 900006 is gone.
4. Cleanup (no-op since already clean).

**Expected result:**
- Device 900006 absent.

---

## Category 2 - Fault State Round-Trips

### TC07 - `batteryLowFault=true` round-trips and HA reflects it

**What is tested:** Setting `smokeDetector.batteryLowFault = True` is stored and causes the corresponding HA binary sensor to report `on`.

**How the test is performed:**
1. Create device 900007 with `batteryLowFault = True` in the SD override.
2. POST via `reset_test_devices([d])`.
3. GET and check `smokeDetector.batteryLowFault`.
4. HA check: `binary_sensor.genius_900007_battery_low` = `on`.
5. Cleanup.

**Expected result:**
- `smokeDetector.batteryLowFault` = `true` in GET.
- HA `battery_low` entity = `on` (device class `battery`, HA renders amber/orange icon in detail dialog).

---

### TC08 - `deviceFault=true` round-trips

**What is tested:** `smokeDetector.deviceFault = True` is stored and surfaced in HA.

**How the test is performed:**
1. Create device 900008 with `deviceFault = True`.
2. POST, GET, verify flag.
3. HA check: `binary_sensor.genius_900008_device_fault` = `on`.
4. Cleanup.

**Expected result:**
- `smokeDetector.deviceFault` = `true`.
- HA `device_fault` entity = `on` (device class `problem`, HA renders orange `!` icon).

---

### TC09 - `radioNetworkFault=true` round-trips

**What is tested:** `radioModule.radioNetworkFault = True` is stored and surfaced in HA as the `radio_fault` entity.

**How the test is performed:**
1. Create device 900009 with `radioNetworkFault = True` in the RM override.
2. POST, GET, verify flag.
3. HA check: `binary_sensor.genius_900009_radio_fault` = `on`.
4. Cleanup.

**Expected result:**
- `radioModule.radioNetworkFault` = `true`.
- HA `radio_fault` entity = `on`.

---

### TC10 - All faults simultaneously

**What is tested:** All fault-related fields can be set at the same time without interfering with each other.

**Fields set:** `batteryLowFault=True`, `deviceFault=True`, `driftState=7`, `dirtForecastNegative=True`, `warrantyFlags=65535`, `radioNetworkFault=True`, `radioInterference=99.9`.

**How the test is performed:**
1. Create device 900010 with all fault fields set.
2. POST via `reset_test_devices([d])`.
3. GET and assert each field individually.
4. HA checks: `battery_low`, `device_fault`, `radio_fault` all = `on`.
5. Cleanup.

**Expected result:**
- All seven fields match their posted values in GET.
- All three HA binary sensors = `on`.

---

### TC11 - Clear fault - HA transitions from `on` to `off`

**What is tested:** After a fault is cleared (by advancing `readoutTime` and setting the flag to `False`), HA updates the binary sensor from `on` to `off`.

**How the test is performed:**
1. Create device 900011 with `batteryLowFault = True`.
2. POST and confirm HA `battery_low` = `on`.
3. GET the full list, locate 900011, set `readoutTime` to a later timestamp and `batteryLowFault = False` in the Python dict.
4. POST the updated list.
5. HA check: `binary_sensor.genius_900011_battery_low` = `off`.
6. Cleanup.

**Expected result:**
- HA `battery_low` transitions from `on` to `off` after the fault-clear POST.

---

## Category 3 - Readout & Availability States

### TC12 - Recent readout - HA diagnostics available

**What is tested:** A device with a current `readoutTime` has its diagnostic entities published to HA with the correct timestamp.

**How the test is performed:**
1. Create device 900012 with `readoutTime = "2026-05-04T10:00:00.000Z"`.
2. POST via `reset_test_devices([d])`.
3. HA check: `sensor.genius_900012_last_readout` = `"2026-05-04T10:00:00+00:00"` (HA normalises to `+00:00` offset notation).
4. Cleanup.

**Expected result:**
- HA `last_readout` sensor reflects the exact timestamp that was posted.

---

### TC13 - Overdue readout (>1 year old) - available but old date

**What is tested:** A device whose last readout is more than a year old is still available in HA (no automatic expiry), but its `last_readout` sensor shows the old date.

**How the test is performed:**
1. Create device 900013 with `readoutTime = "2024-01-10T08:00:00.000Z"`.
2. POST and GET - verify `readoutTime` is preserved and starts with `"2024-"`.
3. HA check: `sensor.genius_900013_last_readout` = `"2024-01-10T08:00:00+00:00"`.
4. Cleanup.

**Expected result:**
- `readoutTime` in GET starts with `"2024-"`.
- HA sensor shows the 2024 date, not a current or epoch date.

> **Manual follow-up (MV-01):** Open the HA device page and verify that the `last_readout` value is visually prominent and clearly indicates the readout is overdue.

---

### TC14 - No readout - HA diagnostics unavailable

**What is tested:** A device without any readout data (manual device, no `readoutTime`) has its diagnostic HA entities marked as `unavailable` because the MQTT availability payload is `offline` until a readout is performed.

**How the test is performed:**
1. Create a manual device 900014 with no `readoutTime`.
2. POST via `reset_test_devices([d])`.
3. HA checks:
   - `binary_sensor.genius_900014_battery_low` = `unavailable`
   - `binary_sensor.genius_900014_device_fault` = `unavailable`
   - `sensor.genius_900014_last_readout` = `unavailable`
4. Cleanup.

**Expected result:**
- All three diagnostic entities = `unavailable` in HA (not `on`/`off`).

---

## Category 4 - Device Models & Radio Configurations

### TC15 - No radio module (`GRM_NONE=0`, `sn=0`)

**What is tested:** A device with no radio module (`model=0`, `sn=0`) is stored correctly and the `radio_fault` HA entity still exists and reports `off` (not `unavailable`).

**How the test is performed:**
1. Create device 900015 with `radioModule = {"model": 0, "sn": 0}`.
2. POST, GET, verify `radioModule.model = 0` and `radioModule.sn = 0`.
3. HA check: `binary_sensor.genius_900015_radio_fault` = `off`.
4. Cleanup.

**Expected result:**
- RM model = `0`, sn = `0`.
- HA `radio_fault` = `off` (entity exists even without a physical radio module).

---

### TC16 - All smoke detector models (H, Hx, Plus, Plus X)

**What is tested:** All four valid `GeniusSmokeDetector` enum values (0–3) are stored and retrieved correctly.

**How the test is performed:**
1. Create four devices (IDs 900016–900019) with `smokeDetector.model` set to 0, 1, 2, 3 respectively.
2. POST all four via `reset_test_devices`.
3. GET and verify each device's `smokeDetector.model`.
4. Cleanup.

**Expected result:**
- Device 900016: `model = 0` (Genius H)
- Device 900017: `model = 1` (Genius Hx)
- Device 900018: `model = 2` (Genius Plus)
- Device 900019: `model = 3` (Genius Plus X)

> **Manual follow-up (MV-02):** Open the HA device page for each and verify that the model name is displayed correctly in the device info section.

---

### TC17 - All radio module models (None to FM Pro X)

**What is tested:** All six valid `GeniusRadioModule` enum values (0–5) are stored and retrieved correctly.

**How the test is performed:**
1. Create six devices (IDs 900020–900025) with `radioModule.model` 0–5 and matching serial numbers.
2. POST all six, GET, verify model and sn for each.
3. Cleanup.

**Expected result:**
- Each device returns the exact `model` and `sn` that was posted.

---

## Category 5 - Bulk & Import

### TC18 - Bulk add 10 devices - all persisted and published

**What is tested:** POSTing 10 devices in a single request causes all 10 to be persisted and retrievable. Tests that the firmware does not silently drop devices in a multi-device POST.

**How the test is performed:**
1. Create 10 device fixtures with IDs 900030–900039.
2. POST all 10 via `reset_test_devices`.
3. GET the full device list and check that each ID (900030–900039) is present.
4. Cleanup.

**Expected result:**
- All 10 IDs present in the GET response.

---

### TC19 - Import preserves field order and values

**What is tested:** A simulated web UI import (POST of real devices plus new imported devices) preserves specific field values (`warrantyFlags`, `driftState`) without corruption.

**How the test is performed:**
1. Build two import fixtures: 900040 with `warrantyFlags=7`, 900041 with `driftState=3`.
2. `set_devices(real + imports)` - simulates web UI import.
3. GET and verify each field.
4. Cleanup.

**Expected result:**
- Device 900040: `smokeDetector.warrantyFlags = 7`.
- Device 900041: `smokeDetector.driftState = 3`.

---

### TC20 - Re-import same IDs - updates, no duplicates

**What is tested:** Re-posting a device that already exists (same ID, changed `location`) updates the device in-place rather than creating a duplicate.

**How the test is performed:**
1. Add device 900042 with `location="Before Import"`.
2. GET the list, change `location` to `"After Import"` for 900042 in the Python dict.
3. POST the modified list.
4. GET and count occurrences of ID 900042, verify location.
5. Cleanup.

**Expected result:**
- Exactly 1 entry with ID 900042 in the GET response.
- `location = "After Import"`.

---

### TC21 - Import → delete all → re-import (full cycle)

**What is tested:** The complete add / remove / re-add lifecycle produces consistent results at each stage - no stale data or ghost entries survive the intermediate delete step.

**How the test is performed:**
1. Add device 900043 → assert it is present.
2. POST real-only list → assert 900043 is absent.
3. Re-add device 900043 → assert it is present again.
4. Cleanup.

**Expected result:**
- Each of the three assertions passes independently.

---

## Category 6 - Edge Cases

### TC22 - Location with ASCII special characters and numbers

**What is tested:** A `location` string containing hyphens, parentheses, brackets, and numbers survives the POST→GET round-trip without escaping or truncation.

**How the test is performed:**
1. Add device 900050 with `location = "Room-01 (2nd Floor) [North]"`.
2. GET and compare.
3. Cleanup.

**Expected result:**
- `location = "Room-01 (2nd Floor) [North]"` - character-for-character identical.

---

### TC23 - Location with German umlauts (UTF-8)

**What is tested:** Non-ASCII UTF-8 characters (ü, Ü) in `location` are accepted by the REST API. The REST round-trip is expected to be intact; however, MQTT/HA may render replacement characters (known firmware limitation with multi-byte UTF-8 in the MQTT payload).

**How the test is performed:**
1. Add device 900051 with `location = "Küche Über Erdgeschoss"`.
2. GET and verify `location` field is non-null (content check is lenient due to known encoding issue).
3. Cleanup.

**Expected result:**
- `location` field is present and non-null.
- REST round-trip may or may not preserve the exact characters; test is marked pass as long as the device is stored and returned.

> **Manual follow-up (MV-03):** Check the HA device name for 900051 - it may display as `K?che` or similar. This documents the known UTF-8 limitation in the MQTT layer.

---

### TC24 - Empty location string

**What is tested:** `location = ""` is a valid value and the device is stored and retrievable.

**How the test is performed:**
1. Add device 900052 with `location = ""`.
2. GET and verify device is present.
3. Cleanup.

**Expected result:**
- Device 900052 present in GET (empty location accepted).

---

### TC25 - `warrantyFlags` at all significant bit positions

**What is tested:** The full range of `warrantyFlags` values (0 to 65535, covering individual bits, byte boundaries, and the maximum) round-trips correctly as a `uint16_t`.

**Values tested:** `0, 1, 2, 4, 128, 255, 32768, 65535`

**How the test is performed:**
1. Create 8 devices (IDs 900053–900060) with one `warrantyFlags` value each.
2. POST all 8 via `reset_test_devices`.
3. GET and compare each value individually.
4. Cleanup.

**Expected result:**
- Each device returns the exact `warrantyFlags` value that was posted.

---

### TC26 - `driftState` all values 0–7

**What is tested:** All eight valid `driftState` values (0–7, a 3-bit field) round-trip correctly.

**How the test is performed:**
1. Create 8 devices (IDs 900061–900068) with `driftState` 0–7.
2. POST, GET, and verify each.
3. Cleanup.

**Expected result:**
- Each of the 8 devices returns the exact `driftState` value that was posted.

---

### TC27 - `radioStateMask` all-bits set (255)

**What is tested:** The maximum value of the `radioStateMask` bitmask field (`uint8_t`, all 8 bits set = 255) is stored correctly.

**How the test is performed:**
1. Create device 900069 with `radioStateMask = 255`.
2. POST, GET, verify.
3. Cleanup.

**Expected result:**
- `radioModule.radioStateMask = 255`.

---

### TC28 - `radioInterference` boundary values (0, 50, 100)

**What is tested:** The `radioInterference` float field stores values at 0%, 50%, and 100% correctly (the three key boundary points of the percentage scale).

**How the test is performed:**
1. Create 3 devices (IDs 900070–900072) with `radioInterference` 0.0, 50.0, 100.0.
2. POST, GET, verify 50.0 and 100.0 (0.0 is the default and less interesting to assert).
3. Cleanup.

**Expected result:**
- Device 900071: `radioInterference = 50.0`.
- Device 900072: `radioInterference = 100.0`.

> **Manual follow-up (MV-04):** Open the HA device page for 900072 and verify `radio_interference` sensor shows `100 %`.

---

### TC29 - Device with multiple alarms - all preserved

**What is tested:** A device can store multiple alarm entries and all of them - including specific `endingReason` values - survive the round-trip intact.

**Alarm entries posted:**
1. `2025-01-10 08:00–08:05`, `endingReason=0` (ended by smoke detector)
2. `2025-06-15 14:30–14:35`, `endingReason=1` (ended manually)
3. `2026-02-20 03:22–03:28`, `endingReason=0`

**How the test is performed:**
1. Create device 900073 with all three alarm entries.
2. POST, GET, verify `len(alarms) = 3` and `alarms[1].endingReason = 1`.
3. Cleanup.

**Expected result:**
- `alarms` array contains exactly 3 entries.
- Second alarm has `endingReason = 1`.

---

### TC30 - Dates without milliseconds parsed correctly (Utils.cpp fix)

**What is tested:** Regression test for the `Utils::iso8601_to_time_t` fix. Before the fix, timestamps without `.000Z` milliseconds were parsed as `time_t = -1` and serialised as `1969-12-31T23:59:59.000Z`. The test fixture uses `.000Z` format and verifies the readoutTime does not regress to an epoch value.

**How the test is performed:**
1. Verify the fixture `readoutTime` ends in `.000Z` (guards against fixture change).
2. Create device 900074, POST, GET.
3. Assert `readoutTime` starts with `"2026-"` (not `"1969-"` or `"1970-"`).
4. Cleanup.

**Expected result:**
- `readoutTime` starts with `"2026-"`.

---

### TC31 - `isAlarming=true` reflected in HA smoke sensor

**What is tested:** A new device created with `isAlarming=True` causes the HA smoke sensor to report `on`.

**Important note:** `isAlarming` is managed internally by the alarm detection system. This flag is respected when set on a **new** device via POST, but is ignored (preserved from internal state) for **existing** devices on subsequent updates.

**How the test is performed:**
1. Create device 900075 with `isAlarming = True`.
2. POST via `reset_test_devices([d])` (creates as new device).
3. HA check: `binary_sensor.genius_900075_smoke` = `on`.
4. Cleanup.

**Expected result:**
- HA `smoke` entity = `on`.

---

### TC32 - `deinstallationCount` and `alarmCountTotal` preserved

**What is tested:** The counter fields `deinstallationCount`, `alarmCountTotal`, and `alarmCountLast3Months` are stored correctly and their values are surfaced in HA sensor entities.

**How the test is performed:**
1. Create device 900076 with `deinstallationCount=7`, `alarmCountTotal=12`, `alarmCountLast3Months=3`.
2. POST, GET, verify all three counters.
3. HA checks:
   - `sensor.genius_900076_deinstallation_count` = `"7"`
   - `sensor.genius_900076_alarm_count_total` = `"12"`
4. Cleanup.

**Expected result:**
- All three counter values match in GET.
- HA sensors show the correct numeric values.

---

### TC33 - Registration type never overwritten on edit

**What is tested:** The `registration` field is write-once on device creation. Subsequent POSTs that include a different `registration` value for an existing device must be ignored by the firmware.

**Background:** Registration type reflects how the device was discovered (Built-in=0, Genius Packet=1, Manual=2, Acoustic=3). This must not change retroactively when the Web UI saves an edited device.

**How the test is performed:**
1. Create device 900077 with `registration=3` (Acoustic).
2. GET, verify `registration = 3`.
3. GET the full list, set `registration = 2` on device 900077 in the Python dict, POST.
4. GET again and assert `registration` is still `3`.
5. Cleanup.

**Expected result:**
- `registration` remains `3` after the edit POST that attempted to change it to `2`.

---

### TC34 - `lineCharacter` valid values (A–J)

**What is tested:** A valid `lineCharacter` value in the accepted range (`A`–`J`) is stored and retrieved correctly together with `lineNumber`.

**How the test is performed:**
1. Create device 900078 with `lineCharacter="F"` and `lineNumber=3`.
2. POST, GET, verify both fields.
3. Cleanup.

**Expected result:**
- `radioModule.lineCharacter = "F"`.
- `radioModule.lineNumber = 3`.

---

## Category 7 - Resilience (Basic)

### TC35 - POST with `version` field - accepted

**What is tested:** The optional root-level `version` field is accepted without error and does not cause unexpected behaviour.

**How the test is performed:**
1. Create device 900080.
2. POST `{"version": 1, "devices": real + [d]}` directly via `invoke_gg`.
3. GET and verify 900080 is present.
4. Cleanup.

**Expected result:**
- Device 900080 stored correctly despite the `version` field.

---

### TC36 - POST empty devices array - clears test devices

**What is tested:** Posting a list that contains only real devices (and no test devices) effectively deletes the test device. Verifies that a reductive POST is processed atomically.

**How the test is performed:**
1. Add device 900081.
2. Verify it is present.
3. `set_devices(get_real_devices())` - POST with test device excluded.
4. GET and verify 900081 is absent.
5. Cleanup (no-op).

**Expected result:**
- Device 900081 absent after the reductive POST.

---

### TC37 - Repeated identical POST - idempotent

**What is tested:** POSTing the exact same device list twice does not create duplicates or change the device count.

**How the test is performed:**
1. Add device 900082.
2. Record the total device count.
3. POST the same list again via `set_devices(get_devices())`.
4. GET and compare the count.
5. Cleanup.

**Expected result:**
- Device count after second POST equals device count after first POST.

---

## Category 8 - Missing & Invalid Data

### TC38 - POST without `devices` key - state unchanged

**What is tested:** A POST body that contains no `devices` key is handled gracefully. The firmware is expected to return `UNCHANGED` and leave the device list intact.

**How the test is performed:**
1. Record the current device count.
2. POST `{"version": 1}` (no `devices` key) via `invoke_gg`.
3. GET and compare the count.

> No cleanup needed - this test adds no devices.

**Expected result:**
- Device count is identical before and after the POST.

---

### TC39 - Device with `id=0` - accepted and retrievable

**What is tested:** A device ID of zero (the default when `id` is missing from JSON) is accepted and the device can be retrieved.

**How the test is performed:**
1. Build a full device fixture with `id=0`.
2. `set_devices(real + [d])` - manual POST (not via `reset_test_devices` since ID 0 is outside the `MOCK_IDS` range used by that helper).
3. GET and filter for `id = 0`.
4. Restore: `set_devices(real)`.

**Expected result:**
- A device with `id = 0` is present in GET.

---

### TC40 - Duplicate IDs in same POST - firmware stores both (no dedup)

**What is tested:** When two device objects in the same POST payload share the same ID, the firmware stores both entries without deduplication.

**How the test is performed:**
1. Build two fixtures both with `id=900083`: one with `location="First"`, one with `location="Second"`.
2. POST `real + [first, second]` - both objects in the same array.
3. GET, count occurrences of ID 900083.
4. Cleanup.

**Expected result:**
- Exactly 2 entries with ID 900083 (firmware applies no deduplication on POST).

---

### TC41 - Device without `smokeDetector` field - defaults applied

**What is tested:** Omitting the entire `smokeDetector` object from a device payload does not crash the firmware; ArduinoJson applies zero-value defaults.

**How the test is performed:**
1. POST a device object containing only `id`, `location`, `isAlarming`, `registration`, `alarms` - no `smokeDetector` key.
2. GET device 900084.
3. Verify `smokeDetector.model = -1` (GSD_UNKNOWN) and `smokeDetector.sn = 0`.
4. Cleanup.

**Expected result:**
- Device stored without crash.
- `smokeDetector.model = -1`, `sn = 0` (ArduinoJson defaults).

---

### TC42 - Device without `radioModule` field - defaults applied

**What is tested:** Omitting the `radioModule` object does not crash the firmware; defaults are applied.

**How the test is performed:**
1. POST a device with `smokeDetector` but no `radioModule`.
2. GET device 900085.
3. Verify `radioModule.model = -1`.
4. Cleanup.

**Expected result:**
- Device stored.
- `radioModule.model = -1` (GRM_UNKNOWN default).

---

### TC43 - `null` location - stored as empty or null

**What is tested:** Sending `"location": null` (JSON null) does not crash the firmware. ArduinoJson coerces null to an empty string for `String` fields.

**How the test is performed:**
1. POST a device with `location = None` (Python `None` serialises to JSON `null`).
2. GET device 900086 and verify the device is present (field value not asserted strictly).
3. Cleanup.

**Expected result:**
- Device stored without error.
- `location` field is present (empty string or null).

---

### TC44 - Out-of-range SD model enum (`-999` and `99`) - no crash

**What is tested:** The firmware casts the JSON integer to the `GeniusSmokeDetector` enum without range validation. Values outside the defined range (`-1` to `3`) should be stored and the firmware must not crash or refuse the POST.

**How the test is performed:**
1. Create devices 900087 (`model=-999`) and 900088 (`model=99`).
2. POST both.
3. GET and verify both devices are present.
4. Cleanup.

**Expected result:**
- Both devices stored.
- No HTTP error returned.

> **Note:** The stored `model` value may or may not round-trip correctly depending on ArduinoJson's enum serialisation. The test only asserts no crash and device presence.

---

### TC45 - Invalid ISO8601 timestamp - stored as epoch or absent (no crash)

**What is tested:** Sending `readoutTime = "not-a-date"` and `productionDate = "2022/01/15"` (wrong separator) is handled gracefully. `Utils::iso8601_to_time_t` returns `0` for unparseable strings; `0` is treated as "no date" and either omitted from the response or serialised as a near-epoch timestamp.

**How the test is performed:**
1. Create device 900089, override `readoutTime` and `productionDate` with invalid strings.
2. POST via `reset_test_devices`.
3. GET: assert device exists.
4. If `readoutTime` is non-null in the response, assert it does **not** start with `"2026-"` (the garbage string must not be stored as a valid future date).
5. Cleanup.

**Expected result:**
- Device stored without crash.
- `readoutTime` is either absent/null or shows a near-epoch value, not a future date.

---

### TC46 - Partial ISO8601 (date only, no time) - graceful handling

**What is tested:** A `productionDate` formatted as `"2022-01-15"` (missing `T` and `Z` components) does not cause a crash. This is a common mistake when manually constructing import data.

**How the test is performed:**
1. Create device 900090, set `smokeDetector.productionDate = "2022-01-15"`.
2. POST, GET, assert device is present.
3. Cleanup.

**Expected result:**
- Device stored without HTTP error or firmware crash.

---

### TC47 - Max device limit (50) - 51st device dropped

**What is tested:** The firmware enforces a hard cap of 50 devices (`GATEWAY_MAX_DEVICES`). Posting a 51st device should result in the excess being silently dropped.

**Precondition:** Requires at least 5 free slots (50 − number of real devices ≥ 5). The test is skipped automatically if the cap headroom is insufficient.

**How the test is performed:**
1. Calculate available slots: `50 − realCount`.
2. Create exactly enough devices to fill to 50 total, POST.
3. Assert `len(get_devices()) ≤ 50`.
4. Attempt to POST one additional device (ID 900150).
5. Assert `len(get_devices()) ≤ 50` (cap enforced).
6. Cleanup.

**Expected result:**
- After filling to 50: count ≤ 50.
- After adding 51st: count still ≤ 50.

---

### TC48 - More than 100 alarms per device - excess silently dropped

**What is tested:** The firmware enforces a per-device alarm cap of 100 (`GATEWAY_MAX_ALARMS`). Posting 110 alarms should result in the first 100 being stored.

**How the test is performed:**
1. Build 110 alarm entries with incrementing timestamps.
2. Create device 900151 with all 110 alarms.
3. POST via `reset_test_devices([d])`.
4. GET device 900151, assert `len(alarms) ≤ 100`.
5. Cleanup.

**Expected result:**
- Device stored.
- `len(alarms) ≤ 100` (excess dropped).

---

### TC49 - `lineCharacter` invalid values - rejected (stored as 0)

**What is tested:** Only characters `A`–`J` are valid for `lineCharacter` (the firmware has explicit validation). Characters outside this range are normalised to `0` (null char).

**Values tested:** `"Z"` (alphabetic but out of range), `"!"` (non-alphabetic), `"1"` (digit).

**How the test is performed:**
1. Create 3 devices (900152–900154) with the three invalid characters.
2. POST via `reset_test_devices`.
3. GET each device, assert `radioModule.lineCharacter` is `None`, `""`, `0`, or `"0"`.
4. Cleanup.

**Expected result:**
- None of the three invalid characters are stored as-is.
- `lineCharacter` is absent or zero for all three devices.

---

### TC50 - `driftState` out of range (8 and 255) - uint8 behaviour documented

**What is tested:** `driftState` is a `uint8_t` field and valid only in 0–7. Values 8 and 255 exceed the defined range but are within uint8 capacity. The test documents the firmware's behaviour (no validation; stored and returned as-is).

**How the test is performed:**
1. Create devices 900155 (`driftState=8`) and 900156 (`driftState=255`).
2. POST both, GET, assert both devices are present.
3. Cleanup.

**Expected result:**
- Both devices stored without error.
- No assertion on the actual stored value (behaviour is documented, not enforced).

---

### TC51 - `alarmCountTotal=300` overflow (> uint8 max 255) - truncated

**What is tested:** `alarmCountTotal` is a `uint8_t`. Posting `300` causes ArduinoJson to truncate the value to the uint8 range (either `300 % 256 = 44` by modular arithmetic, or `255` if clamped).

**How the test is performed:**
1. Create device 900157 with `alarmCountTotal=300`.
2. POST, GET, assert `smokeDetector.alarmCountTotal ≤ 255`.
3. Cleanup.

**Expected result:**
- Device stored.
- `alarmCountTotal ≤ 255`.

---

### TC52 - Extra unknown fields in device object - silently ignored

**What is tested:** Sending additional JSON keys that are not part of the firmware schema (e.g. for future compatibility or import from a newer version) does not cause an error and known fields are unaffected.

**How the test is performed:**
1. Create device 900158, add extra keys `unknownFoo="bar"` and `futureField=42` to the Python dict.
2. POST (these extra keys are serialised into the JSON body).
3. GET, assert device present, assert `location` is intact.
4. Cleanup.

**Expected result:**
- Device stored without HTTP error.
- `location` field unchanged.

---

### TC53 - Extra unknown fields at root level - silently ignored

**What is tested:** An unexpected key at the root of the POST body (e.g. `unexpectedRootKey`) does not cause the request to fail.

**How the test is performed:**
1. Build body `{"version": 1, "devices": [...], "unexpectedRootKey": "hello"}` and POST via `invoke_gg`.
2. GET device 900159, assert present.
3. Cleanup.

**Expected result:**
- Device 900159 stored.
- No HTTP error for the unknown root key.

---

### TC54 - Alarm with missing `startTime`/`endTime` - no crash

**What is tested:** An alarm entry that contains only `endingReason` (both `startTime` and `endTime` missing) does not crash the firmware. The device should either be stored with a zero-timestamp alarm or with the alarm dropped.

**How the test is performed:**
1. Create device 900160 with `alarms = [{"endingReason": 0}]`.
2. POST wrapped in a `try/except` to detect HTTP exceptions.
3. GET device 900160, assert present.
4. Cleanup.

**Expected result:**
- No exception thrown by the HTTP call.
- Device 900160 stored (alarm may be absent or have zero timestamps).

---

### TC55 - Alarm `endingReason=-1` (AlarmActive) - stored correctly

**What is tested:** The `GAE_ALARM_ACTIVE = -1` enum value for `endingReason` (meaning the alarm is still active / has no defined end) is stored and round-trips correctly.

**How the test is performed:**
1. Create device 900161 with a single alarm with `endingReason=-1`.
2. POST, GET, assert `alarms[0].endingReason = -1`.
3. Cleanup.

**Expected result:**
- `alarms[0].endingReason = -1`.

---

### TC56 - Negative `radioInterference` - stored as-is (no clamping)

**What is tested:** The firmware does not validate the range of `radioInterference`. A negative value (`-5.5`) is stored without modification or rejection.

**How the test is performed:**
1. Create device 900162 with `radioInterference=-5.5`.
2. POST, GET, assert device is present and `radioInterference` is non-null.
3. Cleanup.

**Expected result:**
- Device stored.
- `radioInterference` field is present (actual stored value is implementation-dependent; test documents no-crash behaviour).

---

### TC57 - `readoutProtocolVersion` boundary values (0, 1, 255)

**What is tested:** `readoutProtocolVersion` is a `uint8_t`. Values `0`, `1`, and `255` (the boundaries of the type) all round-trip correctly.

**How the test is performed:**
1. Create devices 900163–900165 with `readoutProtocolVersion` 0, 1, 255 respectively.
2. POST, GET, verify each.
3. Cleanup.

**Expected result:**
- `readoutProtocolVersion = 0`, `1`, `255` respectively.

---

## Category 9 - Device Ordering (Draggable List)

The web UI presents devices in a drag-and-drop list. The device order is submitted by posting the full device array in the desired sequence. The firmware preserves this order in its internal `std::vector` and in LittleFS storage, and detects order changes by comparing ID sequences.

### TC58 - Insertion order preserved - GET returns same order as POST

**What is tested:** The fundamental ordering guarantee: the sequence of devices in a GET response matches the sequence that was POSTed.

**How the test is performed:**
1. POST 5 devices (IDs 900170–900174) in this exact order.
2. GET, filter to test devices, extract IDs.
3. Assert `returned[0]=900170`, `[1]=900171`, …, `[4]=900174`.
4. Cleanup.

**Expected result:**
- Each position in the returned array matches the posted sequence.

---

### TC59 - Reorder devices - GET reflects new order

**What is tested:** After reversing the order of existing devices via a new POST, the GET response reflects the reversed order.

**How the test is performed:**
1. POST devices 900175, 900176, 900177 in this order.
2. GET test devices, reverse the list in Python.
3. POST the full list with reversed test devices.
4. GET test devices, assert order is 900177, 900176, 900175.
5. Cleanup.

**Expected result:**
- `returned[0] = 900177`, `[1] = 900176`, `[2] = 900175`.

---

### TC60 - Insert new device at beginning of list

**What is tested:** A new device prepended to the front of the existing list appears at position 0 in the subsequent GET, and the previously first device moves to position 1.

**How the test is performed:**
1. POST existing devices 900178, 900179, 900180.
2. Build new list: `[new_dev] + test_devs` (new device first).
3. POST the full list.
4. GET test devices, assert `returned[0] = 900181`, `returned[1] = 900178`.
5. Cleanup.

**Expected result:**
- Position 0: 900181 (new).
- Position 1: 900178 (previously first).

---

### TC61 - Insert new device at end of list

**What is tested:** A new device appended to the end of the list appears last, and the previously last device is at second-to-last position.

**How the test is performed:**
1. POST existing devices 900182, 900183, 900184.
2. Build new list: `test_devs + [new_dev]` (new device last).
3. POST and GET.
4. Assert `returned[-1] = 900185`, `returned[-2] = 900184`.
5. Cleanup.

**Expected result:**
- Last position: 900185 (new).
- Second-to-last: 900184 (previously last).

---

### TC62 - Insert new device in middle of list

**What is tested:** A device inserted between positions 1 and 2 of a 4-device list lands at position 2, and the device previously at position 2 shifts to position 3.

**How the test is performed:**
1. POST 4 devices: 900186, 900187, 900188, 900189.
2. Build new list: `[900186, 900187] + [900190] + [900188, 900189]`.
3. POST and GET.
4. Assert `returned[2] = 900190`, `returned[3] = 900188`.
5. Cleanup.

**Expected result:**
- Position 2: 900190 (inserted).
- Position 3: 900188 (shifted right).

---

### TC63 - Move device from last to first

**What is tested:** The last device in the list can be moved to the first position. This is the most disruptive ordering operation (maximum shift for all other devices).

**How the test is performed:**
1. POST 4 devices: 900191, 900192, 900193, 900194.
2. Take the last device (900194), prepend it: `[last] + rest`.
3. POST and GET.
4. Assert `returned[0] = 900194`, `returned[1] = 900191`.
5. Cleanup.

**Expected result:**
- Position 0: 900194 (moved from last).
- Position 1: 900191 (shifted right by one).

---

### TC64 - Order change persisted across two GETs

**What is tested:** A reorder is durably written to LittleFS (not just in-memory). Two consecutive GETs - with a 300 ms pause between them - return identical order.

**How the test is performed:**
1. POST 3 devices: 900195, 900196, 900197.
2. Reverse and POST.
3. GET immediately → `returned1`.
4. Wait 300 ms.
5. GET again → `returned2`.
6. Assert `returned2[0] = returned1[0]` and `returned2[2] = returned1[2]`.
7. Cleanup.

**Expected result:**
- Both GETs agree on the device order.

> **Note:** This test cannot definitively verify filesystem persistence (a reboot would be needed for that), but it does rule out in-memory inconsistency.

---

### TC65 - Reorder-only POST - content unchanged

**What is tested:** A POST that only changes device order (no field values modified) does not corrupt any device data.

**How the test is performed:**
1. POST 3 devices (900196–900198) all with `warrantyFlags=42`.
2. Reverse the order and POST again without touching any field values.
3. GET each device by ID and assert `warrantyFlags = 42`.
4. Cleanup.

**Expected result:**
- `warrantyFlags = 42` on all three devices after the reorder POST.

---

## Section 10 - Manual HA UI Verification

The following checks require a human to look at the Home Assistant interface. They cannot be automated via the REST API.

### MV-01 - Overdue readout visual prominence

**Precondition:** TC13 has been run (or a device with a 2024 `readoutTime` exists in HA).

**Procedure:**
1. Open HA → Devices → find a device with an old `last_readout` date.
2. Note whether the UI highlights the old date in any way (e.g. colour, warning icon).

**Expected result:**
- The `last_readout` value is clearly legible and shows the 2024 date.
- Note: HA does not natively highlight "stale" sensor values; consider a Mushroom card template for visual emphasis.

---

### MV-02 - Smoke detector model name in HA device info

**Precondition:** TC16 has been run (or devices with different SD models exist).

**Procedure:**
1. Open HA → Devices → select a Genius Plus X device (model=3).
2. Check the device info panel.

**Expected result:**
- The model name `"Genius Plus X"` (or equivalent) appears in the device info.
- Verify against all four models.

---

### MV-03 - UTF-8 umlaut in device name (known limitation)

**Precondition:** TC23 has been run.

**Procedure:**
1. Open HA → Devices → find the device with location `"Küche Über Erdgeschoss"`.
2. Note the displayed name.

**Expected result:**
- REST round-trip preserves the umlauts.
- HA device name may display `K?che` or `K<?>che` - this is the known MQTT UTF-8 encoding limitation.
- Document the actual rendering for the changelog.

---

### MV-04 - Radio interference at 100 % shown in HA

**Precondition:** TC28 has been run, or device 900072 with `radioInterference=100` is present.

**Procedure:**
1. Open HA → Devices → select device 900072.
2. Locate the `radio_interference` sensor.

**Expected result:**
- Value displayed as `100 %` (or `100.0 %`).

---

### MV-05 - Fault state visual in HA entity list vs. detail dialog

**Precondition:** A device with `deviceFault=True` or `batteryLowFault=True` is present.

**Procedure:**
1. Open HA → Devices → select a faulted device.
2. Observe the `device_fault` or `battery_low` entity in the entity list.
3. Click on the entity to open the detail dialog.

**Expected result:**
- **Entity list:** Orange `!` badge visible on the problem entity row (background tint).
- **Detail dialog:** Orange/red colour on the state timeline bars for `on` periods; `on` state label in orange.
- The entity list color change is subtle (row tint only); the detail dialog provides the clearest visual differentiation.

---

### MV-06 - Unavailable diagnostic entities (no readout)

**Precondition:** TC14 has been run, or a manual device (no `readoutTime`) is present.

**Procedure:**
1. Open HA → Devices → select a manual device.
2. Check the diagnostic entities.

**Expected result:**
- `battery_low`, `device_fault`, `radio_fault`, `last_readout`, etc. all show as **Unavailable**.
- The smoke sensor itself should still be `off` (it has its own availability independent of the readout).

---

### MV-07 - HA device page completeness

**Precondition:** Any fully-populated acoustic device exists in HA.

**Procedure:**
1. Open HA → Devices → select a Genius Plus X acoustic device.
2. Verify the entities listed under the device.

**Expected result:**
The device page should contain:
- 1 × binary sensor: Smoke (main alert entity)
- Binary sensors (diagnostic): Battery Low, Device State, Radio Module State
- Sensors (diagnostic): Last Readout, Production Date, Deinstallation Count, Alarm Count Total, Alarm Count Last 3 Months, Hours in Storage Mode, Warranty Flags, Drift State, Dirt Forecast, Radio Interference
- Total: approximately 13–14 entities per device.

---

### MV-08 - Alarm in HA history

**Precondition:** TC29 has been run, or a device with alarms exists.

**Procedure:**
1. Open HA → History.
2. Search for the `smoke` binary sensor of the device with alarms.
3. Adjust the time range to cover the alarm timestamps from the fixture.

**Expected result:**
- HA history shows `on` spikes at the alarm timestamps.
- Note: HA history only records state changes received by the MQTT broker while HA was running. Historical alarms from the import may not appear (they are not re-published as MQTT state changes).

---

## Automation coverage summary

| Layer | Automated | Notes |
|-------|-----------|-------|
| REST API - device CRUD and field round-trips | Yes (100 %) | TC01–TC65 |
| HA entity state (`on`/`off`/`unavailable`) | Yes, when `--ha-token` is provided | Polls with 60 s retry |
| MQTT retained discovery payloads | No | Requires `mosquitto_sub` or `paho-mqtt` |
| HA visual rendering (colours, icons, history) | No - manual | See Section 10 |
