import type {
	GeniusDevice,
	GeniusFlagState,
	GeniusSmokeDetectorInfo,
	GeniusRadioModuleInfo
} from '$lib/types/models';
import { GeniusDeviceRegistration } from '$lib/types/enums';
import { isOldFmModule } from '$lib/genius/line';
import {
	STATUS_BATTERY_LOW,
	STATUS_DEVICE_FAULT,
	STATUS_FAULT_MASK,
	STATUS_FAULT_MUTED,
	STATUS_FLAGS,
	STATUS_RADIO_FAULT,
	STATUS_RADIO_MODULE_MASK,
	STATUS_SMOKE_DETECTOR_MASK,
	activeStatusFlags,
	flagState,
	statusFlagLabel,
	statusFlagLabels
} from '$lib/genius/status';

/** Threshold for treating a readout as stale (~1 year, 365.25 days). */
export const ONE_YEAR_MS = 365.25 * 24 * 60 * 60 * 1000;

/** True iff the device was identified acoustically and has a recorded readout time. */
export function hasReadout(d: GeniusDevice): boolean {
	return d.registration === GeniusDeviceRegistration.Acoustic && !!d.readoutTime;
}

/** True iff the readout is older than ONE_YEAR_MS. False if no readoutTime. */
export function isStaleReadout(d: GeniusDevice): boolean {
	if (!d.readoutTime) return false;
	return Date.now() - d.readoutTime.getTime() > ONE_YEAR_MS;
}

/** True iff the device has a readout that is not stale - the precondition for showing "OK". */
export function hasCurrentReadout(d: GeniusDevice): boolean {
	return hasReadout(d) && !isStaleReadout(d);
}

// ---------------------------------------------------------------------------
// Readout snapshot - the values a readout reports, as it comes from the device.
// Used right after a readout (before the gateway took it over) and by the PDF report.
// ---------------------------------------------------------------------------

/** Human-readable fault list for the smoke detector portion of a readout. */
export function getSmokeDetectorFaults(sd: GeniusSmokeDetectorInfo): string[] {
	const faults: string[] = [];
	if (sd.batteryLowFault) faults.push('Battery low');
	if (sd.deviceFault) faults.push('Device fault');
	faults.push(...getReadoutHints(sd));
	return faults;
}

/** Human-readable fault list for the radio module portion of a readout. */
export function getRadioModuleFaults(rm: GeniusRadioModuleInfo): string[] {
	const faults: string[] = [];
	if (rm.radioNetworkFault) faults.push('Radio network fault');
	return faults;
}

/**
 * Readout findings that are no fault of their own but worth a hint: chamber drift, a negative
 * dirt forecast and warranty flags. They have no counterpart among the status flags.
 */
export function getReadoutHints(sd: GeniusSmokeDetectorInfo): string[] {
	const hints: string[] = [];
	const drift = sd.driftState ?? 0;
	if (drift >= 4) hints.push(`Drift defect (state ${drift})`);
	else if (drift >= 2) hints.push(`Drift warning (state ${drift})`);
	if (sd.dirtForecastNegative) hints.push('Dirt forecast negative');
	if ((sd.warrantyFlags ?? 0) > 0) hints.push('Warranty flag(s) set');
	return hints;
}

// ---------------------------------------------------------------------------
// Current status - one state per status flag, kept by the gateway: fed by status changes
// received over the air and by acoustic readouts, whichever is newest.
// ---------------------------------------------------------------------------

/**
 * Overall condition of a device, in order of priority:
 * - `alarm`   - the smoke detector is alarming
 * - `fault`   - it reports at least one fault (battery, device, radio link, dismounted, missing)
 * - `ok`      - no fault and a current readout confirms battery, detector and radio link -
 *               and none of them was reset to unknown since
 * - `unknown` - no fault reported, but nothing current confirms that all is well
 */
export type DeviceCondition = 'alarm' | 'fault' | 'ok' | 'unknown';

/** The faults a device currently reports, in display order. */
export function activeFaults(d: GeniusDevice): GeniusFlagState[] {
	const states = d.status ?? [];
	return STATUS_FLAGS.filter((f) => f.fault)
		.map((f) => states.find((s) => s.flag === f.bit && s.active))
		.filter((s): s is GeniusFlagState => !!s);
}

/**
 * True iff a current readout still stands for the device: battery, detector and - for radio
 * modules that report it - the radio link are known. A readout sets them all; "Reset Status"
 * makes the reset ones unknown again, and from then on the readout confirms nothing for them.
 */
export function hasConfirmedStatus(d: GeniusDevice): boolean {
	if (!hasCurrentReadout(d)) return false;
	if (flagState(d, STATUS_BATTERY_LOW) === 'unknown') return false;
	if (flagState(d, STATUS_DEVICE_FAULT) === 'unknown') return false;
	// Old FM modules report no radio status in a readout - the radio link is never known there
	return isOldFmModule(d.radioModule.model) || flagState(d, STATUS_RADIO_FAULT) !== 'unknown';
}

export function deviceCondition(d: GeniusDevice): DeviceCondition {
	if (d.isAlarming) return 'alarm';
	if (activeFaults(d).length > 0) return 'fault';
	return hasConfirmedStatus(d) ? 'ok' : 'unknown';
}

/**
 * Information about a device that is no fault: a fault muted on the detector, status flags not
 * known yet, and the readout findings (drift, dirt forecast, warranty).
 */
export function deviceHints(d: GeniusDevice): string[] {
	const hints: string[] = [];
	const active = activeStatusFlags(d);
	if (active & STATUS_FAULT_MUTED) hints.push('Fault muted on detector');
	const knownBits = STATUS_FLAGS.reduce((acc, f) => acc | f.bit, 0);
	for (let bit = 1; bit <= 0x8000; bit <<= 1) {
		if (active & bit & ~knownBits) hints.push(statusFlagLabel(bit));
	}
	if (hasReadout(d)) hints.push(...getReadoutHints(d.smokeDetector));
	return hints;
}

/** Current faults of the smoke detector itself (battery, device fault, dismounted). */
export function getCurrentSmokeDetectorFaults(d: GeniusDevice): string[] {
	return statusFlagLabels(activeStatusFlags(d) & STATUS_SMOKE_DETECTOR_MASK & STATUS_FAULT_MASK);
}

/** Current faults of the radio module (radio link fault, reported missing). */
export function getCurrentRadioModuleFaults(d: GeniusDevice): string[] {
	return statusFlagLabels(activeStatusFlags(d) & STATUS_RADIO_MODULE_MASK & STATUS_FAULT_MASK);
}

/** Combined current fault list across both subsystems. */
export function getDeviceFaults(d: GeniusDevice): string[] {
	return activeFaults(d).map((s) => statusFlagLabel(s.flag));
}

/**
 * True iff the smoke detector's status can be shown: it reports a fault, or a current readout
 * confirms it. Without either nothing is known - no OK is shown.
 */
export function hasSmokeDetectorStatus(d: GeniusDevice): boolean {
	return (
		getCurrentSmokeDetectorFaults(d).length > 0 ||
		(hasCurrentReadout(d) &&
			flagState(d, STATUS_BATTERY_LOW) === 'ok' &&
			flagState(d, STATUS_DEVICE_FAULT) === 'ok')
	);
}

/**
 * True iff the radio module's status can be shown: it reports a fault, or a current readout
 * confirmed the radio link (old FM modules report no radio status in a readout).
 */
export function hasRadioModuleStatus(d: GeniusDevice): boolean {
	return (
		getCurrentRadioModuleFaults(d).length > 0 ||
		(hasCurrentReadout(d) && flagState(d, STATUS_RADIO_FAULT) === 'ok')
	);
}
