import type {
	GeniusDevice,
	GeniusSmokeDetectorInfo,
	GeniusRadioModuleInfo
} from '$lib/types/models';
import { GeniusDeviceRegistration } from '$lib/types/enums';
import {
	STATUS_ALARM,
	STATUS_FAULT_MUTED,
	STATUS_RADIO_MODULE_MASK,
	STATUS_SMOKE_DETECTOR_MASK,
	activeStatusFlags,
	isStatusKnown,
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

// ---------------------------------------------------------------------------
// Readout snapshot - the values reported by the last acoustic readout
// ---------------------------------------------------------------------------

/** Human-readable fault list for the smoke detector portion of a readout. */
export function getSmokeDetectorFaults(sd: GeniusSmokeDetectorInfo): string[] {
	const faults: string[] = [];
	if (sd.batteryLowFault) faults.push('Battery low');
	if (sd.deviceFault) faults.push('Device fault');
	faults.push(...getSmokeDetectorReadoutOnlyFaults(sd));
	return faults;
}

/** Human-readable fault list for the radio module portion of a readout. */
export function getRadioModuleFaults(rm: GeniusRadioModuleInfo): string[] {
	const faults: string[] = [];
	if (rm.radioNetworkFault) faults.push('Radio network fault');
	return faults;
}

/** Readout findings that have no status flag counterpart (drift, dirt, warranty). */
function getSmokeDetectorReadoutOnlyFaults(sd: GeniusSmokeDetectorInfo): string[] {
	const faults: string[] = [];
	const drift = sd.driftState ?? 0;
	if (drift >= 4) faults.push(`Drift defect (state ${drift})`);
	else if (drift >= 2) faults.push(`Drift warning (state ${drift})`);
	if (sd.dirtForecastNegative) faults.push('Dirt forecast negative');
	if ((sd.warrantyFlags ?? 0) > 0) faults.push('Warranty flag(s) set');
	return faults;
}

// ---------------------------------------------------------------------------
// Current status - status flags (received over the air, from the last readout or reset
// manually, whichever is newest) plus the readout-only findings
// ---------------------------------------------------------------------------

/** Status flags reported per subsystem; MUTE is informational and not listed as a fault. */
const NON_FAULT_FLAGS = STATUS_ALARM | STATUS_FAULT_MUTED;

/** True iff anything is known about the smoke detector's current status. */
export function hasSmokeDetectorStatus(d: GeniusDevice): boolean {
	return hasReadout(d) || isStatusKnown(d, STATUS_SMOKE_DETECTOR_MASK);
}

/** True iff anything is known about the radio module's current status. */
export function hasRadioModuleStatus(d: GeniusDevice): boolean {
	return hasReadout(d) || isStatusKnown(d, STATUS_RADIO_MODULE_MASK);
}

/** Current faults of the smoke detector (incl. flags not known yet, which belong to no subsystem). */
export function getCurrentSmokeDetectorFaults(d: GeniusDevice): string[] {
	const flags = activeStatusFlags(d) & ~STATUS_RADIO_MODULE_MASK & ~NON_FAULT_FLAGS;
	const faults = statusFlagLabels(flags);
	if (hasReadout(d)) faults.push(...getSmokeDetectorReadoutOnlyFaults(d.smokeDetector));
	return faults;
}

/** Current faults of the radio module. */
export function getCurrentRadioModuleFaults(d: GeniusDevice): string[] {
	return statusFlagLabels(activeStatusFlags(d) & STATUS_RADIO_MODULE_MASK);
}

/** Combined current fault list across both subsystems. */
export function getDeviceFaults(d: GeniusDevice): string[] {
	return [...getCurrentSmokeDetectorFaults(d), ...getCurrentRadioModuleFaults(d)];
}
