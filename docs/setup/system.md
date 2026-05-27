---
icon: tabler/adjustments
---

# System

The System pages provide comprehensive information about the Genius Gateway's hardware status, performance metrics, diagnostic tools, and firmware management capabilities.

## :tabler-stethoscope: System Status

The System Status page displays detailed information about the gateway's current operating state, hardware configuration, and resource utilization. This information updates in real-time via WebSocket connection.

![System Status](../assets/images/software/gg-system-status.png)

### System Information

The status page displays a lot of self-explaning system related information.

### Device Management Actions

At the bottom of the System Status page, several management actions are available:

#### :tabler-reload: Restart

Performs a software restart of the device. The device will reboot and reconnect to the network automatically.

#### :tabler-refresh-dot: Factory Reset

Resets all settings to factory defaults, including:
- WiFi configuration
- User accounts
- MQTT settings
- Device and alarm line configurations
- All other custom settings

!!! danger "Permanent Action"
    Factory reset cannot be undone. All configuration data will be permanently erased. The device will restart with default system settings (from `factory_settings.ini`) and not Genius PLus X related configuration.

## :tabler-cpu: CC1101

The CC1101 page displays the current state of the CC1101 radio transceiver chip used for communication with smoke detectors. This page is only accessible to administrators and only available if the CC1101 controller feature is enabled.

![CC1101 Status](../assets/images/software/gg-system-cc1101.png)

### Main Radio Control State Machine State (MARCSTATE)

The CC1101 chip operates in various states during normal operation. The page displays the current state in the format `STATE / NAME`, where STATE represents the functional state group and NAME represents the specific MARCSTATE value.

- **SLEEP / SLEEP**: Low-power sleep mode
- **IDLE / IDLE**: Radio is idle and ready
- **XOFF / XOFF**: Crystal oscillator off
- **MANCAL / VCOON_MC**: Voltage-controlled oscillator on (manual calibration)
- **MANCAL / REGON_MC**: Regulator on (manual calibration)
- **MANCAL / MANCAL**: Manual calibration in progress
- **FS_WAKEUP / VCOON**: Voltage-controlled oscillator on
- **FS_WAKEUP / REGON**: Regulator on
- **CALIBRATE / STARTCAL**: Starting calibration
- **SETTLING / BWBOOST**: Bandwidth boost during settling
- **SETTLING / FS_LOCK**: Frequency synthesizer lock
- **SETTLING / IFADCON**: IF ADC on during settling
- **CALIBRATE / ENDCAL**: Ending calibration
- **RX / RX**: Receiving mode (normal listening state)
- **RX / RX_END**: End of receive
- **RX / RX_RST**: Receive reset
- **TXRX_SETTLING / TXRX_SWITCH**: Switching from transmit to receive
- **RXFIFO_OVERFLOW / RXFIFO_OVERFLOW**: Receive buffer overflow (data loss)
- **FSTXON / FSTXON**: Frequency synthesizer ready for transmission
- **TX / TX**: Transmitting mode
- **TX / TX_END**: End of transmit
- **RXTX_SETTLING / RXTX_SWITCH**: Switching from receive to transmit
- **TXFIFO_UNDERFLOW / TXFIFO_UNDERFLOW**: Transmit buffer underflow

### Actions

#### :tabler-reload: Update CC1101 State

Click the :tabler-reload: **Update** button to refresh the current CC1101 state from the chip.

#### :tabler-ear: Set CC1101 to RX State

Click the :tabler-ear: **Listen** button to command the CC1101 chip to enter receive (RX) mode. This button is disabled if:
- The chip is already in RX state
- An SPI communication error occurred

!!! warning "Not for regular use"
    The CC1101 chip should typically be in RX state (`RX/RX`) during normal operation to receive packets from smoke detectors. If the chip is in an unexpected state, you can try to return to RX mode.

### Error Conditions

If an error occurs, the state display will show:

- **Red indicator**: Error condition detected
- **Error message**: Description of the problem (e.g., "SPI error while obtaining state" or "Invalid state")

## :tabler-report-analytics: System Metrics

The System Metrics page provides real-time graphical visualization of system performance over time.

Metrics collection begins when the web interface connects to the device and continues while it remains open, with data displayed as line charts. Charts update automatically as new data arrives via WebSocket.

![System Metrics](../assets/images/software/gg-system-metrics.png)

!!! tip "Performance Monitoring"
    Monitor these metrics to identify resource constraints, memory leaks, or thermal issues. Sudden spikes or trends may indicate problems that need attention.

### Memory (Heap) Usage

Line chart showing:

- **Used**: Amount of heap memory currently allocated (primary line)
- **Max Alloc**: Maximum contiguous block available for allocation (secondary line)

Y-axis scale adjusts to the total heap size.

### PSRAM Usage

Line chart showing PSRAM utilization over time.

!!! info
    This chart is only displayed if PSRAM is available on the device.

### File System Usage

Line chart showing the amount of flash storage used by the file system over time.

### Core Temperature

Line chart showing the ESP32 chip temperature in degrees Celsius over time. Useful for monitoring thermal performance and detecting overheating conditions.

## :tabler-bug: Core Dump

The Core Dump page allows you to download diagnostic information captured when the device experiences a crash or exception. This data can be helpful for debugging firmware issues.

![Core Dump](../assets/images/software/gg-system-coredump.png)

### Core Dump Information

A core dump is a snapshot of the device's memory at the moment of a crash, including:

- Register states
- Stack trace
- Memory contents
- Exception cause

### Downloading Core Dumps

If a core dump is available:

1. Click the **Download Core Dump (coredump.bin)** button to download the core dump file to your computer
2. This file can be analyzed using [ESP-IDF tools :material-open-in-new:](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/api-guides/core_dump.html) to determine the crash cause

!!! info
    If no core dump is available, the message "No core dump available." is displayed.

## :tabler-database-import: Migrations

The Migrations page surfaces the state of automatic one-shot config transforms that run across firmware upgrades. The user does not have to do anything — migrations apply on first boot after an upgrade and are recorded so they never re-run.

<!-- TODO: screenshot at ../assets/images/software/gg-system-migrations.png -->

!!! info "Administrator Access Required"
    The Migrations page is only accessible to users with administrator privileges. The `GET /rest/migrations` endpoint is available to any authenticated user; only the **Retry** button (`POST /rest/migrations/retry`) requires admin.

State is persisted in `/config/migrations.json` (applied IDs with the firmware version at apply time, plus any failure records).

### States

Each migration is shown in exactly one of four states:

| State | Icon | Meaning |
|---|---|---|
| **Applied** | :tabler-circle-check: | Ran successfully and was recorded. Will never re-run. |
| **Failed** | :tabler-circle-x: | Ran but its apply step returned an error. Retries on the next reboot unless its failure policy is `skipAfterRetries` and the attempt counter has been exceeded. |
| **Pending** | :tabler-clock: | Registered, never applied, precondition currently true — the migration will run on the next applicable boot trigger. |
| **Not applicable** | :tabler-circle-minus: | Registered, never applied, precondition currently false on this device. Typical for migrations whose legacy source file does not exist (e.g. on a fresh install). |

In normal steady-state operation after a successful upgrade, the table contains only **Applied** rows for completed migrations and **Not applicable** rows for migrations whose preconditions never matched this particular device.

### Failure Handling

Each migration is tagged with one of three failure policies:

| Policy | Behaviour on failure |
|---|---|
| `retryNextBoot` (default) | Migration left unmarked. Retries on the next reboot. Use for transient failures. |
| `skipAfterRetries` | Retries up to `maxAttempts` boots, then records as given up. The **Retry** button can re-enable it. |
| `abortBoot` | Boot is halted with a loud error log. The device cannot run in a half-migrated state. Manual recovery required (factory reset or fresh flash). |

If any migration is in the **Failed** state, a warning banner appears at the top of the page. Pending migrations tagged `abortBoot` are additionally marked with a small **critical** badge.

### Retry

Admins can clear the failure history with **Retry on next reboot**. This removes every entry from the failure list in `/config/migrations.json` — but does **not** mark anything as applied and does **not** run migrations synchronously. The device must then be rebooted; on boot, the run loop encounters the migrations as eligible (not applied, not failed) and tries them again. Migrations are designed to be idempotent, so re-running after a partial failure is safe.

### Currently Shipped Migrations

| ID | Introduced in | Purpose |
|---|---|---|
| `v1.3-split-mqtt-settings-ha` | v1.3.0 | Splits the legacy `/config/mqtt-settings.json` into the dedicated `/config/haSettings.json` for Home Assistant integration flags. |
| `v1.3-split-mqtt-settings-alarm` | v1.3.0 | Splits the legacy `/config/mqtt-settings.json` into the dedicated `/config/alarm-publishing.json`. |
| `v1.3-drop-legacy-mqtt-settings` | v1.3.0 | Once both successor files exist, removes the obsolete `/config/mqtt-settings.json`. |

All three are `retryNextBoot` and only execute on devices that still have the pre-v1.3.0 file on disk. On a fresh v1.3.0+ install they show as *Not applicable*.

### How it works

The migration runner has two phases:

- **PreServiceBegin** — fires once inside `ESP32SvelteKit::begin()`, right after the filesystem is mounted but before any settings service reads its persisted config. Used for file-level transforms (rename, split, reshape on disk).
- **PostServiceBegin** — fires after all settings services have started. Used for cleanup that depends on successor files being present (e.g. dropping a legacy file once the services that read its data have rewritten their own).

Each migration declares a stable, version-prefixed ID that is never renamed (e.g. `v1.3-split-mqtt-settings-ha`), a cheap precondition (`shouldRun`), an idempotent apply function, a failure policy, and a phase + order. Source: `src/migrations/MigrationService.h`, `src/migrations/GatewayMigrations.cpp`, `src/migrations/MigrationApi.h`.

## :tabler-refresh-alert: Firmware Update

The Firmware Update page provides two methods for updating the gateway firmware: downloading releases from GitHub or manually uploading firmware files.

![Firmware Update](../assets/images/software/gg-system-firmware.png)

!!! info "Administrator Access Required"
    Firmware update features are only accessible to users with administrator privileges.

### :tabler-brand-github: GitHub Firmware Manager

The GitHub Firmware Manager lists all firmware releases published to the project's GitHub repository and lets you install any of them with one click.

#### Release List

Each release row shows the version tag and a list of available firmware assets. When the device's current version is known, the row matching the installed version is highlighted — indicating that version is already running.

#### Build Target Filtering

The device reports its build target (e.g. `seeed-xiao-esp32s3`) as part of the API response. Assets whose filename contains the build target are marked as **compatible**; any others are **incompatible**.

By default only compatible assets are shown. If a release contains incompatible assets a toggle labelled **Hide incompatible build targets** appears above the list. It is checked by default, hiding incompatible assets; unchecking it reveals them.

!!! info "No toggle shown"
    If all assets across all releases are compatible with the device, the toggle is not displayed at all.

#### Installing a Release

1. Find the release you want and click the install button next to the desired `.bin` asset.
2. A confirmation dialog appears listing the target URL.
    - For a **compatible** asset the dialog uses the standard warning style.
    - For an **incompatible** asset the dialog uses a red/error style and warns that the firmware was not built for this hardware variant.
3. Click **Update** (or **Update anyway** for incompatible assets) to start the download. The device fetches the binary from GitHub, writes it to flash, and restarts automatically.

### :tabler-file-upload: Upload Firmware

The Upload Firmware section allows manual installation of firmware files from your local computer.

#### Firmware Validation

The upload service performs several validation checks to ensure firmware safety:

- **File Type Verification**: Only `.bin` (firmware binary) and `.md5` (checksum) files are accepted
- **Firmware Size Check**: Minimum size of 1 MB required (for the `.bin` file) to be recognized as valid firmware
- **Target Hardware Validation**: The firmware header is checked to ensure it matches the device's chip type (ESP32, ESP32-S2, ESP32-C3, or ESP32-S3)

    !!! warning "Target Compatibility"
        Firmware compiled for a different ESP32 chip variant will be rejected during upload. For example, firmware built for ESP32-S3 cannot be uploaded to an ESP32-S2 device.

- **MD5 Checksum**: If an `.md5` file is uploaded before the firmware, the binary is verified against the checksum during upload

#### Supported File Types

- **.bin**: Compiled firmware binary file (minimum 1 MB)
- **.md5**: MD5 checksum file (32-character hex string)

The `.md5` file should contain only the 32-character MD5 hash of the corresponding `.bin` file.

#### Uploading Firmware

To upload firmware manually:

1. **(Optional) Upload MD5 checksum first**:
    - Click **Choose File** and select the `.md5` file
    - The checksum is stored and will be used to verify the firmware

2. **Upload the firmware binary**:
    - Click **Choose File** and select the `.bin` file
    - A confirmation dialog appears: "Are you sure you want to overwrite the existing firmware with a new one?"
    - Click **Upload** to confirm and begin the installation

3. **Firmware Installation**:
    - The device validates the firmware header
    - If an MD5 checksum was provided, the firmware is verified against it
    - The firmware is written to the device's flash memory
    - Upon successful completion, the device automatically restarts and executes the newly installed firmware

!!! danger "Failure Recovery"
    If the firmware validation or flashing fails or the device becomes unresponsive after an update:
    
    - **After upload**: If validation fails (wrong chip type, MD5 mismatch), the upload is rejected and the current firmware remains active
    - **During flashing**: If persisting the new firmware to flash memory fails, the current firmware remains active after restart
    - **After restart**: If the new firmware crashes or fails to boot properly, the device will likely enter a boot loop. Then a manual recovery is required, i.e. flashing a working firmware via USB using the [build and flash instructions](general-setup.md#flash-software).
    
    !!! warning "No Automatic Rollback"
        The firmware does not implement automatic rollback to the previous version. Always test new firmware builds on a development device before updating production devices.
