#!/usr/bin/env node
/**
 * Generates smoke-detector test fixture files in steps of 5 devices.
 * Each device has 10 historical alarms and fully populated readout data.
 *
 * Output: test-fixtures/smoke-detectors-{5,10,15,20,25}.json
 */

import { writeFileSync, mkdirSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __dir = dirname(fileURLToPath(import.meta.url));
const OUT_DIR = join(__dir, '..', 'tests');

// ─── lookup tables ────────────────────────────────────────────────────────────

const LOCATIONS = [
  'Flur EG', 'Wohnzimmer', 'Küche', 'Schlafzimmer 1', 'Schlafzimmer 2',
  'Kinderzimmer 1', 'Kinderzimmer 2', 'Bad EG', 'Bad OG', 'Büro',
  'Keller', 'Dachboden', 'Garage', 'Technikraum', 'Speisekammer',
  'Esszimmer', 'Elternschlafzimmer', 'Gäste WC', 'Treppenhaus EG', 'Treppenhaus OG',
  'Hauswirtschaftsraum', 'Hobbyraum', 'Spielzimmer', 'Ankleide', 'Wintergarten',
  'Flur OG', 'Abstellraum', 'Waschküche', 'Weinkeller', 'Fitnessraum',
  'Bibliothek', 'Musikzimmer', 'Gästeappartement', 'Balkon', 'Terrasse',
  'Carport', 'Schuppen', 'Heizungsraum', 'Serverraum', 'Lagerraum',
  'Flur DG', 'Schlafzimmer 3', 'Schlafzimmer 4', 'Bad DG', 'Dachstudio',
  'Arbeitszimmer', 'Gäste Bad', 'Gäste Schlafzimmer', 'Abstellkammer', 'Vorraum',
];

// Cycle through these every 5 devices
const SD_MODELS       = [3, 3, 2, 3, 1]; // PlusX, PlusX, Plus, PlusX, Hx
const RM_MODELS       = [5, 5, 4, 5, 2]; // ProX, ProX, BasisX, ProX, Pro
const PROD_DATES      = ['2020-03-15','2020-09-22','2021-04-10','2021-11-05','2022-06-18'];
const SELFTEST_DATES  = ['2026-04-02','2026-04-08','2026-04-15','2026-04-22','2026-04-29'];
const SWITCH_MASKS    = [3, 5, 6, 7, 1];
const INTERFERENCES   = [0, 3, 5, 8, 12];
const ALARM_CNT_3M    = [0, 1, 2, 1, 0];

// Line assignment cycles every 4 devices
const LINE_CHARS = ['A', 'B', 'C', 'D'];
const LINE_IDS   = [1, 2, 3, 4];

// 10 historical alarms — same set for every device
const ALARMS = [
  { startTime: '2024-06-10T08:15:00.000Z', endTime: '2024-06-10T08:37:00.000Z', endingReason: 0 },
  { startTime: '2024-07-22T14:30:00.000Z', endTime: '2024-07-22T14:52:00.000Z', endingReason: 1 },
  { startTime: '2024-09-05T11:10:00.000Z', endTime: '2024-09-05T11:28:00.000Z', endingReason: 0 },
  { startTime: '2024-10-18T07:45:00.000Z', endTime: '2024-10-18T08:02:00.000Z', endingReason: 0 },
  { startTime: '2024-11-30T16:20:00.000Z', endTime: '2024-11-30T16:41:00.000Z', endingReason: 1 },
  { startTime: '2025-01-14T09:55:00.000Z', endTime: '2025-01-14T10:15:00.000Z', endingReason: 0 },
  { startTime: '2025-03-02T13:40:00.000Z', endTime: '2025-03-02T14:00:00.000Z', endingReason: 0 },
  { startTime: '2025-05-17T18:25:00.000Z', endTime: '2025-05-17T18:47:00.000Z', endingReason: 1 },
  { startTime: '2025-08-09T10:05:00.000Z', endTime: '2025-08-09T10:22:00.000Z', endingReason: 0 },
  { startTime: '2025-11-24T15:50:00.000Z', endTime: '2025-11-24T16:08:00.000Z', endingReason: 0 },
];

// ─── device builder ───────────────────────────────────────────────────────────

function buildDevice(i) {
  const p = i % 5;
  const q = i % 4;
  const id = 1758215001 + i;

  return {
    id,
    smokeDetector: {
      sn: id,
      model: SD_MODELS[p],
      productionDate: `${PROD_DATES[p]}T00:00:00.000Z`,
      lastSelftest:   `${SELFTEST_DATES[p]}T06:00:00.000Z`,
      lastAlarm:      '2025-11-24T15:50:00.000Z',
      deinstallationCount:    p === 4 ? 1 : 0,
      alarmCountTotal:        10,
      alarmCountLast3Months:  ALARM_CNT_3M[p],
      hoursInStorageMode:     p === 2 ? 24 : 0,
      warrantyFlags:          0,
      batteryLowFault:        false,
      deviceFault:            false,
      driftState:             0,
      dirtForecastNegative:   false,
    },
    radioModule: {
      sn:               2001001 + i,
      model:            RM_MODELS[p],
      lineId:           LINE_IDS[q],
      lineCharacter:    LINE_CHARS[q],
      lineNumber:       LINE_IDS[q],
      radioStateMask:   0,
      radioSwitchMask:  SWITCH_MASKS[p],
      radioInterference: INTERFERENCES[p],
      radioNetworkFault: false,
    },
    location:              LOCATIONS[i],
    registration:          1, // GeniusDeviceRegistration.GeniusPacket
    isAlarming:            false,
    alarms:                ALARMS,
    readoutTime:           '2026-05-25T08:00:00.000Z',
    readoutProtocolVersion: 3,
  };
}

// ─── generate all 25 devices, then slice into 5 files ────────────────────────

mkdirSync(OUT_DIR, { recursive: true });

const MAX_DEVICES = 50;
const allDevices = Array.from({ length: MAX_DEVICES }, (_, i) => buildDevice(i));

for (const count of Array.from({ length: MAX_DEVICES / 5 - 2 }, (_, i) => (i + 3) * 5)) {
  const payload = { version: 1, devices: allDevices.slice(0, count) };
  const path = join(OUT_DIR, `smoke-detectors-${count}.json`);
  writeFileSync(path, JSON.stringify(payload, null, 2));
  console.log(`Written: ${path}  (${count} devices)`);
}
