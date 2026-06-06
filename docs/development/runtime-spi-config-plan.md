# Runtime SPI / CC1101 Pin Configuration — Implementation Plan

Branch: `feature/spi-config-runtime` (from `v1.4.0`)

Goal: move the CC1101 SPI/GDO pins from compile-time `-D CONFIG_*_GPIO` flags to a
backend-persisted, UI-editable configuration. The gateway boots into an *unconfigured*
state when no valid pin set exists, the radio is only brought up once configured, a
self-test verifies the wiring, and the radio/SPI state is reflected live in the Web UI
(overview banner + top-bar indicator).

---

## 1. Current state (what we're changing)

Pins are compile-time macros baked deep into the driver:

- Definitions: [config.ini](../../config.ini) per board env (`esp32-s3-devkitc-1`,
  `esp32-s3-devkitc-1-n8r2`, `seeed-xiao-esp32s3`) — `CONFIG_CSN/MISO/GDO0/MOSI/SCK_GPIO`,
  plus `-D HOST_ID=1` (SPI host).
- Driver [src/cc1101.c](../../src/cc1101.c):
  - `CC1101_SELECT()/DESELECT()` macros (`CONFIG_CSN_GPIO`) — lines 54-56
  - `wait_miso_low()` (`CONFIG_MISO_GPIO`), `wait_gdo0_high/low()` (`CONFIG_GDO0_GPIO`) — 66-108
  - `cc1101_spi_init()`: `gpio_set_direction(CONFIG_CSN_GPIO…)`, `buscfg.{sclk,mosi,miso}_io_num`,
    `spi_bus_initialize(HOST_ID…)`, `spi_bus_add_device` — 275-301
  - `cc1101_init()`: GDO0 `gpio_config` + `gpio_install_isr_service` + `gpio_isr_handler_add` — 511-519
- Consumer [src/CC1101Controller.cpp](../../src/CC1101Controller.cpp): reads
  `CONFIG_GDO0_GPIO` directly (lines 74, 94).
- Bring-up: `cc1101_init(nofifyReceivedPacket)` is called **unconditionally** at boot in
  [src/GeniusGateway.cpp:106](../../src/GeniusGateway.cpp). CC1101Controller is started and
  RX monitoring enabled at lines 134-138.
- Existing UI: read-only MARCSTATE page at
  [interface/src/routes/system/cc1101/+page.svelte](../../interface/src/routes/system/cc1101/+page.svelte),
  gated by `page.data.features.cc1101_controller && $user.admin`; menu entry in
  [menu.svelte:154](../../interface/src/routes/menu.svelte).

---

## 2. Single source of truth for targets & pins

Requirement: target pin catalogs maintained **once**, consumed by both firmware
(defaults + validation) and frontend (pickers) with no hand-kept redundancy.

Design: the **firmware header is the single source of truth** — no JSON, no codegen. The
frontend never reads the catalog directly; it fetches the active profile from the backend
via REST, so nothing outside the firmware needs the raw data.

1. **Author once** — `src/cc1101_pin_profiles.h`, a hand-maintained `static const` table
   (not `#define`s, which are awkward for per-GPIO metadata). The active target is selected
   by the board macro already set per env in [config.ini](../../config.ini):
   ```c
   typedef struct { uint8_t num; const char *label; bool input, output, reserved; } cc1101_gpio_t;
   typedef struct {
       const char *label; int spi_host;
       cc1101_pins_t defaults;
       const cc1101_gpio_t *gpios; size_t gpio_count;
   } cc1101_pin_profile_t;

   #if defined(BOARD_SEEED_XIAO_S3)
   static const cc1101_gpio_t PROFILE_GPIOS[] = { {1,"D0",true,true,false}, /* … */ };
   static const cc1101_pin_profile_t PIN_PROFILE = {
       "Seeed XIAO ESP32-S3 (GG-1.0)", SPI2_HOST, {5,8,9,7,6}, PROFILE_GPIOS, /* count */ };
   #elif defined(BOARD_ESP32_S3_DEVKITC_1)
   // …
   #endif
   ```
   The per-board `CONFIG_*_GPIO` flags move out of config.ini into this header's `defaults`;
   config.ini keeps only a `-D BOARD_*` selector per env. The header is **compile-checked**.
2. **Runtime exposure** — backend serializes the active `PIN_PROFILE` struct to JSON in
   `GET /rest/cc1101/pin-profile` (label, spiHost, defaults, gpios, reserved).
3. **Frontend consumes at runtime** — the config page fetches `/rest/cc1101/pin-profile`
   and builds pickers/validation from it. A device only ever exposes the one profile that
   matches its firmware → zero redundancy, single authoring point.

Result: editing `cc1101_pin_profiles.h` updates firmware defaults/validation **and** the UI
pickers from one place. (A JSON catalog + build-time codegen was considered and rejected: it
adds a Python build step and a generated artifact while buying nothing, since no build-time
or host-side consumer of the catalog exists — the frontend reads it over REST. Revisit only
if an offline consumer, e.g. a docs page listing all boards, ever appears.)

---

## 3. Backend

### 3.1 Driver refactor — runtime pins (`cc1101.c` / `cc1101.h`)

Replace all `CONFIG_*_GPIO` / `HOST_ID` macro uses with a runtime struct held in the driver:

```c
typedef struct {
    int csn, miso, mosi, sck, gdo0;   // gpio_num_t
    int spi_host;                      // SPI2_HOST / SPI3_HOST
} cc1101_pins_t;
```

- `cc1101_init()` gains a `const cc1101_pins_t *pins` parameter.
- Static `_pins` copy stored in the driver; `CC1101_SELECT/DESELECT` and the `wait_*`
  helpers become inline functions reading `_pins`.
- Keep `PIN_PROFILE.defaults` (from `cc1101_pin_profiles.h`) available for the *seed*, not
  as live wiring.

### 3.2 Keystone: `cc1101_deinit()`

A fully-releasing teardown so init can be called repeatedly (consumed by **both** the
self-test and live re-init):

- `spi_bus_remove_device(_handle)` → `spi_bus_free(host)`
- `gpio_isr_handler_remove(gdo0)` (and `gpio_uninstall_isr_service` only if we own it)
- reset CSN/GDO0 GPIOs (`gpio_reset_pin`)
- clear `_handle`, `_mode = CCM_IDLE`, edge timestamps
- idempotent / safe to call when never initialized

> This is the highest-risk code (leaked bus handles / dangling ISR). Get it bulletproof
> first; everything else builds on it.

### 3.3 Self-test — `cc1101_probe(const cc1101_pins_t*, cc1101_probe_result_t*)`

Transient init → checks → deinit, on **candidate** pins:

- Read `VERSION` (status reg 0x31) and `PARTNUM` (0x30) over SPI. A sane `VERSION`
  (e.g. 0x14/0x04 depending on chip rev) proves SCK/MOSI/MISO/CSN wiring.
- Drive `IOCFG0` to a known output and read GDO0 to validate that line.
- Return per-function result so the UI can distinguish "no chip / SPI wiring wrong" vs
  "GDO0 wrong". In the UNCONFIGURED state this runs with no teardown of a live radio; to
  probe *different* pins while a config is already live, release current via `cc1101_deinit()`
  first (same primitive).

### 3.4 Settings service — `CC1101PinsService`

Follow the existing `StatefulService<T>` pattern
([GatewaySettingsService](../../src/GatewaySettingsService.h)) with
`HttpEndpoint` + `EventEndpoint` + `FSPersistence`:

- File: `/config/cc1101-pins.json`; REST: `/rest/cc1101-pins`; event: `cc1101-pins`.
- Model: `csn, miso, mosi, sck, gdo0, spiHost`, plus `configured` flag.
- **Defaults seeded from `PIN_PROFILE.defaults`** so a fresh flash on a known board can
  optionally auto-configure (decide: seed-as-configured vs seed-as-suggestion; recommend
  seed values present but `configured=false` until the user confirms/tests once — except in
  a migration path for existing installs, see §3.7).
- On update: validate (§3.5) → persist → trigger radio re-init (§3.6).

### 3.5 Validation (backend authoritative)

Backend rejects bad saves regardless of UI, using SoC truth + profile:

- `GPIO_IS_VALID_GPIO(n)` for all; `GPIO_IS_VALID_OUTPUT_GPIO(n)` for CSN/SCK/MOSI.
- No duplicate pins across the five functions.
- Reject pins in the profile `reserved` list (flash/PSRAM/USB) and any pin used by the
  system (compare against known WiFi-less ESP32 reserved set).
- `spiHost` ∈ {SPI2_HOST, SPI3_HOST}.
- Return structured 400 with per-field errors for the form to render.

### 3.6 Radio state machine + live re-init (no reboot)

Introduce an explicit radio state owned by the driver/controller:
`UNCONFIGURED → INITIALIZING → OK → ERROR` (+ back to INITIALIZING on reconfigure).

- Boot: if `CC1101PinsService.configured == false` → stay `UNCONFIGURED`, **do not** call
  `cc1101_init`. [GeniusGateway.cpp:106](../../src/GeniusGateway.cpp) becomes conditional.
- Reconfigure at runtime (preferred over reboot — see decision log §6):
  1. Take `ThreadSafeService` transaction lock (CC1101Controller already is one).
  2. Quiesce: disable RX monitoring, pause/skip the RX path & ISR work.
  3. `cc1101_deinit()` → `cc1101_init(newPins)` → on success `cc1101_set_rx_state()`.
  4. Resume RX monitoring; set state OK or ERROR.
- Emit state on every transition via the EventSocket (`registerEvent`/`emitEvent`, as done
  for `GATEWAY_EVENT_ALARM` in GeniusGateway.cpp) under event name `cc1101_status`
  carrying `{ state, chipVersion, lastError }`. Also expose `GET /rest/cc1101/status`.

### 3.7 Migration (existing installs must not break)

Use the existing migration framework ([src/migrations/](../../src/migrations/)):

- On first boot after update with **no** `/config/cc1101-pins.json`: write the file from the
  `PIN_PROFILE.defaults` **with `configured=true`** so already-deployed devices
  keep their working pins seamlessly. (New/unknown boards remain `UNCONFIGURED`.)

### 3.8 Consumers must handle non-OK state

- `CC1101Controller::loop()` must no-op when not OK (today it reads `CONFIG_GDO0_GPIO`
  unconditionally — lines 74/94).
- TX paths, packet visualizer, AlarmLinesService, GeniusDevicesService, MQTT publishing:
  guard against `state != OK`, fail soft, avoid log spam.
- `enableRXMonitoring()` only meaningful once OK.

### 3.9 Feature flag

Keep `FT_CC1101_CONTROLLER` / `addFeature("cc1101_controller", true)`. The config page and
indicator are gated the same way as the existing CC1101 page.

---

## 4. Frontend

### 4.1 Status store + EventSocket wiring

- New store `interface/src/lib/stores/cc1101.svelte.ts` (or extend `telemetry.ts`): holds
  `{ state, chipVersion, lastError }`.
- Subscribe in `+layout.svelte` via `socket.on('cc1101_status', …)` (same mechanism as the
  existing telemetry events), plus an initial `GET /rest/cc1101/status` fetch.
- Add `CC1101State`/`CC1101Status`/`CC1101Pins`/`PinProfile` types to
  `interface/src/lib/types/models.ts`.

### 4.2 Top-bar indicator

- New `interface/src/lib/components/RadioIndicator.svelte`, placed in
  [statusbar.svelte](../../interface/src/routes/statusbar.svelte) next to `AlarmStatus`.
- Icon states: OK (cpu/antenna), UNCONFIGURED (gear/dashed, muted), INITIALIZING (spinner),
  ERROR (alert, `text-error`). Tooltip with detail; click → `/system/cc1101`.
- Gated by `page.data.features.cc1101_controller`.

### 4.3 Overview banner (unconfigured / error)

- New `interface/src/lib/components/RadioSetupBanner.svelte` rendered at the top of the
  overview [+page.svelte](../../interface/src/routes/+page.svelte) when state is
  UNCONFIGURED or ERROR: an `alert` explaining the radio isn't ready with a CTA to the
  config page. Hidden when OK.

### 4.4 Config page

- New route `interface/src/routes/system/cc1101/config/+page.svelte` (or expand the existing
  cc1101 page into tabs: *Status* + *Pin Configuration*), admin + feature gated.
- Fetch `/rest/cc1101/pin-profile`; render five pin pickers (CSN/MISO/MOSI/SCK/GDO0) +
  optional SPI host, each constrained to the profile's `gpios`, reserved pins disabled with
  a reason, duplicates flagged inline.
- Buttons: **Test** (POST candidate pins to a probe endpoint → render per-function
  pass/fail), **Save** (persist via `/rest/cc1101-pins`). Live status badge from the store.
- Reuse `SettingsCard`, follow the form patterns; honor the planned dirty-flag/validation
  work in TODO-v1.4.0.
- "Reset to compiled defaults" action (re-reads profile defaults).

### 4.5 Menu

- Keep/relabel the CC1101 menu entry; ensure config is reachable for admins.

---

## 5. Phasing (incremental, each independently testable)

1. **SSoT plumbing** — `src/cc1101_pin_profiles.h` table + `-D BOARD_*` selector per env.
   Remove `CONFIG_*_GPIO` from config.ini (move into the header's `defaults`). Build still
   uses defaults → no behavior change.
2. **Driver refactor** — `cc1101_pins_t`, runtime pins, `cc1101_init(pins)`. Seed from
   profile defaults at boot. Verify radio works exactly as before.
3. **`cc1101_deinit()`** — the keystone teardown; prove re-init by calling
   deinit→init once at boot.
4. **Settings service + migration** — persist pins, seed existing installs as configured.
5. **State machine + lazy init + consumer guards** — boot unconfigured when no config;
   EventSocket `cc1101_status`; `/rest/cc1101/status`.
6. **Self-test (`cc1101_probe`) + probe endpoint**.
7. **Live re-init** under the mutex with RX quiescing.
8. **Frontend** — store + layout wiring, top-bar indicator, overview banner, config page,
   menu.
9. **Docs** — update [general-setup.md](../../docs/setup/general-setup.md) from
   "edit config.ini" to UI-based config.

## 6. Decision log

- **Live re-init, not reboot-after-save.** The risky SPI/ISR teardown has to exist anyway
  for the self-test (`cc1101_deinit` is shared), so reboot buys little. Remaining live-only
  risk = quiescing the RX task/ISR during teardown, handled with the existing
  `ThreadSafeService` mutex. Reboot remains an acceptable fallback if a clean live teardown
  proves unstable on hardware.
- **Validation is required regardless of approach** and is no argument for reboot — a bad
  pin set is equally damaging in the compile-time version; the UI can validate where a `-D`
  flag cannot.
- **Single source of truth = hand-maintained `cc1101_pin_profiles.h` (firmware) + REST
  (frontend).** No JSON, no codegen: the frontend consumes the profile over REST, so nothing
  outside the firmware needs the raw catalog, and the header is compile-checked. JSON+codegen
  was rejected as indirection that buys nothing here.
- **Pin catalog = curated allowlist** in each profile's `gpios[]` (labelled, with
  input/output/reserved flags), not a derived valid-minus-reserved list. Pins not listed are
  not assignable. Any default pin MUST appear in its own catalog (e.g. DevKitC-1 CSn=45, a
  strapping pin, is listed because it is the wired default).
- **Fixed-PCB boards are locked.** `cc1101_pin_profile_t.configurable` = false for the
  soldered GG-1.0 (catalog = its 5 hardwired pins, shown read-only); true for the DevKitC-1
  (DIY wiring, full picker). The settings service must refuse pin changes when
  `configurable == false`.
- **Three board cases, with an Expert escape hatch (`allow_expert`).**
  1. *Fixed PCB* (GG-1.0): locked, curated 5-pin catalog.
  2. *Documented dev boards* (DevKitC-1, easy to add as a new `#if` branch): curated
     allowlist + Expert toggle available.
  3. *Undocumented clones*: a dedicated **`BOARD_GENERIC_S3`** target (no trusted defaults →
     boots UNCONFIGURED, forces pick + self-test) **and** the in-UI Expert toggle on any
     `allow_expert` board. Decision was "Both".
  Expert mode is **chip-aware, not board-aware**: its pin set is *derived at runtime* from
  `esp_chip_info` + PSRAM/flash mode + `GPIO_IS_VALID_OUTPUT_GPIO()` — the one place deriving
  beats curating, because the board is unknown but the chip is known. Even in Expert mode,
  flash/PSRAM/USB pins stay **hard-blocked** (a wrong pick there hangs the board, not "user's
  risk"); strapping pins are **allowed-with-warning**. This makes the **self-test the
  load-bearing validation for case 3** (VERSION read confirms a guessed pinout).
  TODO (later phases): add the `generic-esp32s3` PlatformIO env; runtime-derived Expert pin
  set + reserved/strapping classifier; per-chip descriptor when non-S3 chips are added.

## 7. Open questions

- Seed-as-configured vs seed-as-suggestion for a **fresh flash** on a known board (migration
  path for *existing* installs is seed-as-configured either way).
- Expose GDO2 slot now (currently unused) or leave out?
- SPI host: expose to user or pin to the profile's value?
- Reserved-pin source on the backend: rely on the profile `reserved` list, or also query
  `esp_gpio` reservations at runtime where available?
