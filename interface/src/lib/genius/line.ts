/**
 * Alarm-line semantics for FM radio modules.
 *
 * Old-generation modules (FM.Basis / FM.Pro) cannot expose a trustworthy alarm
 * line via the acoustic readout or radio packets, so the line must be entered by
 * hand - mirroring the official Hekatron app (SmartSonicValidationService.isOldFm /
 * isManualLineEntryRequired, AddDeviceManualLineEntryFragment.setHLineEnabled).
 *
 * Line-validity rules come from the FM Basis X / Pro X manual (§"Linienübersicht").
 * Keep this in sync with src/GeniusDevicesService.h (isOldFmModule / line clamps).
 */

import { GeniusRadioModule } from '$lib/types/enums';

/** Old-generation FM modules whose alarm line must be entered manually. */
export function isOldFmModule(model: number | undefined): boolean {
	return model === GeniusRadioModule.FmBasis || model === GeniusRadioModule.FmPro;
}

/** True iff the alarm line for this module must be entered by hand. */
export function isManualLineEntryRequired(model: number | undefined): boolean {
	return isOldFmModule(model);
}

/** True iff the module exposes its alarm line automatically (FM.MCP, FM.Basis X, FM.Pro X). */
export function hasAutomaticLineDetection(model: number | undefined): boolean {
	return (
		model !== undefined &&
		model !== GeniusRadioModule.None &&
		model !== GeniusRadioModule.Unknown &&
		!isOldFmModule(model)
	);
}

/**
 * The "H" major (Sammelalarm / collective-alarm lines) is a Pro-only feature.
 * FM.Basis cannot do Sammelalarm, so H is disabled for it.
 * Replicates setHLineEnabled(FM_BASIS != funkmodul).
 */
export function isHMajorAllowed(model: number | undefined): boolean {
	return model !== GeniusRadioModule.FmBasis;
}

/** Major letters offered by the manual-entry UI. */
export const LINE_MAJORS = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'] as const;

/** Minor digits offered by the manual-entry UI. */
export const LINE_MINORS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9] as const;

/** Valid Sammelalarm minors for the H major (FM.Pro only). */
const H_VALID_MINORS = new Set([0, 1, 2, 4, 5, 7]);

/**
 * Returns the set of valid minor digits for a given major + module, or null if
 * the major itself is invalid for the module. A/B/...G allow 0–9; H allows only
 * the Sammelalarm minors and only on Pro modules.
 */
export function validMinorsFor(major: string, model: number | undefined): number[] | null {
	if (major >= 'A' && major <= 'G') return [...LINE_MINORS];
	if (major === 'H') {
		if (!isHMajorAllowed(model)) return null;
		return [...H_VALID_MINORS];
	}
	return null; // I / J and beyond have no usable lines
}

/**
 * Validates a major+minor line for the given module. Returns an error string
 * (suitable for display) or null when the line is valid.
 */
export function validateLine(
	major: string,
	minor: number,
	model: number | undefined
): string | null {
	if (major === 'H' && !isHMajorAllowed(model)) {
		return 'Sammelalarm (H) lines are only available on FM.Pro modules.';
	}
	const minors = validMinorsFor(major, model);
	if (!minors) {
		return `Line ${major}.${minor} has no function.`;
	}
	if (!minors.includes(minor)) {
		return `Line ${major}.${minor} has no function.`;
	}
	return null;
}

/** Factory-default line for FM.Basis (rotary A.0). */
export const DEFAULT_LINE_MAJOR = 'A';
export const DEFAULT_LINE_MINOR = 0;
