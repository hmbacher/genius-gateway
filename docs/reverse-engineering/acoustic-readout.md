# Acoustic Readout

Hekatron Genius Plus X smoke detectors expose their internal diagnostic snapshot via an acoustic signal that is emitted on demand. The signal is centered around **4.45 kHz** and carries a complete dump of the device's identity, lifetime statistics, fault flags, and (if a radio module is fitted) the radio module's identity, configuration, and link status. Hekatron markets this mechanism as **SmartSonic**. This page documents the modulation, framing, and payload format as reverse-engineered from the official Android service app (the SmartSonic Tuner package) and re-implemented in TypeScript at [`interface/src/lib/audio/tuner-pipeline.ts`](https://github.com/hmbacher/genius-gateway/blob/main/interface/src/lib/audio/tuner-pipeline.ts).

!!! warning "Disclaimer"
    The information here is the result of reverse-engineering and observation. It is internally consistent and matches every test vector the project has captured, but it has not been validated against any official specification. Field meanings, especially within bitmasks, may be incomplete or partially mislabelled. Use at your own risk.

## Signal Characteristics

| Parameter | Value |
|-----------|-------|
| Carrier (NCO center) frequency | 4,449.1 Hz |
| Modulation | Continuous-Phase FSK (CPFSK), space/mark = ±250 Hz |
| Bit rate | ~ 84 bit/s (≈ 11.92 ms / bit) |
| Required ADC sample rate | ≥ 11 kHz; 44.1 kHz used by the reference decoder |
| Forward error correction | Hamming(8,4) per nibble (one-bit correction per 4 data bits) |
| Frame check | CRC-16/IBM (poly `0x8005`, init `0xFFFF`) over payload |
| Frame structure | `[length:1][payload:0..39][crc:2 LE]` |
| Typical session length | ~7–9 s (≈0.29 s sync preamble + ~6.1 s payload at 84 bit/s + silence padding) |

The signal lives entirely in the audible band — there is no actual ultrasound — which is why a phone, laptop, or tablet microphone is sufficient to capture it.

## Demodulation Pipeline

The reference decoder is implemented as a streaming TypeScript pipeline that runs in the browser. Each new audio sample passes through the chain below; the chain is pure DSP and does not use an FFT.

| # | Stage | Input → Output |
|---|-------|----------------|
| 1 | **NCO Heterodyne** | 44.1 kHz PCM → complex I/Q, baseband ±250 Hz |
| 2 | **IIR Band-Pass** | I/Q → noise-filtered I/Q (2 cascaded biquad stages) |
| 3 | **Decimate ×8** | 44.1 kHz I/Q → 5,512.5 Hz I/Q |
| 4 | **Phase Discriminator** | I/Q → instantaneous frequency error (arctan-derivative) |
| 5 | **Median + IIR Smoothing** | Noisy frequency error → smoothed frequency error |
| 6 | **Sync Correlator** | Smoothed signal → sync-locked / searching (SYNC1 or SYNC2 pattern) |
| 7 | **Bit Correlator** | Locked stream → raw bits (64-sample template, dual-polarity) |
| 8 | **Hamming(8,4) Decode** | 8-bit codewords → data nibbles (1-bit error correction per nibble) |
| 9 | **Frame Assembler** | Decoded bytes → `[length][payload][CRC-16/IBM]` |
| 10 | **Payload Parser** | Frame bytes → `TunerData` fields |

### 1. Heterodyning

Each input sample is mixed with a digitally-generated complex carrier at 4449.1 Hz, producing a baseband I/Q pair around DC. This is what shifts the FSK deviation from `[4199.1 Hz, 4699.1 Hz]` to `[−250 Hz, +250 Hz]`.

### 2. IIR Band-Pass + Decimation

Two cascaded biquad-style IIR stages remove out-of-band noise and DC. After filtering, the I/Q stream is decimated by 8, giving an effective working rate of **5,512.5 Hz**. The filter coefficients are hard-coded (`IIR_A1/B1`, `IIR_A2/B2`, `IIR_A_SMOOTH/B_SMOOTH` in `tuner-pipeline.ts`).

### 3. Phase Discriminator

An arctan-derivative discriminator turns I/Q into an instantaneous frequency error:

`phaseErr = (ΔQ · I_prev − ΔI · Q_prev) / (I² + Q²)`

The output is clamped to ±400 (the `CLAMP` constant) and median-filtered (window 5) before being correlated against the bit and sync templates.

### 4. Sync Detection

Two alternating sync preambles are searched for in parallel:

- **SYNC1** — 1,576 decimated samples, segment-encoded in `buildSyncPattern1()`
- **SYNC2** — 1,578 decimated samples, segment-encoded in `buildSyncPattern2()`

A correlation magnitude exceeding `SYNC_THRESHOLD = 300,000` (and exceeding the opposite-polarity correlation) declares lock. The decoder reports a **sync quality** percentage:

`quality = round(100 · maxCorr / (CLAMP · sync_length))`

Sub-sync sequences (`SubSync1g/h/i`, `SubSync2g/h/i`) provide fine timing alignment within each pattern set.

### 5. Bit Recovery

After sync, the decoder slides a 64-sample bit template

`[ 0×3, −1×26, 0×6, +1×26, 0×3 ]`

across the discriminator output. The template is correlated against both polarities; the higher-magnitude polarity wins, yielding one bit. Symbol timing is refined by accumulating 16 consecutive bit correlations and taking a weighted median of their offsets.

### 6. Hamming(8,4) Byte Decoding

Each transmitted byte is two Hamming(8,4) codewords (low nibble first, then high nibble). The codebook is the 16-entry `CODEBOOK = [0, 135, 153, 30, 170, 45, 51, 180, 75, 204, 210, 85, 225, 102, 120, 255]`. Decoding selects the codebook entry with the smallest Hamming distance from the received 8-bit codeword, allowing **1-bit error correction per nibble**. There is no rejection on Hamming distance — any closest-codeword wins.

### 7. Frame Assembly

The byte stream is fed into a three-state machine:

| State | Action |
|-------|--------|
| `LENGTH` | Read one byte. If ≥ 40, abort with error. Otherwise expect that many payload bytes. |
| `DATA` | Buffer payload bytes. |
| `CHECKSUM` | Read two bytes (low-byte first), compute CRC-16/IBM over the buffered payload, compare. Mismatch → error. |

A successful frame is handed to the payload parser. There is no retransmission and no multi-frame agreement; one good frame ends the session.

## Payload Format

All current-generation Genius Plus X devices emit **protocol version 5**. A complete readout (with radio module) is **32 bytes**; a smoke detector without a radio module emits **20 bytes** (bytes 0–19 only).

### Compact view (32-byte payload)

```
Byte | 0  | 1-4         | 5  | 6  | 7  | 8  | 9-10  | 11-12 | 13-14 | 15-16 | 17-18 | 19 |
Hex  | 05 | XX XX XX XX | XX | XX | XX | XX | XX XX | XX XX | XX XX | XX XX | XX XX | XX |
Field| ver| SD-Serial   | typ| del| alm| 3m | lAlm  | prdAge| store | lSlf  | wrnty | st |

Byte | 20 | 21-24       | 25-28       | 29  | 30  | 31  |
Hex  | XX | XX XX XX XX | XX XX XX XX | XX  | XX  | XX  |
Field| rs | RM-Serial   | Line-ID     | line| sw  | int |
```

### Detailed Field Reference

| Offset | Length | Field | Type | Description |
|:------:|:------:|:------|:-----|:------------|
| 0 | 1 | `protocolVersion` | `u8` | Currently observed value: `0x05`. The decoder does not validate this byte but stores it on the resulting device record (`readoutProtocolVersion`). |
| 1–4 | 4 | `serialNumber` | `u32` BE | Smoke detector serial number, big-endian. |
| 5 [3:0] | ½ | `productType` | `u4` | `0` = Genius H, `1` = Genius Hx, `2` = Genius Plus, `3` = Genius Plus X. |
| 5 [7:4] | ½ | `radioProductType` | `u4` | `0` = no FM module, `1` = FM Basis, `2` = FM Pro, `3` = FM MCP, `4` = FM Basis X, `5` = FM Pro X. |
| 6 | 1 | `deinstallationCount` | `u8` | Number of times the detector has been removed from its mounting plate. |
| 7 | 1 | `alarmCount` | `u8` | Lifetime alarm count. |
| 8 | 1 | `alarmCountLast3Months` | `u8` | Alarm count in the trailing 90-day window. |
| 9–10 | 2 | `lastAlarmOffset` | `u16` LE | Days from production to the most recent alarm. `0xFFFF` = never alarmed. |
| 11–12 | 2 | `productionAge` | `u16` LE | Days elapsed since the production date. The decoder uses **the current wall-clock time** as the reference point (see [Date Reconstruction](#date-reconstruction)). |
| 13–14 | 2 | `hoursInStorageMode` | `u16` LE | Cumulative hours the detector has spent in transport / storage mode. |
| 15–16 | 2 | `lastSelftestOffset` | `u16` LE | Days from production to the most recent self-test. `0xFFFF` = no self-test recorded. |
| 17–18 | 2 | `warrantyFlagsRaw` | `u16` LE | Bitmask of warranty-voiding conditions. See [Warranty Flags](#warranty-flags). |
| 19 | 1 | (status byte) | `u8` | Smoke detector fault and drift state. See [Status Byte](#status-byte-byte-19). |
| 20 | 1 | `radioStateMask` | `u8` | Radio module link & fault flags. See [Radio State Mask](#radio-state-mask-byte-20). Only present if the radio module is fitted. |
| 21–24 | 4 | `radioSerialNumber` | `u32` BE | Radio module serial number, big-endian. `0` = no radio module. |
| 25–28 | 4 | `lineId` | `u32` BE | Alarm line identifier the detector is commissioned to. `0` = unassigned, `0xFFFFFFFF` = broadcast/global. |
| 29 [7:4] | ½ | `lineCharacter` | `u4` → char | Maps `0..9` to `'A'..'J'`. Identifies the alarm-line letter. |
| 29 [3:0] | ½ | `lineNumber` | `u4` | Alarm-line numeric suffix (`0..15`). |
| 30 | 1 | `radioSwitchMask` | `u8` | DIP-switch / configuration bits. See [Radio Switch Mask](#radio-switch-mask-byte-30). |
| 31 | 1 | `radioInterference` | `u8` | Interference level. The UI scales values `> 0` by 1/10 and renders the result as a percentage (so byte `25` → `2.5%`). |

#### Status Byte (byte 19) {#status-byte-byte-19}

| Bit | Mask | Field | Meaning when set |
|:---:|:----:|:------|:-----------------|
| 0 | `0x01` | `batteryLowFault` | Battery voltage below the low-battery threshold. |
| 1 | `0x02` | `deviceFault` | Generic internal hardware fault. |
| 2 | `0x04` | `radioNetworkFault` | Radio mesh / link integrity issue (mirrored from the radio module). |
| 3–6 | `0x78` | `driftState` | Smoke chamber drift indicator. Observed values: `0` = OK, `2` = warning, `4` = defect. Other values are unspecified. |
| 7 | `0x80` | `dirtForecastNegative` | Predictive dust accumulation will cause failure before end of warranty. |

#### Warranty Flags (bytes 17–18, LE) {#warranty-flags}

If `warrantyFlagsRaw == 0`, the warranty is fully intact and the parser surfaces a synthetic flag `WarrantyPossible`. Otherwise, each set bit corresponds to a specific voiding reason:

| Bit | Mnemonic | Trigger |
|:---:|:---------|:--------|
| 0 | `MaxDirty` | Maximum allowed contamination exceeded. |
| 1 | `OutOfTemp` | Operated outside the rated ambient temperature range. |
| 2 | `DetectorTooOld` | Calendar age exceeds the warranty period. |
| 3 | `StorageTimeExceeded` | Spent too long in storage mode before activation. |
| 4 | `ActivationTimeExceeded` | Time since activation has exceeded the limit. |
| 5 | `TooManyEvents` | Total event counter exhausted. |
| 6 | `TooManyAlarms` | Real-alarm counter exhausted. |
| 7 | `TooManyFaults` | Internal fault counter exhausted. |
| 8 | `TooManySelfTests` | Self-test counter exhausted. |
| 9 | `TooManyRadioFaults` | Radio module fault counter exhausted. |
| 10 | `TooManyRadioOutOfOrderEvents` | Radio out-of-order events counter exhausted. |
| 11 | `RadioInstallationTooOld` | Radio module installation age exceeded. |
| 12 | `TooMuchRadioActivity` | Radio activity counter exhausted. |
| 13 | `TooMuchRadioInterference` | Cumulative interference counter exhausted. |
| 14 | `TooManyRadioTxEvents` | Radio TX event counter exhausted. |
| 15 | `TooManyRadioRxEvents` | Radio RX event counter exhausted. |

#### Radio State Mask (byte 20) {#radio-state-mask-byte-20}

| Bit | Mask | Mnemonic | Meaning when set |
|:---:|:----:|:---------|:-----------------|
| 0 | `0x01` | `FmFault` | Radio module reports a generic fault. |
| 1 | `0x02` | `TransmissionRangeTest` | A range test is currently active. |
| 2 | `0x04` | `Selftest` | The radio module is performing a self-test. |
| 3 | `0x08` | `FmBatteryLowFault` | Radio module battery low. |
| 4 | `0x10` | `RemoteBattLow` | Battery-low reported by a remote (linked) device. |
| 5 | `0x20` | `RemoteError` | Generic error reported by a remote device. |
| 6 | `0x40` | `RadioLinkError` | Radio link layer error (packet loss / supervision miss). |
| 7 | `0x80` | `RemoteAlarm` | Alarm triggered remotely (collective alarm propagation). |

#### Radio Switch Mask (byte 30) {#radio-switch-mask-byte-30}

These bits mirror the DIP-switch configuration on the FM Basis X module. Bits 0 and 1 are reserved.

| Bit | Mask | Mnemonic | Function |
|:---:|:----:|:---------|:---------|
| 2 | `0x04` | `ReducedTransmittingPower` | TX output is reduced (battery saving / regulatory). |
| 3 | `0x08` | `RadioLinkSupervision` | Periodic link-supervision heartbeats are enabled. |
| 4 | `0x10` | `ReceiveCollectiveAlarm` | Module forwards collective alarms received from peers. |
| 5 | `0x20` | `SendCollectiveAlarm` | Module emits collective alarms to peers. |
| 6 | `0x40` | `SuppressAlarms` | Local alarm output is suppressed. |
| 7 | `0x80` | `SuppressWarnings` | Local warning chirps are suppressed. |

### Date Reconstruction {#date-reconstruction}

The payload contains **no absolute timestamps**. All dates are reconstructed at decode time from `productionAge` and the offsets, using the current wall-clock time as anchor:

```
now             = Date.now()
productionDate  = now − productionAge          days
lastAlarm       = now − (productionAge − lastAlarmOffset)     days   (if offset ≠ 0xFFFF)
lastSelftest    = now − (productionAge − lastSelftestOffset)  days   (if offset ≠ 0xFFFF)
```

!!! warning "Clock drift"
    Because the reconstruction anchors on the receiving device's clock, every reconstructed date drifts by exactly one calendar day for each day that passes between two readouts of the same detector. The relative spacing between dates is preserved; the absolute values are only as accurate as the receiver's system time on the day of the readout.

## Test Vectors

A set of hand-crafted WAV files lives under [`tests/`](https://github.com/hmbacher/genius-gateway/tree/main/tests) and exercises every status / fault / warranty flag combination:

| File | Purpose |
|------|---------|
| `sd-complete-healthy.wav` | Reference healthy detector with radio module. |
| `sd-battery-low.wav` | `batteryLowFault` set. |
| `sd-device-fault.wav` | `deviceFault` set. |
| `sd-battery-low-and-device-fault.wav` | Both faults set. |
| `sd-drift-warning.wav` | `driftState = 2`. |
| `sd-drift-defect.wav` | `driftState = 4`. |
| `sd-dirt-forecast-negative.wav` | `dirtForecastNegative` set. |
| `sd-warranty-voided.wav` | Selected warranty flag bits set. |
| `sd-multiple-faults-with-radio.wav` | Multiple SD + RM faults combined. |
| `sd-no-line-id.wav` | `lineId = 0x00000000` (unassigned / no specific line configured). |
| `sd-no-radio.wav` | TC-EC-05 — `radioProductType = 0`, 20-byte payload (no radio module bytes). Parser sets `hasRadio = false`. |

The companion script [`generate-smartsonic-wav.mjs`](https://github.com/hmbacher/genius-gateway/blob/main/tests/generate-smartsonic-wav.mjs) regenerates all fixtures from in-line test specs and can be extended to produce custom payloads. It implements the full forward chain — Hamming(8,4) encode → CRC-16/IBM append → CPFSK modulate at 44.1 kHz, 16-bit mono — and is the authoritative reference for verifying the decoder against new corner cases.
