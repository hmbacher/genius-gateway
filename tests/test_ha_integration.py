#!/usr/bin/env python3
"""
Integration tests for the Genius Gateway HA/MQTT device integration.

Tests the full device lifecycle (add, edit, delete, import) via the REST API
and optionally verifies HA entity states via the HA REST API.

Layers tested:
  [AUTO]  REST API  - device CRUD, import, field round-trips
  [AUTO]  HA API    - entity state/availability (requires --ha-token)

Usage:
  # Interactive prompts for all settings
  python test_ha_integration.py

  # REST-only (no HA checks, no prompts)
  python test_ha_integration.py --gg-host 192.168.178.133 --gg-password admin

  # Full run with HA verification (no prompts)
  python test_ha_integration.py --gg-host 192.168.178.133 --gg-password admin \
      --ha-host 192.168.178.10 --ha-port 8123 --ha-user Hans --ha-token "eyJ..."

  # Filtered run
  python test_ha_integration.py --gg-host 192.168.178.133 --gg-password admin \
      --ha-host 192.168.178.10 --ha-token "eyJ..." --filter "TC0[789]"

  # Entity-ID rename tests on a gateway with real detectors (renames their HA entity IDs!)
  python test_ha_integration.py --gg-host 192.168.178.133 --gg-password admin \
      --ha-host 192.168.178.10 --ha-token "eyJ..." --filter "TC8[1-7]" --rename-entity-ids

Requires:
  pip install requests
"""

import argparse
import re
import sys
import time

try:
    import requests
except ImportError:
    print("Missing dependency: pip install requests")
    sys.exit(1)

# ---------------------------------------------------------------------------
# ANSI colour support
# ---------------------------------------------------------------------------

def _enable_ansi_on_windows():
    try:
        import ctypes
        ctypes.windll.kernel32.SetConsoleMode(
            ctypes.windll.kernel32.GetStdHandle(-11), 7)
    except Exception:
        pass

if sys.platform == "win32":
    _enable_ansi_on_windows()

GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
WHITE  = "\033[97m"
RESET  = "\033[0m"

# ---------------------------------------------------------------------------
# Global state (populated by main() before tests run)
# ---------------------------------------------------------------------------

_pass_count  = 0
_fail_count  = 0
_skip_count  = 0
_jwt         = None
_gg_base     = ""
_ha_base     = ""
_ha_snapshot: list = []
_gg_password = ""
_ha_token    = ""
_filter_re   = ".*"

MOCK_IDS = set(range(900001, 900201))

# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------

def write_pass(msg: str) -> None:
    global _pass_count
    _pass_count += 1
    print(f"  {GREEN}[PASS]{RESET} {msg}")

def write_fail(msg: str) -> None:
    global _fail_count
    _fail_count += 1
    print(f"  {RED}[FAIL]{RESET} {msg}")

def write_skip(msg: str) -> None:
    global _skip_count
    _skip_count += 1
    print(f"  {YELLOW}[SKIP]{RESET} {msg}")

def write_section(msg: str) -> None:
    print(f"\n{CYAN}=== {msg} ==={RESET}")

# ---------------------------------------------------------------------------
# Assertion helpers
# ---------------------------------------------------------------------------

def assert_equal(actual, expected, label: str) -> None:
    if actual == expected:
        write_pass(label)
    else:
        write_fail(f"{label}  expected=[{expected!r}]  actual=[{actual!r}]")

def assert_true(cond, label: str) -> None:
    if cond:
        write_pass(label)
    else:
        write_fail(label)

def assert_null(val, label: str) -> None:
    if val is None:
        write_pass(label)
    else:
        write_fail(f"{label}  (expected null, got [{val!r}])")

def assert_not_null(val, label: str) -> None:
    if val is not None:
        write_pass(label)
    else:
        write_fail(f"{label}  (expected non-null)")

# ---------------------------------------------------------------------------
# GG REST API
# ---------------------------------------------------------------------------

def _gg_sign_in() -> None:
    global _jwt
    r = requests.post(
        f"{_gg_base}/rest/signIn",
        json={"username": "admin", "password": _gg_password},
    )
    r.raise_for_status()
    _jwt = r.json()["access_token"]

def invoke_gg(method: str, path: str, body=None):
    global _jwt
    if _jwt is None:
        _gg_sign_in()
    headers = {"Authorization": f"Bearer {_jwt}"}
    url = f"{_gg_base}{path}"
    if body is not None:
        resp = requests.request(method, url, headers=headers, json=body)
    else:
        resp = requests.request(method, url, headers=headers)
    resp.raise_for_status()
    return resp.json()

def invoke_gg_raw(method: str, path: str, body=None):
    """Like invoke_gg() but returns the raw Response without raise_for_status() so tests
    can assert on non-2xx status codes (409 BUSY, 410 BAD_SESSION, etc.)."""
    global _jwt
    if _jwt is None:
        _gg_sign_in()
    headers = {"Authorization": f"Bearer {_jwt}"}
    url = f"{_gg_base}{path}"
    if body is not None:
        return requests.request(method, url, headers=headers, json=body)
    return requests.request(method, url, headers=headers)

def get_devices() -> list:
    return invoke_gg("GET", "/rest/gateway-devices").get("devices", [])

def set_devices(devs: list) -> None:
    invoke_gg("POST", "/rest/gateway-devices", {"devices": devs})

def get_real_devices() -> list:
    return [d for d in get_devices() if d.get("id") not in MOCK_IDS]

def reset_test_devices(test_devices=None) -> None:
    real = get_real_devices()
    set_devices(real)
    if test_devices:
        # Brief pause so firmware's async MQTT "offline/remove" messages for deleted
        # devices propagate to HA before we publish the new "online+state" messages.
        time.sleep(3)
        set_devices(real + list(test_devices))

def get_test_device(id_: int):
    matches = [d for d in get_devices() if d.get("id") == id_]
    return matches[0] if matches else None

def get_prop(obj, name: str):
    if obj is None:
        return None
    return obj.get(name)

# ---------------------------------------------------------------------------
# HA REST API
# ---------------------------------------------------------------------------

def invoke_ha(path: str):
    if not _ha_token:
        return None
    try:
        resp = requests.get(
            f"{_ha_base}/api{path}",
            headers={"Authorization": f"Bearer {_ha_token}"},
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json()
    except Exception:
        return None

def get_ha_states() -> list:
    raw = invoke_ha("/states")
    if not raw or not isinstance(raw, list):
        return []
    return [s for s in raw if isinstance(s, dict) and "entity_id" in s]

def save_ha_snapshot() -> None:
    global _ha_snapshot
    if _ha_token:
        _ha_snapshot = [s["entity_id"] for s in get_ha_states()]
    else:
        _ha_snapshot = []

def find_ha_new_entity(pattern: str, timeout_sec: int = 8):
    """Return the first new entity matching pattern, or a retained match after timeout."""
    if not _ha_token:
        return None
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        for s in get_ha_states():
            eid = s.get("entity_id", "")
            if eid not in _ha_snapshot and re.search(pattern, eid):
                return s
        time.sleep(1)
    # Fallback: retained entity from a prior run (same MQTT unique_id → same entity_id)
    return next(
        (s for s in get_ha_states() if re.search(pattern, s.get("entity_id", ""))),
        None,
    )

def assert_ha_new(pattern: str, expected: str, label: str) -> None:
    if not _ha_token:
        write_skip(f"HA: {label} (no token)")
        return

    # Poll up to 60s scanning ALL entities matching the pattern on every tick.
    # Prefer a brand-new entity; accept a retained one. Succeed as soon as ANY
    # candidate reaches the expected state. 60s is used because the firmware's MQTT
    # publish loop is async; after a remove+readd cycle the availability=online message
    # can lag by up to one full publish interval.
    ha_timeout_sec = 60
    deadline = time.time() + ha_timeout_sec
    found = None

    while True:
        candidates = [s for s in get_ha_states() if re.search(pattern, s.get("entity_id", ""))]

        # 1st priority: new entity already in expected state
        found = next(
            (c for c in candidates
             if c["entity_id"] not in _ha_snapshot and c.get("state") == expected),
            None,
        )
        # 2nd priority: any entity (retained) in expected state
        if not found:
            found = next((c for c in candidates if c.get("state") == expected), None)

        if found:
            break
        if time.time() >= deadline:
            break
        time.sleep(1)

    if not found:
        # Final check: state may have arrived in the last second between the last poll
        # and the deadline expiry - re-query once more before declaring failure.
        candidates = [s for s in get_ha_states() if re.search(pattern, s.get("entity_id", ""))]
        found = next((c for c in candidates if c.get("state") == expected), None)

    if not found:
        candidates = [s for s in get_ha_states() if re.search(pattern, s.get("entity_id", ""))]
        new_ents = [s["entity_id"] for s in get_ha_states() if s["entity_id"] not in _ha_snapshot]
        if candidates:
            info = "  ".join(f"{c['entity_id']}={c.get('state')}" for c in candidates)
            write_fail(
                f"HA: {label} - no candidate reached expected={expected!r} "
                f"in {ha_timeout_sec}s; {info}"
            )
        elif new_ents:
            write_fail(
                f"HA: {label} - nothing matched {pattern!r}; "
                f"new entities: {', '.join(new_ents)}"
            )
        else:
            write_fail(f"HA: {label} - nothing matched {pattern!r} and no new entities appeared")
        return

    eid  = found["entity_id"]
    note = "" if eid not in _ha_snapshot else " [retained]"
    assert_equal(found.get("state"), expected, f"HA: {label} ({eid}{note})")

def assert_ha_entity_state(entity_id: str, expected: str, label: str) -> None:
    if not _ha_token or not entity_id:
        write_skip(f"HA: {label} (no token or entity)")
        return
    state_val = None
    for _ in range(8):
        r = invoke_ha(f"/states/{entity_id}")
        state_val = get_prop(r, "state")
        if state_val == expected:
            break
        time.sleep(1)
    assert_equal(state_val, expected, f"HA: {label}")

def assert_ha_entity_gone(entity_id: str, label: str) -> None:
    if not _ha_token or not entity_id:
        write_skip(f"HA: {label} (no token or entity)")
        return
    state_val = None
    for _ in range(15):
        r = invoke_ha(f"/states/{entity_id}")
        state_val = get_prop(r, "state")
        if state_val is None or state_val == "unavailable":
            write_pass(f"HA: {label}")
            return
        time.sleep(1)
    write_fail(f"HA: {label} - entity still present after 15s (state={state_val})")

# ---------------------------------------------------------------------------
# Test runner
# ---------------------------------------------------------------------------

def run_test(name: str, fn) -> None:
    if not re.search(_filter_re, name):
        return
    print(f"\n{WHITE}--- {name} ---{RESET}")
    try:
        fn()
    except Exception as e:
        write_fail(f"Exception: {e}")

# ---------------------------------------------------------------------------
# Device fixtures
# ---------------------------------------------------------------------------

def new_acoustic_device(id_: int, location: str = "Test Room",
                        sd_overrides: dict = None, rm_overrides: dict = None) -> dict:
    sd = {
        "model": 3, "sn": id_ + 80000000,
        "productionDate": "2022-01-15T00:00:00.000Z",
        "lastSelftest": "2026-04-01T06:00:00.000Z",
        "deinstallationCount": 0, "alarmCountTotal": 0, "alarmCountLast3Months": 0,
        "hoursInStorageMode": 0, "warrantyFlags": 0,
        "batteryLowFault": False, "deviceFault": False,
        "driftState": 0, "dirtForecastNegative": False,
    }
    if sd_overrides:
        sd.update(sd_overrides)
    rm = {
        "model": 4, "sn": id_ + 70000000, "lineId": id_,
        "lineCharacter": "A", "lineNumber": 1,
        "radioStateMask": 0, "radioSwitchMask": 0,
        "radioInterference": 5.0, "radioNetworkFault": False,
    }
    if rm_overrides:
        rm.update(rm_overrides)
    return {
        "id": id_, "location": location, "isAlarming": False, "registration": 3,
        "readoutTime": "2026-05-04T10:00:00.000Z", "readoutProtocolVersion": 2,
        "alarms": [], "smokeDetector": sd, "radioModule": rm,
    }

def new_manual_device(id_: int, location: str = "Manual Room") -> dict:
    return {
        "id": id_, "location": location, "isAlarming": False, "registration": 2,
        "alarms": [],
        "smokeDetector": {
            "model": 3, "sn": id_ + 80000000,
            "productionDate": "2022-01-15T00:00:00.000Z",
        },
        "radioModule": {"model": 4, "sn": id_ + 70000000},
    }

def new_minimal_device(id_: int, location: str = "T",
                       sd_fields: dict = None, rm_fields: dict = None) -> dict:
    sd = {"model": 3, "sn": 0}
    if sd_fields:
        sd.update(sd_fields)
    rm = {"model": 4, "sn": 0}
    if rm_fields:
        rm.update(rm_fields)
    return {
        "id": id_, "location": location, "isAlarming": False, "registration": 2,
        "alarms": [], "smokeDetector": sd, "radioModule": rm,
    }

# ---------------------------------------------------------------------------
# CATEGORY 1: Basic CRUD
# ---------------------------------------------------------------------------

def tc01():
    save_ha_snapshot()
    d = new_acoustic_device(900001, "Wohnzimmer")
    reset_test_devices([d])

    got = get_test_device(900001)
    assert_not_null(got, "device exists after add")
    assert_equal(get_prop(got, "location"), "Wohnzimmer", "location")
    assert_equal(get_prop(got, "registration"), 3, "registration=Acoustic")
    assert_not_null(get_prop(got, "readoutTime"), "readoutTime present")
    assert_equal(get_prop(get_prop(got, "smokeDetector"), "sn"), d["smokeDetector"]["sn"], "SD serial number")
    assert_equal(get_prop(get_prop(got, "radioModule"), "sn"), d["radioModule"]["sn"], "RM serial number")
    assert_ha_new(r"binary_sensor.*smoke_detector(_\d+)?$", "off", "smoke sensor off")
    reset_test_devices()

def tc02():
    save_ha_snapshot()
    d = new_manual_device(900002, "Keller")
    reset_test_devices([d])

    got = get_test_device(900002)
    assert_equal(get_prop(got, "registration"), 2, "registration=Manual")
    assert_null(get_prop(got, "readoutTime"), "no readoutTime")
    assert_ha_new(r"binary_sensor.*smoke_detector(_\d+)?$", "off", "smoke sensor present")
    reset_test_devices()

def tc03():
    reset_test_devices([new_acoustic_device(900003, "OldName")])
    devs = get_devices()
    for d in devs:
        if d.get("id") == 900003:
            d["location"] = "NewName"
    set_devices(devs)

    assert_equal(get_prop(get_test_device(900003), "location"), "NewName", "location updated")
    reset_test_devices()

def tc04():
    reset_test_devices([new_acoustic_device(900004)])
    devs = get_devices()
    for d in devs:
        if d.get("id") == 900004:
            d["readoutTime"] = "2026-05-04T12:00:00.000Z"
            d["smokeDetector"]["driftState"] = 5
    set_devices(devs)

    got = get_test_device(900004)
    assert_equal(get_prop(get_prop(got, "smokeDetector"), "driftState"), 5,
                 "driftState updated after readoutTime change")
    reset_test_devices()

def tc05():
    save_ha_snapshot()
    reset_test_devices([new_acoustic_device(900005)])
    smoke_ent = find_ha_new_entity(r"binary_sensor.*smoke_detector(_\d+)?$")
    assert_not_null(get_test_device(900005), "exists before delete")

    set_devices([d for d in get_devices() if d.get("id") != 900005])
    assert_null(get_test_device(900005), "absent after delete")
    assert_ha_entity_gone(get_prop(smoke_ent, "entity_id"), "HA entity removed")
    reset_test_devices()

def tc06():
    reset_test_devices([new_acoustic_device(900006)])
    set_devices(get_real_devices())
    assert_null(get_test_device(900006), "test device gone")
    reset_test_devices()

# ---------------------------------------------------------------------------
# CATEGORY 2: Fault States
# ---------------------------------------------------------------------------

def tc07():
    save_ha_snapshot()
    d = new_acoustic_device(900007, "Battery Test", {"batteryLowFault": True})
    reset_test_devices([d])

    assert_true(get_prop(get_prop(get_test_device(900007), "smokeDetector"), "batteryLowFault"),
                "batteryLowFault stored")
    assert_ha_new(r"battery(_\d+)?$", "on", "HA battery_low=on")
    reset_test_devices()

def tc08():
    save_ha_snapshot()
    d = new_acoustic_device(900008, "DevFault Test", {"deviceFault": True})
    reset_test_devices([d])

    assert_true(get_prop(get_prop(get_test_device(900008), "smokeDetector"), "deviceFault"),
                "deviceFault stored")
    assert_ha_new(r"smoke_detector_state(_\d+)?$", "on", "HA device_fault=on")
    reset_test_devices()

def tc09():
    save_ha_snapshot()
    d = new_acoustic_device(900009, "Radio Test", rm_overrides={"radioNetworkFault": True})
    reset_test_devices([d])

    assert_true(get_prop(get_prop(get_test_device(900009), "radioModule"), "radioNetworkFault"),
                "radioNetworkFault stored")
    assert_ha_new(r"radio_module_state(_\d+)?$", "on", "HA radio_fault=on")
    reset_test_devices()

def tc10():
    save_ha_snapshot()
    d = new_acoustic_device(
        900010, "AllFaults",
        sd_overrides={
            "batteryLowFault": True, "deviceFault": True,
            "driftState": 7, "dirtForecastNegative": True, "warrantyFlags": 65535,
        },
        rm_overrides={"radioNetworkFault": True, "radioInterference": 99.9},
    )
    reset_test_devices([d])

    got = get_test_device(900010)
    sd  = get_prop(got, "smokeDetector")
    rm  = get_prop(got, "radioModule")
    assert_true(get_prop(sd, "batteryLowFault"),       "batteryLowFault")
    assert_true(get_prop(sd, "deviceFault"),           "deviceFault")
    assert_equal(get_prop(sd, "driftState"), 7,        "driftState=7")
    assert_true(get_prop(sd, "dirtForecastNegative"),  "dirtForecast")
    assert_equal(get_prop(sd, "warrantyFlags"), 65535, "warrantyFlags=65535")
    assert_true(get_prop(rm, "radioNetworkFault"),     "radioNetworkFault")
    assert_ha_new(r"battery(_\d+)?$",              "on", "HA battery_low")
    assert_ha_new(r"smoke_detector_state(_\d+)?$", "on", "HA device_fault")
    assert_ha_new(r"radio_module_state(_\d+)?$",   "on", "HA radio_fault")
    reset_test_devices()

def tc11():
    save_ha_snapshot()
    d = new_acoustic_device(900011, "FaultClear", {"batteryLowFault": True})
    reset_test_devices([d])
    # Use assert_ha_new (60s) for the initial check - entity may be retained in off state
    # from a prior run and needs the full MQTT settle window to transition to on.
    assert_ha_new(r"battery(_\d+)?$", "on", "initially on")
    # Now the entity is on; grab its ID for the cleared-state check below.
    batt_ent    = find_ha_new_entity(r"battery(_\d+)?$")
    batt_ent_id = get_prop(batt_ent, "entity_id")

    # Clear fault by changing readoutTime (triggers full SD merge)
    devs = get_devices()
    for dev in devs:
        if dev.get("id") == 900011:
            dev["readoutTime"] = "2026-05-04T11:00:00.000Z"
            dev["smokeDetector"]["batteryLowFault"] = False
    set_devices(devs)
    assert_ha_entity_state(batt_ent_id, "off", "cleared to off")
    reset_test_devices()

# ---------------------------------------------------------------------------
# CATEGORY 3: Readout & Availability
# ---------------------------------------------------------------------------

def tc12():
    save_ha_snapshot()
    d = new_acoustic_device(900012, "Recent Readout")
    reset_test_devices([d])
    assert_ha_new(r"last_service(_\d+)?$", "2026-05-04T10:00:00+00:00", "last_readout timestamp")
    reset_test_devices()

def tc13():
    save_ha_snapshot()
    d = new_acoustic_device(900013, "Overdue")
    d["readoutTime"] = "2024-01-10T08:00:00.000Z"
    reset_test_devices([d])

    got = get_test_device(900013)
    assert_not_null(get_prop(got, "readoutTime"), "readoutTime preserved")
    assert_true((get_prop(got, "readoutTime") or "").startswith("2024-"), "readoutTime is 2024")
    assert_ha_new(r"last_service(_\d+)?$", "2024-01-10T08:00:00+00:00", "HA last_readout=old date")
    reset_test_devices()

def tc14():
    save_ha_snapshot()
    d = new_manual_device(900014, "No Readout")
    reset_test_devices([d])
    assert_ha_new(r"battery(_\d+)?$",              "unavailable", "battery_low unavailable")
    assert_ha_new(r"smoke_detector_state(_\d+)?$", "unavailable", "device_fault unavailable")
    assert_ha_new(r"last_service(_\d+)?$",         "unavailable", "last_readout unavailable")
    reset_test_devices()

# ---------------------------------------------------------------------------
# CATEGORY 4: Models & Radio Configs
# ---------------------------------------------------------------------------

def tc15():
    save_ha_snapshot()
    d = new_acoustic_device(900015, "No Radio")
    d["radioModule"] = {"model": 0, "sn": 0}
    reset_test_devices([d])

    got = get_test_device(900015)
    assert_equal(get_prop(get_prop(got, "radioModule"), "model"), 0, "RM model=GRM_NONE")
    assert_equal(get_prop(get_prop(got, "radioModule"), "sn"),    0, "RM sn=0")
    assert_ha_new(r"radio_module_state(_\d+)?$", "off", "radio_fault=off even without module")
    reset_test_devices()

def tc16():
    models = [
        {"id": 900016, "model": 0, "name": "Genius H"},
        {"id": 900017, "model": 1, "name": "Genius Hx"},
        {"id": 900018, "model": 2, "name": "Genius Plus"},
        {"id": 900019, "model": 3, "name": "Genius Plus X"},
    ]
    devs = [new_acoustic_device(m["id"], m["name"], {"model": m["model"]}) for m in models]
    reset_test_devices(devs)

    for m in models:
        got = get_test_device(m["id"])
        assert_equal(get_prop(get_prop(got, "smokeDetector"), "model"), m["model"],
                     f"model {m['model']} round-trips")
    reset_test_devices()

def tc17():
    rm_models = [
        {"id": 900020, "model": 0, "sn": 0},
        {"id": 900021, "model": 1, "sn": 70900021},
        {"id": 900022, "model": 2, "sn": 70900022},
        {"id": 900023, "model": 3, "sn": 70900023},
        {"id": 900024, "model": 4, "sn": 70900024},
        {"id": 900025, "model": 5, "sn": 70900025},
    ]
    devs = [
        new_acoustic_device(m["id"], f"RM {m['model']}",
                            rm_overrides={"model": m["model"], "sn": m["sn"]})
        for m in rm_models
    ]
    reset_test_devices(devs)

    for m in rm_models:
        got = get_test_device(m["id"])
        assert_equal(get_prop(get_prop(got, "radioModule"), "model"), m["model"], f"RM model={m['model']}")
        assert_equal(get_prop(get_prop(got, "radioModule"), "sn"),    m["sn"],    f"RM sn={m['sn']}")
    reset_test_devices()

# ---------------------------------------------------------------------------
# CATEGORY 5: Bulk & Import
# ---------------------------------------------------------------------------

def tc18():
    devs = [new_minimal_device(id_, f"Bulk {id_}") for id_ in range(900030, 900040)]
    reset_test_devices(devs)

    ids = {d.get("id") for d in get_devices()}
    for id_ in range(900030, 900040):
        assert_true(id_ in ids, f"device {id_} present")
    reset_test_devices()

def tc19():
    # Simulate what the Web UI import does: POST full JSON including existing devices
    real = get_real_devices()
    imports = [
        new_minimal_device(900040, "Import A", {"warrantyFlags": 7}),
        new_minimal_device(900041, "Import B", {"driftState": 3}),
    ]
    set_devices(real + imports)

    assert_equal(get_prop(get_prop(get_test_device(900040), "smokeDetector"), "warrantyFlags"), 7,
                 "warrantyFlags preserved")
    assert_equal(get_prop(get_prop(get_test_device(900041), "smokeDetector"), "driftState"), 3,
                 "driftState preserved")
    reset_test_devices()

def tc20():
    d = new_acoustic_device(900042, "Before Import")
    reset_test_devices([d])

    # Re-import same ID with changed location (simulates re-import from file)
    devs = get_devices()
    for dev in devs:
        if dev.get("id") == 900042:
            dev["location"] = "After Import"
    set_devices(devs)

    all_matches = [dev for dev in get_devices() if dev.get("id") == 900042]
    assert_equal(len(all_matches), 1, "no duplicate on re-import")
    assert_equal(get_prop(get_test_device(900042), "location"), "After Import", "location updated")
    reset_test_devices()

def tc21():
    d = new_acoustic_device(900043, "Cycle Test")
    reset_test_devices([d])
    assert_not_null(get_test_device(900043), "present after first import")

    set_devices(get_real_devices())
    assert_null(get_test_device(900043), "absent after delete all")

    reset_test_devices([d])
    assert_not_null(get_test_device(900043), "present after re-import")
    reset_test_devices()

# ---------------------------------------------------------------------------
# CATEGORY 6: Edge Cases
# ---------------------------------------------------------------------------

def tc22():
    d = new_acoustic_device(900050, "Room-01 (2nd Floor) [North]")
    reset_test_devices([d])
    assert_equal(get_prop(get_test_device(900050), "location"), "Room-01 (2nd Floor) [North]",
                 "special ASCII preserved")
    reset_test_devices()

def tc23():
    d = new_acoustic_device(900051, "Küche Über Erdgeschoss")
    reset_test_devices([d])
    # Note: firmware round-trip may corrupt non-ASCII (known issue: ü → replacement char in MQTT)
    # REST round-trip should be intact; MQTT/HA may show replacement char
    got = get_prop(get_test_device(900051), "location")
    assert_not_null(got, "location field present (encoding may vary)")
    reset_test_devices()

def tc24():
    d = new_acoustic_device(900052, "")
    reset_test_devices([d])
    assert_not_null(get_test_device(900052), "device present with empty location")
    reset_test_devices()

def tc25():
    cases = [0, 1, 2, 4, 128, 255, 32768, 65535]
    devs = [new_minimal_device(900053 + i, f"WF {v}", {"warrantyFlags": v})
            for i, v in enumerate(cases)]
    reset_test_devices(devs)

    for i, v in enumerate(cases):
        got = get_prop(get_prop(get_test_device(900053 + i), "smokeDetector"), "warrantyFlags")
        # Firmware omits zero-value fields; treat missing as 0
        if got is None:
            got = 0
        assert_equal(got, v, f"warrantyFlags={v} round-trips")
    reset_test_devices()

def tc26():
    devs = [new_minimal_device(900061 + i, f"Drift {i}", {"driftState": i}) for i in range(8)]
    reset_test_devices(devs)

    for i in range(8):
        got = get_prop(get_prop(get_test_device(900061 + i), "smokeDetector"), "driftState")
        # Firmware omits zero-value fields; treat missing as 0
        if got is None:
            got = 0
        assert_equal(got, i, f"driftState={i}")
    reset_test_devices()

def tc27():
    d = new_acoustic_device(900069, "RadioMask", rm_overrides={"radioStateMask": 255})
    reset_test_devices([d])
    assert_equal(get_prop(get_prop(get_test_device(900069), "radioModule"), "radioStateMask"), 255,
                 "radioStateMask=255")
    reset_test_devices()

def tc28():
    devs = [
        new_acoustic_device(900070, "RI=0",   rm_overrides={"radioInterference": 0.0}),
        new_acoustic_device(900071, "RI=50",  rm_overrides={"radioInterference": 50.0}),
        new_acoustic_device(900072, "RI=100", rm_overrides={"radioInterference": 100.0}),
    ]
    reset_test_devices(devs)
    assert_equal(get_prop(get_prop(get_test_device(900071), "radioModule"), "radioInterference"),
                 50, "50% stored")
    assert_equal(get_prop(get_prop(get_test_device(900072), "radioModule"), "radioInterference"),
                 100, "100% stored")
    reset_test_devices()

def tc29():
    alarms = [
        {"startTime": "2025-01-10T08:00:00.000Z", "endTime": "2025-01-10T08:05:00.000Z", "endingReason": 0},
        {"startTime": "2025-06-15T14:30:00.000Z", "endTime": "2025-06-15T14:35:00.000Z", "endingReason": 1},
        {"startTime": "2026-02-20T03:22:00.000Z", "endTime": "2026-02-20T03:28:00.000Z", "endingReason": 0},
    ]
    d = new_acoustic_device(900073, "MultiAlarm")
    d["alarms"] = alarms
    reset_test_devices([d])

    got = get_test_device(900073)
    assert_equal(len(get_prop(got, "alarms") or []), 3, "3 alarms stored")
    assert_equal(get_prop(got, "alarms")[1]["endingReason"], 1, "alarm[1] endingReason preserved")
    reset_test_devices()

def tc30():
    d = new_acoustic_device(900074, "Date Parse")
    # Before fix: iso8601_to_time_t returned -1 for strings without .000Z
    assert_true(d["readoutTime"].endswith(".000Z"), "test fixture uses .000Z format")
    reset_test_devices([d])
    got = get_test_device(900074)
    assert_true((get_prop(got, "readoutTime") or "").startswith("2026-"),
                "readoutTime round-trips correctly (not 1970)")
    reset_test_devices()

def tc31():
    # NOTE: isAlarming is managed internally; POST cannot set it for existing devices.
    # This test verifies the initial value on new device creation only.
    save_ha_snapshot()
    d = new_acoustic_device(900075, "Alarming")
    d["isAlarming"] = True
    reset_test_devices([d])
    assert_ha_new(r"binary_sensor.*smoke_detector(_\d+)?$", "on",
                  "smoke sensor=on when isAlarming=true")
    reset_test_devices()

def tc32():
    save_ha_snapshot()
    d = new_acoustic_device(900076, "Counts", {
        "deinstallationCount": 7, "alarmCountTotal": 12, "alarmCountLast3Months": 3,
    })
    reset_test_devices([d])

    got = get_test_device(900076)
    sd  = get_prop(got, "smokeDetector")
    assert_equal(get_prop(sd, "deinstallationCount"),   7,  "deinstallationCount")
    assert_equal(get_prop(sd, "alarmCountTotal"),       12, "alarmCountTotal")
    assert_equal(get_prop(sd, "alarmCountLast3Months"), 3,  "alarmCountLast3Months")
    assert_ha_new(r"deinstallation_count(_\d+)?$", "7",  "HA deinstall_count")
    assert_ha_new(r"alarms_total(_\d+)?$",         "12", "HA alarm_count_total")
    reset_test_devices()

def tc33():
    d = new_acoustic_device(900077, "RegType")  # registration=3 (Acoustic)
    reset_test_devices([d])
    assert_equal(get_prop(get_test_device(900077), "registration"), 3, "initially Acoustic")

    # Edit location only - registration must not change
    devs = get_devices()
    for dev in devs:
        if dev.get("id") == 900077:
            dev["location"] = "Edited"
            # Also try to override registration (firmware should ignore for existing devices)
            dev["registration"] = 2
    set_devices(devs)

    assert_equal(get_prop(get_test_device(900077), "registration"), 3,
                 "registration not changed by edit")
    reset_test_devices()

def tc34():
    valid = new_acoustic_device(900078, "LineChar Valid",
                                rm_overrides={"lineCharacter": "F", "lineNumber": 3})
    reset_test_devices([valid])
    assert_equal(get_prop(get_prop(get_test_device(900078), "radioModule"), "lineCharacter"), "F",
                 "valid lineChar F stored")
    reset_test_devices()

# ---------------------------------------------------------------------------
# CATEGORY 7: Resilience
# ---------------------------------------------------------------------------

def tc35():
    real = get_real_devices()
    d    = new_acoustic_device(900080, "WithVersion")
    invoke_gg("POST", "/rest/gateway-devices", {"version": 1, "devices": real + [d]})
    assert_not_null(get_test_device(900080), "device with version=1 accepted")
    reset_test_devices()

def tc36():
    reset_test_devices([new_acoustic_device(900081, "ToBeCleared")])
    assert_not_null(get_test_device(900081), "present before")

    set_devices(get_real_devices())
    assert_null(get_test_device(900081), "cleared by empty-of-test POST")
    reset_test_devices()

def tc37():
    d = new_acoustic_device(900082, "Idempotent")
    reset_test_devices([d])
    count_before = len(get_devices())

    # Post same data again
    set_devices(get_devices())
    count_after = len(get_devices())

    assert_equal(count_after, count_before, "no duplicate on identical re-POST")
    reset_test_devices()

# ---------------------------------------------------------------------------
# CATEGORY 8: Missing & Invalid Data
# ---------------------------------------------------------------------------

def tc38():
    before = len(get_devices())
    # Send root object with no 'devices' key
    invoke_gg("POST", "/rest/gateway-devices", {"version": 1})
    after = len(get_devices())
    assert_equal(after, before, "device count unchanged")

def tc39():
    real = get_real_devices()
    d = new_acoustic_device(0, "ZeroId")
    set_devices(real + [d])
    got = [dev for dev in get_devices() if dev.get("id") == 0]
    assert_not_null(got or None, "device id=0 accepted")
    # Cleanup
    set_devices(real)

def tc40():
    real   = get_real_devices()
    first  = new_acoustic_device(900083, "First")
    second = new_acoustic_device(900083, "Second")
    set_devices(real + [first, second])

    dup_matches = [dev for dev in get_devices() if dev.get("id") == 900083]
    # Firmware does not deduplicate on POST - both entries are stored
    assert_equal(len(dup_matches), 2, "firmware stores both duplicate entries (no dedup)")
    reset_test_devices()

def tc41():
    real = get_real_devices()
    d = {"id": 900084, "location": "NoSD", "isAlarming": False, "registration": 2, "alarms": []}
    set_devices(real + [d])

    got = get_test_device(900084)
    assert_not_null(got, "device accepted without smokeDetector")
    sd_model = get_prop(get_prop(got, "smokeDetector"), "model")
    sd_sn    = get_prop(get_prop(got, "smokeDetector"), "sn")
    if sd_model is None: sd_model = -1
    if sd_sn    is None: sd_sn    = 0
    assert_equal(sd_model, -1, "SD model defaults to -1 (Unknown)")
    assert_equal(sd_sn,    0,  "SD sn defaults to 0")
    reset_test_devices()

def tc42():
    real = get_real_devices()
    d = {
        "id": 900085, "location": "NoRM", "isAlarming": False, "registration": 2, "alarms": [],
        "smokeDetector": {"model": 3, "sn": 80900085},
    }
    set_devices(real + [d])

    got = get_test_device(900085)
    assert_not_null(got, "device accepted without radioModule")
    rm_model = get_prop(get_prop(got, "radioModule"), "model")
    if rm_model is None: rm_model = -1
    assert_equal(rm_model, -1, "RM model defaults to -1 (Unknown)")
    reset_test_devices()

def tc43():
    real = get_real_devices()
    d = {
        "id": 900086, "location": None, "isAlarming": False, "registration": 2, "alarms": [],
        "smokeDetector": {"model": 3, "sn": 80900086},
        "radioModule":   {"model": 4, "sn": 70900086},
    }
    set_devices(real + [d])
    assert_not_null(get_test_device(900086), "device with null location accepted")
    reset_test_devices()

def tc44():
    real = get_real_devices()
    neg  = new_acoustic_device(900087, "EnumNeg", {"model": -999})
    pos  = new_acoustic_device(900088, "EnumPos", {"model":  99})
    set_devices(real + [neg, pos])
    # Firmware casts int to enum without range check - verify it doesn't crash
    assert_not_null(get_test_device(900087), "model=-999 accepted")
    assert_not_null(get_test_device(900088), "model=99 accepted")
    reset_test_devices()

def tc45():
    d = new_acoustic_device(900089, "BadDate")
    d["readoutTime"] = "not-a-date"
    d["smokeDetector"]["productionDate"] = "2022/01/15"  # wrong format
    reset_test_devices([d])

    got = get_test_device(900089)
    assert_not_null(got, "device with bad dates accepted")
    # readoutTime should be absent or epoch - must NOT be a future date
    rt = get_prop(got, "readoutTime")
    if rt is not None:
        assert_true(not rt.startswith("2026-"), "bad readoutTime not persisted as valid date")
    else:
        write_pass("readoutTime absent (parsed as 0/null)")
    reset_test_devices()

def tc46():
    d = new_acoustic_device(900090, "PartialDate")
    d["smokeDetector"]["productionDate"] = "2022-01-15"  # no T or Z
    reset_test_devices([d])
    assert_not_null(get_test_device(900090), "device with partial date accepted")
    reset_test_devices()

def tc49():
    cases = [
        {"id": 900152, "char": "Z", "label": "Z (out of A-J)"},
        {"id": 900153, "char": "!", "label": "! (non-alpha)"},
        {"id": 900154, "char": "1", "label": "1 (digit)"},
    ]
    devs = [
        new_acoustic_device(c["id"], f"LC {c['label']}", rm_overrides={"lineCharacter": c["char"]})
        for c in cases
    ]
    reset_test_devices(devs)

    for c in cases:
        got = get_prop(get_prop(get_test_device(c["id"]), "radioModule"), "lineCharacter")
        assert_true(
            got is None or got == "" or got == 0 or got == "0",
            f"lineCharacter '{c['char']}' rejected: {c['label']}",
        )
    reset_test_devices()

def tc50():
    d8   = new_acoustic_device(900155, "Drift8",   {"driftState": 8})
    d255 = new_acoustic_device(900156, "Drift255", {"driftState": 255})
    reset_test_devices([d8, d255])
    assert_not_null(get_test_device(900155), "driftState=8 accepted")
    assert_not_null(get_test_device(900156), "driftState=255 accepted")
    reset_test_devices()

def tc51():
    d = new_acoustic_device(900157, "CountOverflow", {"alarmCountTotal": 300})
    reset_test_devices([d])
    got = get_prop(get_prop(get_test_device(900157), "smokeDetector"), "alarmCountTotal")
    # ArduinoJson coerces 300 into uint8: either 44 (300 % 256) or 255 (clamped)
    assert_true((got or 0) <= 255, "alarmCountTotal truncated to uint8 range")
    reset_test_devices()

def tc52():
    real = get_real_devices()
    d = new_acoustic_device(900158, "ExtraFields")
    d["unknownFoo"]  = "bar"
    d["futureField"] = 42
    set_devices(real + [d])

    got = get_test_device(900158)
    assert_not_null(got, "device with extra fields accepted")
    assert_equal(get_prop(got, "location"), "ExtraFields", "known fields preserved")
    reset_test_devices()

def tc53():
    real = get_real_devices()
    d    = new_acoustic_device(900159, "RootExtra")
    invoke_gg("POST", "/rest/gateway-devices",
              {"version": 1, "devices": real + [d], "unexpectedRootKey": "hello"})
    assert_not_null(get_test_device(900159), "root-level extra fields ignored")
    reset_test_devices()

def tc54():
    real = get_real_devices()
    d = new_acoustic_device(900160, "BadAlarm")
    d["alarms"] = [{"endingReason": 0}]  # startTime and endTime missing
    # Expect device stored (with 0/empty alarm or alarm dropped). Must NOT crash the firmware.
    try:
        set_devices(real + [d])
        assert_not_null(get_test_device(900160), "device accepted despite bad alarm")
    except Exception as e:
        write_fail(f"POST threw exception with bad alarm: {e}")
    reset_test_devices()

def tc55():
    alarm = {
        "startTime": "2025-03-01T10:00:00.000Z",
        "endTime":   "2025-03-01T10:05:00.000Z",
        "endingReason": -1,
    }
    d = new_acoustic_device(900161, "ActiveAlarmReason")
    d["alarms"] = [alarm]
    reset_test_devices([d])

    got = get_test_device(900161)
    assert_equal(get_prop(got, "alarms")[0]["endingReason"], -1,
                 "endingReason=-1 (AlarmActive) preserved")
    reset_test_devices()

def tc56():
    d = new_acoustic_device(900162, "NegRI", rm_overrides={"radioInterference": -5.5})
    reset_test_devices([d])

    got = get_prop(get_test_device(900162), "radioModule")
    assert_not_null(got, "device with negative radioInterference accepted")
    ri = get_prop(got, "radioInterference")
    if ri is None:
        ri = 0.0
    assert_true(ri >= 0, f"radioInterference clamped to >= 0 (got {ri})")
    reset_test_devices()

def tc57():
    devs = [
        new_acoustic_device(900163, "RPV0"),
        new_acoustic_device(900164, "RPV1"),
        new_acoustic_device(900165, "RPV255"),
    ]
    devs[0]["readoutProtocolVersion"] = 0
    devs[1]["readoutProtocolVersion"] = 1
    devs[2]["readoutProtocolVersion"] = 255
    reset_test_devices(devs)

    assert_equal(get_prop(get_test_device(900163), "readoutProtocolVersion"), 0,   "rpv=0")
    assert_equal(get_prop(get_test_device(900164), "readoutProtocolVersion"), 1,   "rpv=1")
    assert_equal(get_prop(get_test_device(900165), "readoutProtocolVersion"), 255, "rpv=255")
    reset_test_devices()

# ---------------------------------------------------------------------------
# CATEGORY 9: Device Ordering
# ---------------------------------------------------------------------------

def tc58():
    ids  = [900170, 900171, 900172, 900173, 900174]
    devs = [new_acoustic_device(id_, f"Order {id_}") for id_ in ids]
    reset_test_devices(devs)

    returned = [d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS]
    for i, id_ in enumerate(ids):
        assert_equal(returned[i], id_, f"position {i} = id {id_}")
    reset_test_devices()

def tc59():
    ids  = [900175, 900176, 900177]
    devs = [new_acoustic_device(id_, f"Reorder {id_}") for id_ in ids]
    reset_test_devices(devs)

    real      = get_real_devices()
    test_devs = [d for d in get_devices() if d.get("id") in MOCK_IDS]
    test_devs.reverse()
    set_devices(real + test_devs)

    returned     = [d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS]
    reversed_ids = [ids[2], ids[1], ids[0]]
    for i, id_ in enumerate(reversed_ids):
        assert_equal(returned[i], id_, f"reversed position {i} = id {id_}")
    reset_test_devices()

def tc60():
    existing  = [900178, 900179, 900180]
    devs      = [new_acoustic_device(id_, f"Existing {id_}") for id_ in existing]
    reset_test_devices(devs)

    new_dev   = new_acoustic_device(900181, "Prepended")
    real      = get_real_devices()
    test_devs = [d for d in get_devices() if d.get("id") in MOCK_IDS]
    set_devices(real + [new_dev] + test_devs)

    returned = [d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS]
    assert_equal(returned[0], 900181, "new device is first")
    assert_equal(returned[1], 900178, "original first is now second")
    reset_test_devices()

def tc61():
    existing  = [900182, 900183, 900184]
    devs      = [new_acoustic_device(id_, f"Existing {id_}") for id_ in existing]
    reset_test_devices(devs)

    new_dev   = new_acoustic_device(900185, "Appended")
    real      = get_real_devices()
    test_devs = [d for d in get_devices() if d.get("id") in MOCK_IDS]
    set_devices(real + test_devs + [new_dev])

    returned = [d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS]
    assert_equal(returned[-1], 900185, "new device is last")
    assert_equal(returned[-2], 900184, "previous last is second-to-last")
    reset_test_devices()

def tc62():
    existing  = [900186, 900187, 900188, 900189]
    devs      = [new_acoustic_device(id_, f"Mid {id_}") for id_ in existing]
    reset_test_devices(devs)

    new_dev   = new_acoustic_device(900190, "Middle")
    real      = get_real_devices()
    test_devs = [d for d in get_devices() if d.get("id") in MOCK_IDS]
    # Insert after index 1 (between 900187 and 900188)
    before = test_devs[:2]
    after  = test_devs[2:]
    set_devices(real + before + [new_dev] + after)

    returned = [d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS]
    assert_equal(returned[2], 900190, "inserted device at position 2")
    assert_equal(returned[3], 900188, "following device shifted right")
    reset_test_devices()

def tc63():
    ids       = [900191, 900192, 900193, 900194]
    devs      = [new_acoustic_device(id_, f"Move {id_}") for id_ in ids]
    reset_test_devices(devs)

    real      = get_real_devices()
    test_devs = [d for d in get_devices() if d.get("id") in MOCK_IDS]
    last = test_devs[-1]
    rest = test_devs[:-1]
    set_devices(real + [last] + rest)

    returned = [d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS]
    assert_equal(returned[0], 900194, "last moved to first")
    assert_equal(returned[1], 900191, "original first is now second")
    reset_test_devices()

def tc64():
    ids       = [900195, 900196, 900197]
    devs      = [new_acoustic_device(id_, f"Persist {id_}") for id_ in ids]
    reset_test_devices(devs)

    real      = get_real_devices()
    test_devs = [d for d in get_devices() if d.get("id") in MOCK_IDS]
    test_devs.reverse()
    set_devices(real + test_devs)

    # Second GET - should still reflect reversed order
    returned1 = [d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS]
    time.sleep(0.3)
    returned2 = [d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS]
    assert_equal(returned2[0], returned1[0], "order stable across two GETs")
    assert_equal(returned2[2], returned1[2], "last position stable")
    reset_test_devices()

def tc65():
    ids       = [900196, 900197, 900198]
    devs      = [new_acoustic_device(id_, f"Content {id_}", {"warrantyFlags": 42}) for id_ in ids]
    reset_test_devices(devs)

    real      = get_real_devices()
    test_devs = [d for d in get_devices() if d.get("id") in MOCK_IDS]
    test_devs.reverse()
    set_devices(real + test_devs)

    # Verify content unchanged after reorder
    for id_ in ids:
        got = get_test_device(id_)
        assert_equal(get_prop(get_prop(got, "smokeDetector"), "warrantyFlags"), 42,
                     f"warrantyFlags intact after reorder (id={id_})")
    reset_test_devices()

# ---------------------------------------------------------------------------
# CATEGORY 10: Per-Device CRUD + Chunked Import endpoints
# ---------------------------------------------------------------------------

def tc70():
    """PUT /rest/gateway-devices/device  - create new device with client-allocated id."""
    real = get_real_devices()
    set_devices(real)  # clean slate

    new_dev = new_acoustic_device(900200, "PutCreate")
    resp = invoke_gg_raw("PUT", "/rest/gateway-devices/device", new_dev)
    assert_equal(resp.status_code, 200, "PUT new device returns 200")
    assert_not_null(get_test_device(900200), "device retrievable after PUT")
    reset_test_devices()

def tc71():
    """PUT /rest/gateway-devices/device  - update existing device in place."""
    real = get_real_devices()
    set_devices(real)

    d = new_acoustic_device(900201, "BeforeEdit")
    invoke_gg_raw("PUT", "/rest/gateway-devices/device", d)
    d["location"] = "AfterEdit"
    resp = invoke_gg_raw("PUT", "/rest/gateway-devices/device", d)
    assert_equal(resp.status_code, 200, "PUT existing device returns 200")
    assert_equal(get_prop(get_test_device(900201), "location"), "AfterEdit",
                 "location updated by PUT")
    reset_test_devices()

def tc72():
    """PUT with id=0 is rejected (clients must allocate ids)."""
    bad = new_acoustic_device(0, "ZeroIdPut")
    resp = invoke_gg_raw("PUT", "/rest/gateway-devices/device", bad)
    assert_equal(resp.status_code, 400, "PUT with id=0 returns 400")

def tc73():
    """POST /rest/gateway-devices/device/delete  - delete by id."""
    real = get_real_devices()
    set_devices(real)

    d = new_acoustic_device(900202, "ToDelete")
    invoke_gg_raw("PUT", "/rest/gateway-devices/device", d)
    assert_not_null(get_test_device(900202), "device present before delete")

    resp = invoke_gg_raw("POST", "/rest/gateway-devices/device/delete", {"id": 900202})
    assert_equal(resp.status_code, 200, "delete returns 200")
    assert_null(get_test_device(900202), "device removed after delete")

def tc74():
    """Deleting a non-existent id returns 404."""
    resp = invoke_gg_raw("POST", "/rest/gateway-devices/device/delete", {"id": 999999})
    assert_equal(resp.status_code, 404, "delete of unknown id returns 404")

def tc75():
    """POST /rest/gateway-devices/reorder  - rearrange existing device ids."""
    real = get_real_devices()
    set_devices(real)

    ids = [900203, 900204, 900205]
    for id_ in ids:
        invoke_gg_raw("PUT", "/rest/gateway-devices/device", new_acoustic_device(id_, f"Reorder {id_}"))

    reversed_ids = list(reversed(ids))
    resp = invoke_gg_raw("POST", "/rest/gateway-devices/reorder",
                         {"order": [d["id"] for d in get_real_devices()] + reversed_ids})
    # The reorder endpoint expects the EXACT current id set. Including real-device ids
    # keeps the new order valid when other devices already exist.
    assert_equal(resp.status_code, 200, "reorder returns 200")
    returned = [d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS]
    assert_equal(returned, reversed_ids, "device order matches request")
    reset_test_devices()

def tc76():
    """Reorder rejects an id set that doesn't match the current state."""
    real = get_real_devices()
    set_devices(real)
    invoke_gg_raw("PUT", "/rest/gateway-devices/device", new_acoustic_device(900206, "ReorderBad"))

    resp = invoke_gg_raw("POST", "/rest/gateway-devices/reorder", {"order": [999999, 888888]})
    assert_equal(resp.status_code, 400, "reorder with unknown ids returns 400")
    reset_test_devices()

def tc77():
    """Chunked import: begin -> 2 chunks -> commit replaces device list atomically."""
    real = get_real_devices()
    set_devices(real)

    devs_a = [new_acoustic_device(900210 + i, f"Chunk1-{i}") for i in range(3)]
    devs_b = [new_acoustic_device(900220 + i, f"Chunk2-{i}") for i in range(2)]

    # begin
    resp = invoke_gg_raw("POST", "/rest/gateway-devices/import/begin")
    assert_equal(resp.status_code, 200, "import/begin returns 200")
    session_id = resp.json()["sessionId"]
    assert_true(len(session_id) > 0, "sessionId is non-empty")

    # chunk 1 (devs_a)
    resp = invoke_gg_raw("POST", "/rest/gateway-devices/import/chunk",
                         {"sessionId": session_id, "devices": devs_a})
    assert_equal(resp.status_code, 200, "first chunk accepted")

    # chunk 2 (devs_b)
    resp = invoke_gg_raw("POST", "/rest/gateway-devices/import/chunk",
                         {"sessionId": session_id, "devices": devs_b})
    assert_equal(resp.status_code, 200, "second chunk accepted")

    # before commit: device list must be untouched
    pre_commit_ids = {d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS}
    assert_equal(pre_commit_ids, set(), "no test devices visible before commit")

    # commit
    resp = invoke_gg_raw("POST", "/rest/gateway-devices/import/commit",
                         {"sessionId": session_id})
    assert_equal(resp.status_code, 200, "commit returns 200")

    expected = {d["id"] for d in devs_a + devs_b}
    got = {d.get("id") for d in get_devices() if d.get("id") in MOCK_IDS}
    assert_equal(got, expected, "all staged devices present after commit")

    reset_test_devices()

def tc78():
    """Chunked import: a second begin while a session is active returns 409 BUSY."""
    resp = invoke_gg_raw("POST", "/rest/gateway-devices/import/begin")
    assert_equal(resp.status_code, 200, "first begin returns 200")
    session_id = resp.json()["sessionId"]

    resp2 = invoke_gg_raw("POST", "/rest/gateway-devices/import/begin")
    assert_equal(resp2.status_code, 409, "second begin returns 409")

    # cleanup
    invoke_gg_raw("POST", "/rest/gateway-devices/import/abort", {"sessionId": session_id})

def tc79():
    """Chunked import: chunk with bad sessionId is rejected with 410."""
    resp = invoke_gg_raw("POST", "/rest/gateway-devices/import/chunk",
                         {"sessionId": "deadbeefdeadbeef", "devices": []})
    assert_equal(resp.status_code, 410, "bad-session chunk returns 410")

def tc80():
    """Chunked import: abort discards staging and frees the slot."""
    resp = invoke_gg_raw("POST", "/rest/gateway-devices/import/begin")
    session_id = resp.json()["sessionId"]
    d = new_acoustic_device(900230, "AbortedStaging")
    invoke_gg_raw("POST", "/rest/gateway-devices/import/chunk",
                  {"sessionId": session_id, "devices": [d]})

    resp = invoke_gg_raw("POST", "/rest/gateway-devices/import/abort",
                         {"sessionId": session_id})
    assert_equal(resp.status_code, 200, "abort returns 200")
    assert_null(get_test_device(900230), "staged device NOT visible after abort")

    # Slot should be free now
    resp2 = invoke_gg_raw("POST", "/rest/gateway-devices/import/begin")
    assert_equal(resp2.status_code, 200, "begin succeeds after abort")
    invoke_gg_raw("POST", "/rest/gateway-devices/import/abort",
                  {"sessionId": resp2.json()["sessionId"]})

# ---------------------------------------------------------------------------
# CATEGORY 11: HA entity-ID rename (HA option "rename_entity_ids")
# ---------------------------------------------------------------------------
#
# With the option on, the gateway gives every detector explicit HA entity IDs
# "<domain>.<prefix>_<entity>" and renames the entities (remove, wait 3 s,
# publish again) whenever the prefix changes. The prefix last announced to HA
# is reported per device as the read-only field "haEntityIdPrefix", which lets
# these tests follow a rename over REST alone; the HA assertions on top need
# --ha-token.
#
# Switching the option on renames ALL detectors of the gateway, real ones
# included, and switching it off again does not rename them back. The category
# therefore only runs if that cannot happen, or if --rename-entity-ids allows it.

_rename_checked     = False
_rename_skip_reason = None
_allow_rename_real  = False

def get_ha_settings() -> dict:
    return invoke_gg("GET", "/rest/haSettings")

def set_rename_option(enabled: bool) -> None:
    settings = get_ha_settings()
    if bool(settings.get("rename_entity_ids")) != enabled:
        settings["rename_entity_ids"] = enabled
        invoke_gg("POST", "/rest/haSettings", settings)

def _rename_tests_skip_reason():
    """Why the rename tests must not run on this gateway, or None if they may."""
    global _rename_checked, _rename_skip_reason
    if _rename_checked:
        return _rename_skip_reason
    _rename_checked = True
    try:
        settings = get_ha_settings()
        if "rename_entity_ids" not in settings:
            _rename_skip_reason = "firmware has no rename_entity_ids option"
        elif not settings.get("enabled"):
            _rename_skip_reason = "HA integration is disabled on the gateway"
        elif not invoke_gg("GET", "/rest/mqttStatus").get("connected"):
            _rename_skip_reason = "gateway is not connected to the MQTT broker"
        elif (not settings.get("rename_entity_ids") and get_real_devices()
              and not _allow_rename_real):
            _rename_skip_reason = ("option is off and real detectors exist - their HA entity IDs "
                                   "would be renamed; pass --rename-entity-ids to allow that")
    except Exception as e:
        _rename_skip_reason = f"HA settings not readable: {e}"
    return _rename_skip_reason

def with_rename_option(enabled: bool, body) -> None:
    """Run body() with the option forced to `enabled`, then remove the test devices and
    restore the option - also if body() raises."""
    reason = _rename_tests_skip_reason()
    if reason:
        write_skip(f"entity-ID rename test ({reason})")
        return
    original = bool(get_ha_settings().get("rename_entity_ids"))
    try:
        set_rename_option(enabled)
        body()
    finally:
        reset_test_devices()
        set_rename_option(original)

def get_prefix(id_: int):
    return get_prop(get_test_device(id_), "haEntityIdPrefix")

def assert_prefix(id_: int, expected, label: str, timeout_sec: int = 45) -> None:
    """Poll until the gateway reports `expected` as the prefix last announced to HA.
    A rename takes at least 3 s (the pause) plus the publishing of 14 entities."""
    deadline = time.time() + timeout_sec
    val = get_prefix(id_)
    while val != expected and time.time() < deadline:
        time.sleep(1)
        val = get_prefix(id_)
    assert_equal(val, expected, label)

def set_location(id_: int, location: str) -> None:
    devs = get_devices()
    for d in devs:
        if d.get("id") == id_:
            d["location"] = location
    set_devices(devs)

def ha_entity(entity_id: str):
    """(known, state object): known is False for a 404, None if HA could not be asked."""
    try:
        resp = requests.get(
            f"{_ha_base}/api/states/{entity_id}",
            headers={"Authorization": f"Bearer {_ha_token}"},
            timeout=10,
        )
    except Exception:
        return None, None
    if resp.status_code == 200:
        return True, resp.json()
    if resp.status_code == 404:
        return False, None
    return None, None

def assert_ha_entity_present(entity_id: str, label: str, timeout_sec: int = 30) -> None:
    if not _ha_token:
        write_skip(f"HA: {label} (no token)")
        return
    deadline = time.time() + timeout_sec
    known, _ = ha_entity(entity_id)
    while known is not True and time.time() < deadline:
        time.sleep(1)
        known, _ = ha_entity(entity_id)
    assert_true(known is True, f"HA: {label} ({entity_id})")

def assert_ha_entity_absent(entity_id: str, label: str, timeout_sec: int = 30) -> None:
    """The entity ID must be unknown to HA (404). Stricter than assert_ha_entity_gone(),
    which also accepts an entity that still exists but is unavailable."""
    if not _ha_token:
        write_skip(f"HA: {label} (no token)")
        return
    deadline = time.time() + timeout_sec
    known, _ = ha_entity(entity_id)
    while known is not False and time.time() < deadline:
        time.sleep(1)
        known, _ = ha_entity(entity_id)
    assert_true(known is False, f"HA: {label} ({entity_id})")

def new_ha_entities(pattern: str, timeout_sec: int = 30) -> list:
    """Entity IDs matching pattern that were not in the snapshot; waits for the first one."""
    if not _ha_token:
        return []
    deadline = time.time() + timeout_sec
    while True:
        found = sorted(s["entity_id"] for s in get_ha_states()
                       if s["entity_id"] not in _ha_snapshot and re.search(pattern, s["entity_id"]))
        if found or time.time() >= deadline:
            return found
        time.sleep(1)

SMOKE_ENTITY_RE = r"^binary_sensor\..*smoke_detector(_\d+)?$"

def tc81():
    """Option off: a location change leaves the entity IDs untouched."""
    def body():
        save_ha_snapshot()
        reset_test_devices([new_acoustic_device(900151, "Rename Off A")])
        before = new_ha_entities(SMOKE_ENTITY_RE)
        if _ha_token:
            assert_equal(len(before), 1, f"HA: one smoke entity registered ({before})")

        set_location(900151, "Rename Off B")
        time.sleep(8)  # longer than a rename cycle would take

        got = get_test_device(900151)
        assert_equal(get_prop(got, "location"), "Rename Off B", "location updated")
        assert_null(get_prop(got, "haEntityIdPrefix"), "no prefix announced while the option is off")
        if _ha_token:
            assert_equal(new_ha_entities(SMOKE_ENTITY_RE, 0), before, "HA: entity ID unchanged")
        else:
            write_skip("HA: entity ID unchanged (no token)")
    with_rename_option(False, body)

def tc82():
    """Option on: a location change removes the old IDs and creates the new ones."""
    def body():
        reset_test_devices([new_acoustic_device(900152, "Rename Start")])
        assert_prefix(900152, "rename_start", "prefix announced after add")
        assert_ha_entity_present("binary_sensor.rename_start_smoke_detector", "smoke entity under first ID")
        assert_ha_entity_present("sensor.rename_start_alarm_line", "diagnostic entity under first ID")

        set_location(900152, "Rename Target")
        assert_prefix(900152, "rename_target", "prefix follows the new location")
        assert_ha_entity_present("binary_sensor.rename_target_smoke_detector", "smoke entity under new ID")
        assert_ha_entity_present("sensor.rename_target_alarm_line", "diagnostic entity under new ID")
        assert_ha_entity_absent("binary_sensor.rename_start_smoke_detector", "old smoke entity ID gone")
        assert_ha_entity_absent("sensor.rename_start_alarm_line", "old diagnostic entity ID gone")
        assert_ha_entity_absent("binary_sensor.rename_target_smoke_detector_2", "no _2 duplicate")
    with_rename_option(True, body)

def tc83():
    """Switching the option on renames all detectors in one batch."""
    devices = {900153: ("Batch One", "batch_one"),
               900154: ("Batch Two", "batch_two"),
               900155: ("Batch Three", "batch_three")}
    def body():
        reset_test_devices([new_acoustic_device(i, loc) for i, (loc, _) in devices.items()])
        time.sleep(5)
        for i in devices:
            assert_null(get_prefix(i), f"{i}: no prefix while the option is off")

        set_rename_option(True)
        for i, (_, slug) in devices.items():
            assert_prefix(i, slug, f"{i}: prefix announced after switching the option on")
        for _, slug in devices.values():
            assert_ha_entity_present(f"binary_sensor.{slug}_smoke_detector", f"{slug}: smoke entity renamed")
    with_rename_option(False, body)

def tc84():
    """A cleared location falls back to the serial-number prefix."""
    def body():
        d = new_acoustic_device(900156, "Fallback Room")
        sn = d["smokeDetector"]["sn"]
        reset_test_devices([d])
        assert_prefix(900156, "fallback_room", "prefix from the location")
        assert_ha_entity_present("binary_sensor.fallback_room_smoke_detector", "smoke entity under location ID")

        set_location(900156, "")
        assert_prefix(900156, f"genius_{sn}", "prefix falls back to the serial number")
        assert_ha_entity_present(f"binary_sensor.genius_{sn}_smoke_detector", "smoke entity under serial ID")
        assert_ha_entity_absent("binary_sensor.fallback_room_smoke_detector", "location-based ID freed")

        # Serial number 0: the internal device ID takes over.
        resp = invoke_gg_raw("PUT", "/rest/gateway-devices/device", new_minimal_device(900157, ""))
        assert_equal(resp.status_code, 200, "PUT device without location and serial number")
        assert_equal(resp.json().get("haEntityIdPrefix"), "genius_device_900157",
                     "prefix falls back to the device ID")
    with_rename_option(True, body)

def tc85():
    """An edit that keeps the prefix (other case, other punctuation) triggers no rename."""
    def body():
        entity_id = "binary_sensor.same_slug_smoke_detector"
        reset_test_devices([new_acoustic_device(900158, "Same Slug")])
        assert_prefix(900158, "same_slug", "prefix announced after add")
        assert_ha_entity_present(entity_id, "smoke entity registered")
        _, before = ha_entity(entity_id) if _ha_token else (None, None)

        set_location(900158, "SAME  slug!")
        time.sleep(8)  # a rename cycle would have removed and re-created the entity by now

        got = get_test_device(900158)
        assert_equal(get_prop(got, "location"), "SAME  slug!", "location updated")
        assert_equal(get_prop(got, "haEntityIdPrefix"), "same_slug", "prefix unchanged")
        if before:
            _, after = ha_entity(entity_id)
            # last_changed only moves when the state value changes; a removed and
            # re-created entity starts with a new one.
            assert_equal(get_prop(after, "last_changed"), before.get("last_changed"),
                         "HA: entity not re-created (last_changed unchanged)")
        else:
            write_skip("HA: entity not re-created (no token)")
    with_rename_option(True, body)

def tc86():
    """Prefix slugs match what Home Assistant would generate; unusable locations fall back."""
    cases = [
        ("Küche", "kuche"),
        ("Straße 5", "strasse_5"),
        ("Café Élysée", "cafe_elysee"),
        ("Łódź", "lodz"),
        ("Bad/WC 2.OG", "bad_wc_2_og"),
        ("  --Flur (EG)--  ", "flur_eg"),
    ]
    def body():
        set_devices(get_real_devices())
        id_ = 900160
        for location, slug in cases:
            resp = invoke_gg_raw("PUT", "/rest/gateway-devices/device", new_acoustic_device(id_, location))
            assert_equal(resp.json().get("haEntityIdPrefix"), slug, f"{location!r} -> {slug}")
            id_ += 1
        for location in ("---", "Unknown location"):
            d = new_acoustic_device(id_, location)
            resp = invoke_gg_raw("PUT", "/rest/gateway-devices/device", d)
            assert_equal(resp.json().get("haEntityIdPrefix"), f"genius_{d['smokeDetector']['sn']}",
                         f"{location!r} -> serial-number fallback")
            id_ += 1
    with_rename_option(True, body)

def tc87():
    """The announced prefix is backend-owned: a value sent by a client is ignored."""
    def body():
        set_devices(get_real_devices())
        d = new_acoustic_device(900170, "Trusted Room")
        d["haEntityIdPrefix"] = "forged"
        resp = invoke_gg_raw("PUT", "/rest/gateway-devices/device", d)
        assert_equal(resp.json().get("haEntityIdPrefix"), "trusted_room", "ignored on create (PUT)")

        d = resp.json()
        d["haEntityIdPrefix"] = "forged_again"
        d["radioModule"]["lineNumber"] = 2
        resp = invoke_gg_raw("PUT", "/rest/gateway-devices/device", d)
        assert_equal(resp.json().get("haEntityIdPrefix"), "trusted_room", "ignored on update (PUT)")

        devs = get_devices()
        for dev in devs:
            if dev.get("id") == 900170:
                dev["haEntityIdPrefix"] = "forged_bulk"
                dev["radioModule"]["lineNumber"] = 3
        set_devices(devs)
        assert_equal(get_prefix(900170), "trusted_room", "ignored on bulk POST")
    with_rename_option(True, body)

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    global _gg_password, _ha_token, _filter_re, _gg_base, _ha_base, _allow_rename_real

    parser = argparse.ArgumentParser(
        description="Integration tests for Genius Gateway HA/MQTT device integration",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--gg-host",     default=None, metavar="HOST",
                        help="Genius Gateway IP or hostname")
    parser.add_argument("--gg-password", default=None, metavar="PASS",
                        help="Admin password (default: admin if prompted)")
    parser.add_argument("--ha-host",     default="homeassistant.local", metavar="HOST")
    parser.add_argument("--ha-port",     type=int, default=8123, metavar="PORT")
    parser.add_argument("--ha-user",     default="", metavar="USER",
                        help="HA username (informational only)")
    parser.add_argument("--ha-token",    default="", metavar="TOKEN",
                        help="Long-lived HA access token; omit to skip HA checks")
    parser.add_argument("--filter",      default=".*", dest="filter_re", metavar="REGEX",
                        help="Run only tests whose name matches this regex")
    parser.add_argument("--rename-entity-ids", action="store_true",
                        help="Let TC81-TC87 switch the HA option 'rename_entity_ids' on although "
                             "real detectors exist. Their HA entity IDs are renamed and stay renamed.")
    args = parser.parse_args()

    gg_host      = args.gg_host or ""
    _gg_password = args.gg_password or ""
    ha_host      = args.ha_host
    ha_port      = args.ha_port
    ha_user      = args.ha_user
    _ha_token    = args.ha_token
    _filter_re   = args.filter_re
    _allow_rename_real = args.rename_entity_ids

    # Interactive prompts when credentials are missing
    if not gg_host or not _gg_password:
        print(f"\n{YELLOW}Genius Gateway settings not fully provided.{RESET}")
        print(f"{YELLOW}  Pass --gg-host / --gg-password to skip this prompt.{RESET}")
        if not gg_host:
            val = input("  GG Host: ").strip()
            if val:
                gg_host = val
        if not _gg_password:
            val = input("  GG Password [admin]: ").strip()
            _gg_password = val if val else "admin"

    if not _ha_token:
        print(f"\n{YELLOW}HA token not provided.{RESET}")
        print(f"{YELLOW}  Pass --ha-host/--ha-port/--ha-user/--ha-token to skip.{RESET}")
        print(f"{YELLOW}  Press Enter to keep the default. Leave HA Token empty to skip HA checks.{RESET}")
        val = input(f"  HA Host [{ha_host}]: ").strip()
        if val:
            ha_host = val
        val = input(f"  HA Port [{ha_port}]: ").strip()
        if val:
            ha_port = int(val)
        val = input(f"  HA User (optional) [{ha_user}]: ").strip()
        if val:
            ha_user = val
        val = input("  HA Token: ").strip()
        if val:
            _ha_token = val

    _gg_base = f"http://{gg_host}"
    _ha_base = f"http://{ha_host}:{ha_port}"

    # Probe HA connection
    if _ha_token:
        print(f"\n{CYAN}Testing HA connection to {_ha_base} ...{RESET}")
        try:
            resp = requests.get(
                f"{_ha_base}/api/",
                headers={"Authorization": f"Bearer {_ha_token}"},
                timeout=5,
            )
            resp.raise_for_status()
            data   = resp.json()
            ha_msg = data.get("message", "OK")
            ha_ver = f" v{data['version']}" if "version" in data else ""
            print(f"  {GREEN}HA connected: {ha_msg}{ha_ver}{RESET}")
            if ha_user:
                print(f"  {CYAN}User: {ha_user}{RESET}")
        except Exception as e:
            print(f"  {RED}HA connection FAILED: {e}{RESET}")
            print(f"  {YELLOW}All HA-layer tests will be SKIPPED.{RESET}")
            _ha_token = ""

    # Run tests
    write_section("CATEGORY 1  - Basic CRUD")
    run_test("TC01 Add acoustic device  - round-trip", tc01)
    run_test("TC02 Add manual device (no readout)", tc02)
    run_test("TC03 Edit location of existing device", tc03)
    run_test("TC04 Edit readoutTime triggers full SD/RM update", tc04)
    run_test("TC05 Delete single device", tc05)
    run_test("TC06 Delete all devices  - state is empty", tc06)

    write_section("CATEGORY 2  - Fault States")
    run_test("TC07 batteryLowFault=true round-trips and HA reflects it", tc07)
    run_test("TC08 deviceFault=true round-trips", tc08)
    run_test("TC09 radioNetworkFault=true round-trips", tc09)
    run_test("TC10 All faults simultaneously", tc10)
    run_test("TC11 Clear fault  - HA transitions from on to off", tc11)

    write_section("CATEGORY 3  - Readout & Availability")
    run_test("TC12 Recent readout  - HA diagnostics available", tc12)
    run_test("TC13 Overdue readout (>1 year)  - available but old date", tc13)
    run_test("TC14 No readout  - HA diagnostics unavailable", tc14)

    write_section("CATEGORY 4  - Models & Radio Configs")
    run_test("TC15 No radio module (GRM_NONE=0, sn=0)", tc15)
    run_test("TC16 All smoke detector models (H, Hx, Plus, Plus X)", tc16)
    run_test("TC17 All radio module models (None to FM Pro X)", tc17)

    write_section("CATEGORY 5  - Bulk & Import")
    run_test("TC18 Bulk add 10 devices  - all persisted and published", tc18)
    run_test("TC19 Import preserves field order and values", tc19)
    run_test("TC20 Re-import same IDs  - updates, no duplicates", tc20)
    run_test("TC21 Import then delete all then re-import (full cycle)", tc21)

    write_section("CATEGORY 6  - Edge Cases")
    run_test("TC22 Location with ASCII special chars and numbers", tc22)
    run_test("TC23 Location with German umlauts (UTF-8)", tc23)
    run_test("TC24 Empty location string", tc24)
    run_test("TC25 warrantyFlags at all bit positions (0 to 65535)", tc25)
    run_test("TC26 driftState all values 0..7", tc26)
    run_test("TC27 radioStateMask all-bits set (255)", tc27)
    run_test("TC28 radioInterference boundary values (0, 50, 100)", tc28)
    run_test("TC29 Device with multiple alarms  - all preserved", tc29)
    run_test("TC30 Dates without milliseconds parsed correctly (Utils.cpp fix)", tc30)
    run_test("TC31 isAlarming=true reflected in HA smoke sensor", tc31)
    run_test("TC32 deinstallationCount and alarmCountTotal preserved", tc32)
    run_test("TC33 Registration type never overwritten on edit", tc33)
    run_test("TC34 lineCharacter valid (A-J) and invalid values", tc34)

    write_section("CATEGORY 7  - Resilience")
    run_test("TC35 POST with version field  - accepted", tc35)
    run_test("TC36 POST empty devices array  - clears test devices", tc36)
    run_test("TC37 Repeated identical POST  - idempotent", tc37)

    write_section("CATEGORY 8  - Missing & Invalid Data")
    run_test("TC38 POST without 'devices' key  - state unchanged", tc38)
    run_test("TC39 Device with id=0  - accepted and retrievable", tc39)
    run_test("TC40 Duplicate IDs in same POST  - last definition wins", tc40)
    run_test("TC41 Device without smokeDetector field  - defaults applied", tc41)
    run_test("TC42 Device without radioModule field  - defaults applied", tc42)
    run_test("TC43 Null location  - stored as empty or null", tc43)
    run_test("TC44 Out-of-range SD model enum (-999 and 99)  - stored as-is (no validation)", tc44)
    run_test("TC45 Invalid ISO8601 timestamp  - stored as epoch or 0 (not crash)", tc45)
    run_test("TC46 Partial ISO8601 (date only, no time)  - graceful handling", tc46)
    run_test("TC49 lineCharacter invalid values  - rejected (set to 0/null)", tc49)
    run_test("TC50 driftState out of range (8 and 255)  - stored as-is (uint8 wraps or clamps)", tc50)
    run_test("TC51 alarmCountTotal overflow (300 > uint8 max 255)  - stored with truncation", tc51)
    run_test("TC52 Extra unknown fields in device object  - silently ignored", tc52)
    run_test("TC53 Extra unknown fields at root level  - silently ignored", tc53)
    run_test("TC54 Alarm with missing startTime/endTime  - graceful (device still stored)", tc54)
    run_test("TC55 Alarm endingReason=-1 (AlarmActive)  - stored correctly", tc55)
    run_test("TC56 radioInterference negative value  - clamped to 0", tc56)
    run_test("TC57 readoutProtocolVersion boundary values (0, 1, 255)", tc57)

    write_section("CATEGORY 9  - Device Ordering")
    run_test("TC58 Insertion order preserved  - GET returns same order as POST", tc58)
    run_test("TC59 Reorder devices  - GET reflects new order", tc59)
    run_test("TC60 Insert new device at beginning of list", tc60)
    run_test("TC61 Insert new device at end of list", tc61)
    run_test("TC62 Insert new device in middle of list", tc62)
    run_test("TC63 Move device from last to first", tc63)
    run_test("TC64 Order change persisted across disconnect (simulated by double GET)", tc64)
    run_test("TC65 Reorder-only POST (no add/delete)  - only order changes, content unchanged", tc65)

    write_section("CATEGORY 10 - Per-Device CRUD + Chunked Import")
    run_test("TC70 PUT /device  - create new device",                       tc70)
    run_test("TC71 PUT /device  - update existing device in place",         tc71)
    run_test("TC72 PUT /device  - id=0 rejected (400)",                     tc72)
    run_test("TC73 POST /device/delete  - removes device",                  tc73)
    run_test("TC74 POST /device/delete  - unknown id returns 404",          tc74)
    run_test("TC75 POST /reorder  - reorders existing devices",             tc75)
    run_test("TC76 POST /reorder  - mismatched id set rejected (400)",      tc76)
    run_test("TC77 Chunked import  - begin/chunk/chunk/commit replaces list", tc77)
    run_test("TC78 Chunked import  - concurrent begin returns 409",         tc78)
    run_test("TC79 Chunked import  - bad sessionId returns 410",            tc79)
    run_test("TC80 Chunked import  - abort discards staging, slot freed",   tc80)

    write_section("CATEGORY 11 - HA Entity-ID Rename")
    run_test("TC81 Option off  - location change leaves entity IDs untouched", tc81)
    run_test("TC82 Option on  - location change renames the entities",        tc82)
    run_test("TC83 Option switched on  - all detectors renamed",              tc83)
    run_test("TC84 Location cleared  - serial-number prefix",                 tc84)
    run_test("TC85 Same prefix after edit  - no rename",                      tc85)
    run_test("TC86 Prefix slugs and fallbacks",                               tc86)
    run_test("TC87 Client-supplied prefix is ignored",                        tc87)

    # Summary
    print("\n")
    print("=" * 60)
    colour = GREEN if _fail_count == 0 else RED
    print(f"{colour}  PASS: {_pass_count:<5} FAIL: {_fail_count:<5} SKIP: {_skip_count}{RESET}")
    print("=" * 60)

    sys.exit(1 if _fail_count > 0 else 0)


if __name__ == "__main__":
    main()
