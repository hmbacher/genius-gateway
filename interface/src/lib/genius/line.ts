/**
 * Alarm-line semantics for FM radio modules.
 *
 * Old-generation modules (FM.Basis / FM.Pro) give no alarm line in an acoustic
 * readout, so the position of their rotary switches (the line they are set to, e.g. A.1) is
 * entered by hand - mirroring the official Hekatron app (SmartSonicValidationService.isOldFm /
 * isManualLineEntryRequired, AddDeviceManualLineEntryFragment.setHLineEnabled).
 * The 32-bit alarm line ID is a different thing: old modules have one as well, and
 * the gateway takes it over from the radio packets of the module. A module works with
 * another one only if both match - rotary position and alarm line ID.
 *
 * Line-validity rules come from the FM Basis X / Pro X manual (§"Linienübersicht").
 * Keep this in sync with src/GeniusDevicesService.h (isOldFmModule / line clamps).
 */

import { AlarmLineCodeSource, GeniusRadioModule } from '$lib/types/enums';
import type { AlarmLine, GeniusDevice, GeniusRadioModuleInfo } from '$lib/types/models';

/** Old-generation FM modules whose alarm line must be entered manually. */
export function isOldFmModule(model: number | undefined): boolean {
	return model === GeniusRadioModule.FmBasis || model === GeniusRadioModule.FmPro;
}

/** True iff the alarm line for this module must be entered by hand. */
export function isManualLineEntryRequired(model: number | undefined): boolean {
	return isOldFmModule(model);
}

/** True iff an acoustic readout or a range test gives the alarm line of the module (FM.MCP, FM.Basis X, FM.Pro X). */
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

/** Line code in the addressed-line byte of a packet that every radio module accepts. */
export const LINE_CODE_ANY = 0xff;

/**
 * Text of a line code - the rotary position a radio module transmits in every packet:
 * high nibble = letter (0 = A), low nibble = number. `0x03` -> `A.3`, `0xff` -> `Any`.
 */
export function lineCodeText(code: number): string {
	if (code === LINE_CODE_ANY) return 'Any';
	return `${String.fromCharCode(65 + ((code >> 4) & 0x0f))}.${code & 0x0f}`;
}

/** Line code A.0 - the line every FM Basis X is fixed to. */
export const LINE_CODE_A0 = 0x00;

/**
 * True for a line a radio module can be set to: A.0 - G.9 and the collective alarm lines H.0,
 * H.1, H.2, H.4, H.5, H.7. Keep in sync with src/GeniusLineCode.h (genius_line_code_valid).
 */
export function isValidLineCode(code: number): boolean {
	const letter = code >> 4;
	const num = code & 0x0f;
	if (num > 9) return false;
	if (letter <= 6) return true;
	return letter === 7 && H_VALID_MINORS.has(num);
}

/** Line code of a letter ('A' - 'H') and a number, or null if that is no line. */
export function lineCodeOf(letter: string | undefined, num: number | undefined): number | null {
	if (!letter || num === undefined || num === null) return null;
	const l = letter.charCodeAt(0) - 65;
	if (l < 0 || l > 7) return null;
	const code = (l << 4) | num;
	return isValidLineCode(code) ? code : null;
}

/** Every line a radio module can be set to, in order (A.0 ... G.9, H.0 ... H.7). */
export const LINE_SETTING_CODES: number[] = Array.from({ length: 0x80 }, (_, c) => c).filter(
	isValidLineCode
);

/** Radio modules with rotary switches: their line can be set (FM Pro X, old FM Basis / FM Pro). */
export function hasRotarySwitches(model: number | undefined): boolean {
	return (
		model === GeniusRadioModule.FmProX ||
		model === GeniusRadioModule.FmBasis ||
		model === GeniusRadioModule.FmPro
	);
}

/**
 * Line code of a radio module as far as it is known: FM Basis X fixed A.0, otherwise from the
 * readout, the entry by hand or a radio packet. Mirrors moduleLineCode() in GeniusDevicesService.cpp.
 */
export function moduleLineCode(rm: GeniusRadioModuleInfo): number | null {
	if (rm.model === GeniusRadioModule.FmBasisX) return LINE_CODE_A0;
	return lineCodeOf(rm.lineCharacter, rm.lineNumber);
}

/**
 * Line codes the smoke detectors of an alarm line tell, the most frequent first (a tie: the lowest
 * first) - the gateway picks the first one, see lineCodeOfLine() in GeniusDevicesService.cpp.
 */
export function detectorLineCodes(lineId: number, devices: GeniusDevice[]): number[] {
	const counts = new Map<number, number>();
	for (const d of devices) {
		if (!d.radioModule || d.radioModule.sn === 0 || d.radioModule.lineId !== lineId) continue;
		const code = moduleLineCode(d.radioModule);
		if (code !== null) counts.set(code, (counts.get(code) ?? 0) + 1);
	}
	return [...counts.entries()].sort((a, b) => b[1] - a[1] || a[0] - b[0]).map(([code]) => code);
}

export type LineSettingSource = 'manual' | 'packet' | 'detectors' | 'fallback';

/** A smoke detector of an alarm line set to another line than the one the gateway sends for. */
export type MismatchedDetector = { id: number; location: string; code: number };

/** The line setting of an alarm line as the gateway transmits it, with what the page shows. */
export type ResolvedLineSetting = {
	/** Line code the gateway sends line test, fire alarm and commissioning for. */
	code: number;
	source: LineSettingSource;
	/** Line codes of the smoke detectors of the line, the most frequent first. */
	detectorCodes: number[];
	/** A smoke detector of the line has a radio module with rotary switches. */
	rotary: boolean;
	/** Smoke detectors of the line set to another line: they do not react to the gateway's packets. */
	mismatched: MismatchedDetector[];
	/** The line setting matters (rotary switches or not A.0): shown prominently. */
	emphasized: boolean;
	label: string;
	tooltip: string;
};

/**
 * Line setting of an alarm line: set by hand or taken from a radio packet, otherwise (automatic)
 * from its smoke detectors, A.0 when they do not tell it. Mirrors
 * AlarmLinesService::effectiveLineCode().
 */
export function resolveLineSetting(line: AlarmLine, devices: GeniusDevice[]): ResolvedLineSetting {
	const detectorCodes = detectorLineCodes(line.id, devices);
	const onLine = devices.filter((d) => (d.radioModule?.sn ?? 0) > 0 && d.radioModule.lineId === line.id);
	const rotary = onLine.some((d) => hasRotarySwitches(d.radioModule.model));
	const fixed =
		(line.lineCodeSource === AlarmLineCodeSource.Manual ||
			line.lineCodeSource === AlarmLineCodeSource.Packet) &&
		line.lineCode !== undefined &&
		isValidLineCode(line.lineCode);

	let code: number;
	let source: LineSettingSource;
	if (fixed) {
		code = line.lineCode as number;
		source = line.lineCodeSource === AlarmLineCodeSource.Manual ? 'manual' : 'packet';
	} else if (detectorCodes.length > 0) {
		code = detectorCodes[0];
		source = 'detectors';
	} else {
		code = LINE_CODE_A0;
		source = 'fallback';
	}

	const mismatched: MismatchedDetector[] = [];
	for (const d of onLine) {
		const c = moduleLineCode(d.radioModule);
		if (c !== null && c !== code) mismatched.push({ id: d.id, location: d.location, code: c });
	}
	const emphasized = source !== 'fallback' && (rotary || code !== LINE_CODE_A0);

	let tooltip: string;
	if (mismatched.length > 0) {
		const n = mismatched.length;
		const codes = [...new Set(mismatched.map((m) => m.code))].map(lineCodeText).join(', ');
		tooltip =
			n === 1
				? `Differs from the line of 1 smoke detector of this alarm line (${codes}) - it does not react.`
				: `Differs from the line of ${n} smoke detectors of this alarm line (${codes}) - they do not react.`;
	} else if (source === 'manual') {
		tooltip = 'Set by hand.';
	} else if (source === 'packet') {
		tooltip = 'Taken from a radio packet of this line.';
	} else if (source === 'detectors' && !rotary && code === LINE_CODE_A0) {
		tooltip = 'FM Basis X - fixed to A.0.';
	} else if (source === 'detectors') {
		tooltip = 'From the smoke detectors of this line.';
	} else {
		tooltip = 'Not known from the smoke detectors of this line - A.0 is used.';
	}
	return { code, source, detectorCodes, rotary, mismatched, emphasized, label: lineCodeText(code), tooltip };
}

/** CSS classes of the line setting on the Alarm Lines page. */
export function lineSettingClass(ls: ResolvedLineSetting): string {
	if (ls.mismatched.length > 0) return 'badge badge-warning badge-sm font-semibold';
	if (ls.emphasized) return 'badge badge-neutral badge-sm font-semibold';
	return 'text-base-content/40';
}

/**
 * Badge of a smoke detector in the Smoke Detectors column: its line, shown only while some detector of
 * the alarm line is set to another line than the one used - yellow for those, grey for the others.
 */
export function detectorLineBadge(
	ls: ResolvedLineSetting,
	device: GeniusDevice
): { text: string; cls: string } | null {
	if (ls.mismatched.length === 0) return null;
	const c = moduleLineCode(device.radioModule);
	if (c === null) return null;
	return {
		text: lineCodeText(c),
		cls: c === ls.code ? 'badge badge-outline badge-xs opacity-60' : 'badge badge-warning badge-xs font-semibold'
	};
}

function escapeHtml(text: string): string {
	return text.replace(/[&<>"']/g, (ch) => `&#${ch.charCodeAt(0)};`);
}

/**
 * Warning for the confirmation of a line action (HTML): the smoke detectors of the line that are set to
 * another line and will not react. Empty when there are none.
 */
export function lineMismatchWarningHtml(ls: ResolvedLineSetting): string {
	if (ls.mismatched.length === 0) return '';
	const names = ls.mismatched
		.map((m) => `<strong>${escapeHtml(m.location)}</strong> (${lineCodeText(m.code)})`)
		.join(', ');
	const which = ls.mismatched.length === 1 ? 'This smoke detector is' : 'These smoke detectors are';
	const react = ls.mismatched.length === 1 ? 'It will' : 'They will';
	return (
		`<div class="alert alert-warning mt-3 p-2 text-sm block" data-testid="line-mismatch-warning">` +
		`${which} set to another line than ${lineCodeText(ls.code)}, the one the packet is sent for: ${names}. ` +
		`${react} not react to it.</div>`
	);
}

/** Factory-default line for FM.Basis (rotary A.0). */
export const DEFAULT_LINE_MAJOR = 'A';
export const DEFAULT_LINE_MINOR = 0;
