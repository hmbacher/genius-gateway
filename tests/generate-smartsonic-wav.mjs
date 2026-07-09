/**
 * SmartSonic test WAV generator
 * Reverse-engineered from tuner-pipeline.ts - produces audio the acoustic decoder accepts.
 *
 * Signal chain (encoder is the inverse of the decoder):
 *   payload → Hamming(8,4) encode → bit stream → BIT_PATTERN FSK symbols
 *   → prepend SYNC1 → upsample ×8 → CPFSK audio at 44100 Hz → WAV
 *
 * Run:  node tests/generate-smartsonic-wav.mjs
 */

import { writeFileSync, mkdirSync } from 'fs';

// ─── Constants (must match tuner-pipeline.ts exactly) ─────────────────────────

const SAMPLE_RATE   = 44100;
const NCO_FREQ      = 4449.1;
const DECIMATION    = 8;
const SYMBOL_LEN    = 65.708;   // PATTERN_SET_1 fractional symbol length (decimated samples)
const SILENCE_DEC   = 2200;     // ~0.4 s of silence at decimated rate for filter warm-up
const AMPLITUDE     = 0.70;     // output amplitude (0–1)
const DELTA_F       = 250;      // FSK frequency deviation in Hz
                                // phaseErr ≈ 2000·sin(2π·250·8/44100) ≈ 563 → clamps to 400 ✓

// Hamming(8,4) codebook - CODEBOOK[nibble] = 8-bit codeword
const CODEBOOK = [0, 135, 153, 30, 170, 45, 51, 180, 75, 204, 210, 85, 225, 102, 120, 255];

// BIT_PATTERN: 64 decimated samples  [0×3, −1×26, 0×6, +1×26, 0×3]
// A bit=1 is encoded as BIT_PATTERN as-is  (positive correlation with template)
// A bit=0 is encoded as −BIT_PATTERN       (negative correlation)
const BIT_PATTERN = new Array(64).fill(0);
for (let i = 3;  i < 29; i++) BIT_PATTERN[i] = -1;
for (let i = 35; i < 61; i++) BIT_PATTERN[i] =  1;

// SYNC1 (1576 decimated samples) - segment list [length, value]
const SYNC1_SEGS = [
    [65,1],[132,-1],[33,1],[33,-1],[163,1],[132,-1],[98,1],[66,-1],
    [33,1],[33,-1],[32,1],[33,-1],[66,1],[99,-1],[130,1],[166,-1],
    [32,1],[33,-1],[131,1],[66,-1]
];
const SYNC1 = [];
for (const [len, val] of SYNC1_SEGS) for (let i = 0; i < len; i++) SYNC1.push(val);

// ─── DSP helpers ──────────────────────────────────────────────────────────────

function hammingEncode(nibble) {
    return CODEBOOK[nibble & 0x0f];
}

// CRC-16/IBM (poly 0x8005, init 0xFFFF) - over payload bytes only, matching the decoder:
//   const computed = crc16(this.frameData, this.frameLen);
function crc16(bytes) {
    const POLY = 0x8005;
    let crc = 0xffff;
    for (const b0 of bytes) {
        let b = b0 & 0xff;
        for (let j = 0; j < 8; j++) {
            const xor = (((crc & 0x8000) >> 8) ^ (b & 0x80)) !== 0;
            crc = xor ? ((crc << 1) ^ POLY) & 0xffff : (crc << 1) & 0xffff;
            b = (b << 1) & 0xff;
        }
    }
    return crc;
}

// Build framing: [length, ...payload, crc_lo, crc_hi]
function buildFrame(payload) {
    const crc = crc16(payload);
    return [payload.length, ...payload, crc & 0xff, (crc >> 8) & 0xff];
}

// ─── Signal builder ───────────────────────────────────────────────────────────

// Encode one frame byte as 16 BIT_PATTERN polarities (±1).
// Byte b → low nibble Hamming → bits 0–7, high nibble Hamming → bits 8–15.
// The bit at position N determines the polarity of the N-th BIT_PATTERN in the symbol.
function byteToPatternPolarities(b) {
    const lowCode  = hammingEncode(b & 0x0f);
    const highCode = hammingEncode((b >>> 4) & 0x0f);
    const sym16    = lowCode | (highCode << 8);
    const pol = [];
    for (let bit = 0; bit < 16; bit++) pol.push((sym16 >>> bit) & 1 ? 1 : -1);
    return pol;
}

// Build the full decimated FSK signal (values ±1 or 0 at SAMPLE_RATE/DECIMATION).
// Layout: [silence] [SYNC1] [BIT_PATTERNs at fractional symbol spacing] [silence]
function buildDecimatedSignal(frame) {
    const polarities = frame.flatMap(byteToPatternPolarities);

    const sig = [
        ...new Array(SILENCE_DEC).fill(0),
        ...SYNC1
    ];

    // Place BIT_PATTERNs at positions round(N × SYMBOL_LEN) from SYNC1 end.
    // Fill with neutral (0) between patterns to maintain exact fractional timing.
    let targetPos = sig.length; // absolute position of the start of BIT_PATTERN N=0
    for (const polarity of polarities) {
        // Advance to the rounded target position with neutral samples
        while (sig.length < Math.round(targetPos)) sig.push(0);
        // Emit one BIT_PATTERN with the given polarity
        for (const v of BIT_PATTERN) sig.push(v * polarity);
        targetPos += SYMBOL_LEN;
    }

    // Trailing silence
    for (let i = 0; i < SILENCE_DEC; i++) sig.push(0);

    return sig;
}

// Convert decimated signal to CPFSK audio at SAMPLE_RATE.
// target = +1 → freq = NCO_FREQ + DELTA_F (positive FM discriminator output after pipeline)
// target = −1 → freq = NCO_FREQ − DELTA_F (negative)
// target =  0 → freq = NCO_FREQ           (neutral, no frequency deviation)
function generateAudio(decimatedSig) {
    const audio = new Int16Array(decimatedSig.length * DECIMATION);
    let phase = 0;
    for (let i = 0; i < decimatedSig.length; i++) {
        const freq      = NCO_FREQ + decimatedSig[i] * DELTA_F;
        const phaseStep = (2 * Math.PI * freq) / SAMPLE_RATE;
        for (let k = 0; k < DECIMATION; k++) {
            phase += phaseStep;
            audio[i * DECIMATION + k] = Math.round(Math.sin(phase) * AMPLITUDE * 32767);
        }
    }
    return audio;
}

// ─── WAV writer ───────────────────────────────────────────────────────────────

function writeWav(path, samples) {
    const dataBytes = samples.length * 2;
    const buf       = Buffer.alloc(44 + dataBytes);
    let o = 0;

    buf.write('RIFF',  o); o += 4;
    buf.writeUInt32LE(36 + dataBytes, o); o += 4;
    buf.write('WAVE',  o); o += 4;
    buf.write('fmt ',  o); o += 4;
    buf.writeUInt32LE(16,           o); o += 4;  // chunk size
    buf.writeUInt16LE(1,            o); o += 2;  // PCM
    buf.writeUInt16LE(1,            o); o += 2;  // mono
    buf.writeUInt32LE(SAMPLE_RATE,  o); o += 4;
    buf.writeUInt32LE(SAMPLE_RATE * 2, o); o += 4; // byte rate
    buf.writeUInt16LE(2,            o); o += 2;  // block align
    buf.writeUInt16LE(16,           o); o += 2;  // bits per sample
    buf.write('data',  o); o += 4;
    buf.writeUInt32LE(dataBytes,    o); o += 4;

    for (const s of samples) { buf.writeInt16LE(s, o); o += 2; }
    writeFileSync(path, buf);
}

// ─── Payload builder ──────────────────────────────────────────────────────────
// Byte layout matches parseTunerData() in tuner-pipeline.ts exactly.
// 16-bit fields are little-endian (low byte first).
// 32-bit serial numbers are big-endian (MSB first).

function u32be(n) { // big-endian 32-bit as 4 bytes
    const u = n >>> 0;
    return [(u >>> 24) & 0xff, (u >>> 16) & 0xff, (u >>> 8) & 0xff, u & 0xff];
}
function u16le(n) { return [n & 0xff, (n >>> 8) & 0xff]; } // little-endian 16-bit

function buildPayload(o) {
    const p = [];

    // Byte 0: protocol version
    p.push(o.protocolVersion ?? 5);

    // Bytes 1–4: smoke detector serial number (big-endian)
    p.push(...u32be(o.serialNumber ?? 0x12345678));

    // Byte 5: product type [3:0] | radio product type [7:4]
    p.push(((o.radioProductType ?? 0) << 4) | (o.productType ?? 3));

    // Byte 6: deinstallation count
    p.push(o.deinstallationCount ?? 0);

    // Byte 7: total alarm count
    p.push(o.alarmCountTotal ?? 0);

    // Byte 8: alarm count last 3 months
    p.push(o.alarmCountLast3Months ?? 0);

    // Bytes 9–10: last alarm offset in days from production date (LE); 0xFFFF = never
    p.push(...u16le(o.lastAlarmOffsetDays ?? 0xffff));

    // Bytes 11–12: production age in days (LE)
    p.push(...u16le(o.productionAgeDays ?? 1095));

    // Bytes 13–14: hours in storage mode (LE)
    p.push(...u16le(o.hoursInStorage ?? 0));

    // Bytes 15–16: last self-test offset in days from production (LE); 0xFFFF = never
    p.push(...u16le(o.lastSelftestOffsetDays ?? 0xffff));

    // Bytes 17–18: warranty flags bitmask (LE)
    // bit 0=MaxDirty, 1=OutOfTemp, 2=DetectorTooOld, 3=StorageTimeExceeded,
    // 4=ActivationTimeExceeded, 5=TooManyEvents, 6=TooManyAlarms, 7=TooManyFaults,
    // 8=TooManySelfTests, 9=TooManyRadioFaults, 10=TooManyRadioOutOfOrderEvents,
    // 11=RadioInstallationTooOld, 12=TooMuchRadioActivity, 13=TooMuchRadioInterference,
    // 14=TooManyRadioTxEvents, 15=TooManyRadioRxEvents
    p.push(...u16le(o.warrantyFlags ?? 0));

    // Byte 19: device status
    // bit 0=batteryLowFault, 1=deviceFault, 2=radioNetworkFault,
    // bits 3–6=driftState (0–7), bit 7=dirtForecastNegative
    p.push(o.deviceStatusByte ?? 0);

    // Bytes 20+ only present when radioProductType > 0
    if ((o.radioProductType ?? 0) > 0) {
        // Byte 20: radio state mask
        // bit 0=FmFault, 1=TransmissionRangeTest, 2=Selftest, 3=FmBatteryLowFault,
        // 4=RemoteBattLow, 5=RemoteError, 6=RadioLinkError, 7=RemoteAlarm
        p.push(o.radioStateMask ?? 0);

        // Bytes 21–24: radio module serial number (big-endian)
        p.push(...u32be(o.radioSerialNumber ?? 0));

        // Bytes 25–28: alarm line ID (big-endian); 0=unassigned, 0xFFFFFFFF=broadcast
        p.push(...u32be(o.lineId ?? 0));

        // Byte 29: line character index [7:4] | line number [3:0]
        // lineCharIdx: 0='A', 1='B', ..., 9='J'
        p.push(((o.lineCharIdx ?? 0) << 4) | (o.lineNumber ?? 0));

        // Byte 30: radio switch flags
        // bit 2=ReducedTransmittingPower, 3=RadioLinkSupervision,
        // 4=ReceiveCollectiveAlarm, 5=SendCollectiveAlarm,
        // 6=SuppressAlarms, 7=SuppressWarnings
        p.push(o.radioSwitchFlags ?? 0);

        // Byte 31: radio interference in 0.1 dB units (0–25.5 dB)
        p.push(Math.round((o.radioInterference ?? 0) * 10) & 0xff);
    }

    return p;
}

// ─── Generate one WAV file ────────────────────────────────────────────────────

function generate(scenario, outDir) {
    const payload   = buildPayload(scenario.opts);
    const frame     = buildFrame(payload);
    const decimated = buildDecimatedSignal(frame);
    const audio     = generateAudio(decimated);
    const path      = `${outDir}/${scenario.filename}`;
    writeWav(path, audio);
    const dur  = (audio.length / SAMPLE_RATE).toFixed(2);
    const size = ((44 + audio.length * 2) / 1024).toFixed(0);
    const hex  = payload.map(b => b.toString(16).padStart(2,'0')).join(' ');
    const crc  = crc16(payload);
    console.log(`  ${scenario.filename}  (${dur}s, ${size} KB)  - ${scenario.name}`);
    console.log(`    payload (${payload.length}B): ${hex}`);
    console.log(`    CRC-16: 0x${crc.toString(16).padStart(4,'0').toUpperCase()}  frame bytes: ${frame.length}`);
}

// ─── Test scenarios ───────────────────────────────────────────────────────────
// deviceStatusByte bit map:
//   0x01 = batteryLowFault
//   0x02 = deviceFault
//   0x04 = radioNetworkFault
//   driftState N → (N << 3)  e.g. state 2 = 0x10, state 4 = 0x20
//   0x80 = dirtForecastNegative

const SCENARIOS = [
    {
        filename: 'sd-battery-low.wav',
        name: 'Battery Low',
        opts: {
            serialNumber:           0x12A10001,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      1460,       // ~4 years
            lastAlarmOffsetDays:    0xffff,     // no alarm history
            lastSelftestOffsetDays: 1400,
            alarmCountTotal:        0,
            warrantyFlags:          0,
            deviceStatusByte:       0x01,       // batteryLowFault
            radioStateMask:         0x00,
            radioSerialNumber:      0xAB001001,
            lineId:                 0x00000001,
            lineCharIdx:            0,          // 'A'
            lineNumber:             1,
            radioSwitchFlags:       0x18,       // RadioLinkSupervision | ReceiveCollectiveAlarm
            radioInterference:      1.2
        }
    },
    {
        filename: 'sd-device-fault.wav',
        name: 'Device Fault',
        opts: {
            serialNumber:           0x12A10002,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      730,        // ~2 years
            lastAlarmOffsetDays:    0xffff,
            lastSelftestOffsetDays: 700,
            alarmCountTotal:        0,
            warrantyFlags:          0,
            deviceStatusByte:       0x02,       // deviceFault
            radioStateMask:         0x00,
            radioSerialNumber:      0xAB001002,
            lineId:                 0x00000001,
            lineCharIdx:            0,          // 'A'
            lineNumber:             2,
            radioSwitchFlags:       0x18,
            radioInterference:      0.9
        }
    },
    {
        filename: 'sd-battery-low-and-device-fault.wav',
        name: 'Battery Low + Device Fault',
        opts: {
            serialNumber:           0x12A10003,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      1825,       // ~5 years
            lastAlarmOffsetDays:    0xffff,
            lastSelftestOffsetDays: 1800,
            alarmCountTotal:        1,
            alarmCountLast3Months:  0,
            warrantyFlags:          0,
            deviceStatusByte:       0x03,       // batteryLowFault + deviceFault
            radioStateMask:         0x00,
            radioSerialNumber:      0xAB001003,
            lineId:                 0x00000002,
            lineCharIdx:            1,          // 'B'
            lineNumber:             1,
            radioSwitchFlags:       0x18,
            radioInterference:      1.5
        }
    },
    {
        filename: 'sd-drift-warning.wav',
        name: 'Drift Warning (state 2)',
        opts: {
            serialNumber:           0x12A10004,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      900,        // ~2.5 years
            lastAlarmOffsetDays:    0xffff,
            lastSelftestOffsetDays: 870,
            alarmCountTotal:        0,
            warrantyFlags:          0,
            deviceStatusByte:       0x10,       // driftState = 2 → (2 << 3)
            radioStateMask:         0x00,
            radioSerialNumber:      0xAB001004,
            lineId:                 0x00000002,
            lineCharIdx:            1,          // 'B'
            lineNumber:             3,
            radioSwitchFlags:       0x18,
            radioInterference:      0.6
        }
    },
    {
        filename: 'sd-drift-defect.wav',
        name: 'Drift Defect (state 4)',
        opts: {
            serialNumber:           0x12A10005,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      1280,       // ~3.5 years
            lastAlarmOffsetDays:    0xffff,
            lastSelftestOffsetDays: 1250,
            alarmCountTotal:        0,
            warrantyFlags:          0,
            deviceStatusByte:       0x20,       // driftState = 4 → (4 << 3)
            radioStateMask:         0x00,
            radioSerialNumber:      0xAB001005,
            lineId:                 0x00000003,
            lineCharIdx:            2,          // 'C'
            lineNumber:             1,
            radioSwitchFlags:       0x18,
            radioInterference:      1.1
        }
    },
    {
        filename: 'sd-dirt-forecast-negative.wav',
        name: 'Dirt Forecast Negative',
        opts: {
            serialNumber:           0x12A10006,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      1095,       // ~3 years
            lastAlarmOffsetDays:    0xffff,
            lastSelftestOffsetDays: 1060,
            alarmCountTotal:        0,
            warrantyFlags:          0,
            deviceStatusByte:       0x80,       // dirtForecastNegative
            radioStateMask:         0x00,
            radioSerialNumber:      0xAB001006,
            lineId:                 0x00000003,
            lineCharIdx:            2,          // 'C'
            lineNumber:             2,
            radioSwitchFlags:       0x18,
            radioInterference:      0.7
        }
    },
    {
        filename: 'sd-warranty-voided.wav',
        name: 'Warranty Voided (DetectorTooOld + TooManyAlarms)',
        opts: {
            serialNumber:           0x12A10007,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      2920,       // ~8 years
            alarmCountTotal:        18,
            alarmCountLast3Months:  0,
            lastAlarmOffsetDays:    2555,       // last alarm ~7 years after production
            lastSelftestOffsetDays: 2880,
            warrantyFlags:          0x0044,     // bit 2 (DetectorTooOld) | bit 6 (TooManyAlarms)
            deviceStatusByte:       0x00,       // no immediate fault, but warranty is void
            radioStateMask:         0x00,
            radioSerialNumber:      0xAB001007,
            lineId:                 0x00000004,
            lineCharIdx:            3,          // 'D'
            lineNumber:             1,
            radioSwitchFlags:       0x18,
            radioInterference:      2.0
        }
    },
    {
        filename: 'sd-complete-healthy.wav',
        name: 'Complete Healthy Device (Genius Plus X + FM.Basis X, all clear)',
        opts: {
            serialNumber:           0x12A10000,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      730,        // ~2 years
            lastAlarmOffsetDays:    0xffff,     // no alarm history
            lastSelftestOffsetDays: 700,
            alarmCountTotal:        0,
            alarmCountLast3Months:  0,
            hoursInStorage:         0,
            warrantyFlags:          0x0000,     // warranty intact
            deviceStatusByte:       0x00,       // all clear
            radioStateMask:         0x00,       // all clear
            radioSerialNumber:      0xAB009900,
            lineId:                 0x00000002,
            lineCharIdx:            1,          // 'B'
            lineNumber:             2,
            radioSwitchFlags:       0x18,       // RadioLinkSupervision | ReceiveCollectiveAlarm
            radioInterference:      0.8
        }
    },
    {
        filename: 'sd-multiple-faults-with-radio.wav',
        name: 'Battery Low + Device Fault + Radio Fault (FM.Basis X)',
        opts: {
            serialNumber:           0x12A10008,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      1460,       // ~4 years
            alarmCountTotal:        3,
            alarmCountLast3Months:  1,
            lastAlarmOffsetDays:    1400,
            lastSelftestOffsetDays: 1450,
            warrantyFlags:          0,
            deviceStatusByte:       0x07,       // batteryLowFault + deviceFault + radioNetworkFault
            radioStateMask:         0x01,       // FmFault (bit 0)
            radioSerialNumber:      0xAB001234,
            lineId:                 0x00000001,
            lineCharIdx:            0,          // 'A'
            lineNumber:             1,
            radioSwitchFlags:       0x18,       // RadioLinkSupervision | ReceiveCollectiveAlarm
            radioInterference:      3.2
        }
    },
    {
        // TC-EC: lineId = 0x00000000 (no specific line configured / unassigned).
        // The parser stores this value verbatim; the frontend must not offer to
        // add an alarm line when lineId is 0 (ALARMLINES_ID_NONE).
        filename: 'sd-no-line-id.wav',
        name: 'No Line ID (lineId = 0x00000000, unassigned)',
        opts: {
            serialNumber:           0x12A1000A,
            productType:            3,          // Genius Plus X
            radioProductType:       4,          // FM.Basis X
            productionAgeDays:      730,        // ~2 years
            lastAlarmOffsetDays:    0xffff,
            lastSelftestOffsetDays: 700,
            alarmCountTotal:        0,
            warrantyFlags:          0,
            deviceStatusByte:       0x00,
            radioStateMask:         0x00,
            radioSerialNumber:      0xAB00100A,
            lineId:                 0x00000000, // unassigned / no line
            lineCharIdx:            0,
            lineNumber:             0,
            radioSwitchFlags:       0x18,
            radioInterference:      0.5
        }
    },
    {
        // TC-EC-05: No radio module fitted.
        // radioProductType = 0 → buildPayload() emits only the 20 SD bytes.
        // Parser sets hasRadio = false; the UI should show "No radio module installed."
        filename: 'sd-no-radio.wav',
        name: 'No Radio Module (20-byte payload, TC-EC-05)',
        opts: {
            serialNumber:           0x12A1000B,
            productType:            3,          // Genius Plus X
            radioProductType:       0,          // no FM module → 20-byte payload
            productionAgeDays:      730,        // ~2 years
            lastAlarmOffsetDays:    0xffff,
            lastSelftestOffsetDays: 700,
            alarmCountTotal:        0,
            warrantyFlags:          0,
            deviceStatusByte:       0x00
        }
    },
    {
        // Manual line-entry test: a current-generation SD (Genius Plus X) paired with
        // an OLD radio module (FM.Basis). Old modules always transmit lineId=0, and the
        // line byte (29) is unreliable - deliberately set to non-zero "garbage" (F.7)
        // here to verify the frontend ignores it and requires manual line entry
        // regardless of what's on the wire (see old-fm-manual-line-entry.md).
        filename: 'sd-genius-plus-x-fm-basis.wav',
        name: 'Genius Plus X + FM.Basis (old module - line must be entered manually)',
        opts: {
            serialNumber:           0x12A1000C,
            productType:            3,          // Genius Plus X
            radioProductType:       1,          // FM.Basis (old)
            productionAgeDays:      365,        // ~1 year
            lastAlarmOffsetDays:    0xffff,
            lastSelftestOffsetDays: 330,
            alarmCountTotal:        0,
            warrantyFlags:          0,
            deviceStatusByte:       0x00,
            radioStateMask:         0x00,
            radioSerialNumber:      0xAB00100C,
            lineId:                 0x00000000, // old modules always transmit 0
            lineCharIdx:            5,          // 'F' - deliberately unreliable/garbage
            lineNumber:             7,          //       to verify it's ignored, not trusted
            radioSwitchFlags:       0x18,
            radioInterference:      0.4
        }
    }
];

// ─── Main ─────────────────────────────────────────────────────────────────────

const OUT_DIR = 'tests/wav';
mkdirSync(OUT_DIR, { recursive: true });

console.log(`Generating ${SCENARIOS.length} SmartSonic test WAV files → ${OUT_DIR}/\n`);
for (const s of SCENARIOS) generate(s, OUT_DIR);
console.log(`\nDone. Play each file through a speaker near a browser running the Genius Gateway`);
console.log(`acoustic detection page (HTTPS required) to verify the decoder accepts it.`);
