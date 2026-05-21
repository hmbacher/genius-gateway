import type {
	GeniusDevice,
	GeniusSmokeDetectorInfo,
	GeniusRadioModuleInfo
} from '$lib/types/models';
import { GeniusDeviceRegistration } from '$lib/types/enums';

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

/** Human-readable fault list for the smoke detector portion only. */
export function getSmokeDetectorFaults(sd: GeniusSmokeDetectorInfo): string[] {
	const faults: string[] = [];
	if (sd.batteryLowFault) faults.push('Battery low');
	if (sd.deviceFault) faults.push('Device fault');
	const drift = sd.driftState ?? 0;
	if (drift >= 4) faults.push(`Drift defect (state ${drift})`);
	else if (drift >= 2) faults.push(`Drift warning (state ${drift})`);
	if (sd.dirtForecastNegative) faults.push('Dirt forecast negative');
	if ((sd.warrantyFlags ?? 0) > 0) faults.push('Warranty flag(s) set');
	return faults;
}

/** Human-readable fault list for the radio module portion only. */
export function getRadioModuleFaults(rm: GeniusRadioModuleInfo): string[] {
	const faults: string[] = [];
	if (rm.radioNetworkFault) faults.push('Radio network fault');
	return faults;
}

/** Combined fault list across both subsystems. Empty if no readout has been taken yet. */
export function getDeviceFaults(d: GeniusDevice): string[] {
	if (!d.readoutTime) return [];
	return [...getSmokeDetectorFaults(d.smokeDetector), ...getRadioModuleFaults(d.radioModule)];
}
