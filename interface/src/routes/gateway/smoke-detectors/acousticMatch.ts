import type { GeniusDevice } from '$lib/types/models';

/**
 * Pure helpers that categorize the outcome of an acoustic readout against the
 * current device list. Kept side-effect free so they can be unit-tested without
 * a Svelte / DOM environment; the calling component is responsible for dispatching
 * the appropriate dialog based on the returned `kind`.
 */

/** Result of matching a newly-detected device against the list (mic button flow). */
export type AcousticMatchResult =
	| { kind: 'all-new' }
	| { kind: 'rwm-match'; rwmIndex: number }
	| { kind: 'fm-match'; fmIndex: number }
	| { kind: 'cross-match'; rwmIndex: number; fmIndex: number };

/**
 * Categorize a newly-detected device against the existing list.
 *
 * - 'all-new'     → neither SN is known. Add as new.
 * - 'rwm-match'   → smoke-detector SN already exists; FM is on the same device
 *                   (or absent). Offer to update the existing one.
 * - 'fm-match'    → smoke-detector SN is new, but the radio module SN is
 *                   already assigned to a different device. Offer to replace.
 * - 'cross-match' → smoke-detector and radio module SNs belong to two
 *                   different existing devices. Offer to delete both and add new.
 */
export function matchAcousticResult(
	devices: GeniusDevice[],
	newDevice: GeniusDevice
): AcousticMatchResult {
	const rwmSN = newDevice.smokeDetector.sn;
	const fmSN = newDevice.radioModule.sn;

	const rwmIndex = devices.findIndex((d) => d.smokeDetector.sn === rwmSN);
	const fmIndex = fmSN > 0 ? devices.findIndex((d) => d.radioModule.sn === fmSN) : -1;

	if (rwmIndex === -1 && fmIndex === -1) return { kind: 'all-new' };
	if (rwmIndex >= 0 && (fmIndex === -1 || fmIndex === rwmIndex))
		return { kind: 'rwm-match', rwmIndex };
	if (rwmIndex === -1 && fmIndex >= 0) return { kind: 'fm-match', fmIndex };
	// rwmIndex >= 0 && fmIndex >= 0 && rwmIndex !== fmIndex
	return { kind: 'cross-match', rwmIndex, fmIndex };
}

/** Result of matching a re-readout against the device the user explicitly targeted. */
export type AcousticUpdateResult =
	| { kind: 'exact-match' }
	| { kind: 'fm-mismatch'; storedFmSN: number }
	| { kind: 'different-device'; rwmIndex: number }
	| { kind: 'unknown' }
	| { kind: 'partial-mismatch' };

/**
 * Categorize a re-readout (refresh button on a specific row) against the target
 * device and the rest of the list.
 *
 * - 'exact-match'      → SNs match the target row exactly. Silent update.
 * - 'fm-mismatch'      → smoke-detector matches the target row, but the FM
 *                        serial number differs from what was stored. Confirm
 *                        before replacing.
 * - 'different-device' → both SNs belong to another existing device. Offer
 *                        to update that one instead.
 * - 'unknown'          → neither SN is in the list. Offer to add as new.
 * - 'partial-mismatch' → partial overlap with other devices but does not
 *                        match the target. Delegate to the "add" flow.
 */
export function matchAcousticUpdate(
	devices: GeniusDevice[],
	newDevice: GeniusDevice,
	targetIndex: number
): AcousticUpdateResult {
	const target = devices[targetIndex];
	const rwmSN = newDevice.smokeDetector.sn;
	const fmSN = newDevice.radioModule.sn;
	const rwmIndex = devices.findIndex((d) => d.smokeDetector.sn === rwmSN);
	const fmIndex = fmSN > 0 ? devices.findIndex((d) => d.radioModule.sn === fmSN) : -1;

	if (rwmIndex === targetIndex && (fmIndex === -1 || fmIndex === targetIndex)) {
		const storedFmSN = target.radioModule.sn;
		if (fmIndex === -1 && fmSN > 0 && storedFmSN > 0 && storedFmSN !== fmSN) {
			return { kind: 'fm-mismatch', storedFmSN };
		}
		return { kind: 'exact-match' };
	}
	if (rwmIndex >= 0 && rwmIndex === fmIndex && rwmIndex !== targetIndex) {
		return { kind: 'different-device', rwmIndex };
	}
	if (rwmIndex === -1 && fmIndex === -1) {
		return { kind: 'unknown' };
	}
	return { kind: 'partial-mismatch' };
}
