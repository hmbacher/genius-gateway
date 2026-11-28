/**
 * Genius device status flags - the "ErrorValue" bit field of Hekatron's FM.GW interface.
 *
 * A radio module announces every change of these flags with a status-change packet (36 bytes,
 * message type 0x00): a mask of newly set flags, a mask of newly cleared flags and the serial
 * of the affected smoke detector. Alarm start/stop is the alarm-bit case of that packet.
 *
 * Keep in sync with src/GeniusDeviceStatus.h.
 */

import type { GeniusDevice } from '$lib/types/models';

export const STATUS_ALARM = 0x0001;
export const STATUS_DEVICE_FAULT = 0x0002;
export const STATUS_BATTERY_LOW = 0x0004;
export const STATUS_RADIO_FAULT = 0x0008;
export const STATUS_FAULT_MUTED = 0x0010;
export const STATUS_DISMOUNTED = 0x0020;
/** Gateway-defined, not an on-air bit: the radio module was named as missing by the radio link
 *  supervision of another detector. */
export const STATUS_REPORTED_MISSING = 0x10000;

/** Flags that indicate a problem. MUTE is informational; unknown flags are not counted. */
export const STATUS_FAULT_MASK =
	STATUS_DEVICE_FAULT |
	STATUS_BATTERY_LOW |
	STATUS_RADIO_FAULT |
	STATUS_DISMOUNTED |
	STATUS_REPORTED_MISSING;

/** Flags concerning the smoke detector itself (as opposed to the radio module). */
export const STATUS_SMOKE_DETECTOR_MASK = STATUS_DEVICE_FAULT | STATUS_BATTERY_LOW | STATUS_DISMOUNTED;

/** Flags concerning the radio module. */
export const STATUS_RADIO_MODULE_MASK = STATUS_RADIO_FAULT | STATUS_REPORTED_MISSING;

export type StatusFlagInfo = {
	bit: number;
	/** Name of the flag as a state (e.g. in a packet: "Battery low set / cleared"). */
	label: string;
	/** Whether an active flag is a fault (false: informational). */
	fault: boolean;
};

/** Known status flags, in display order (alarm first, as it is part of every status-change packet). */
export const STATUS_FLAGS: StatusFlagInfo[] = [
	{ bit: STATUS_ALARM, label: 'Alarm', fault: false },
	{ bit: STATUS_BATTERY_LOW, label: 'Battery low', fault: true },
	{ bit: STATUS_DEVICE_FAULT, label: 'Device fault', fault: true },
	{ bit: STATUS_RADIO_FAULT, label: 'Radio link fault', fault: true },
	{ bit: STATUS_DISMOUNTED, label: 'Dismounted', fault: true },
	{ bit: STATUS_REPORTED_MISSING, label: 'Reported missing', fault: true },
	{ bit: STATUS_FAULT_MUTED, label: 'Fault muted', fault: false }
];

/** Label of a single flag bit; unknown bits are shown by value so nothing is swallowed. */
export function statusFlagLabel(bit: number): string {
	return (
		STATUS_FLAGS.find((f) => f.bit === bit)?.label ??
		`Flag 0x${bit.toString(16).toUpperCase().padStart(4, '0')}`
	);
}

/** Split a flag mask into its single bits, lowest first. */
export function statusBits(mask: number): number[] {
	const bits: number[] = [];
	for (let bit = 1; bit <= STATUS_REPORTED_MISSING; bit <<= 1) if (mask & bit) bits.push(bit);
	return bits;
}

/** Labels of all flags in a mask (known flags in display order, unknown ones after). */
export function statusFlagLabels(mask: number): string[] {
	const known = STATUS_FLAGS.filter((f) => mask & f.bit).map((f) => f.label);
	const knownBits = STATUS_FLAGS.reduce((acc, f) => acc | f.bit, 0);
	const unknown = statusBits(mask & ~knownBits).map(statusFlagLabel);
	return [...known, ...unknown];
}

/** Current state of one flag of a device: unknown until the gateway has heard about it. */
export type FlagState = 'unknown' | 'ok' | 'active';

export function flagState(d: GeniusDevice, bit: number): FlagState {
	const state = d.status?.find((s) => s.flag === bit);
	if (!state) return 'unknown';
	return state.active ? 'active' : 'ok';
}

/** Currently active status flags of a device. */
export function activeStatusFlags(d: GeniusDevice): number {
	return (d.status ?? []).reduce((mask, s) => (s.active ? mask | s.flag : mask), 0);
}

/** Status flags of a device whose state is known (active or OK). */
export function knownStatusFlags(d: GeniusDevice): number {
	return (d.status ?? []).reduce((mask, s) => mask | s.flag, 0);
}

/** True iff the state of at least one flag in `mask` is known. */
export function isStatusKnown(d: GeniusDevice, mask: number): boolean {
	return (knownStatusFlags(d) & mask) !== 0;
}

/** Time of the newest state change among the known flags, if any carries a time. */
export function lastStatusChange(d: GeniusDevice): Date | undefined {
	let last: Date | undefined;
	for (const s of d.status ?? []) {
		if (s.since && (!last || s.since > last)) last = s.since;
	}
	return last;
}

/** Kinds of activity log entries; READOUT and STATUS_PACKET are also the source of a flag state.
 *  Keep in sync with genius_device_event_t in src/GeniusDeviceStatus.h. */
export const EVENT_CREATED = 0;
export const EVENT_READOUT = 1;
export const EVENT_STATUS_PACKET = 2;
export const EVENT_STATUS_RESET = 3;
export const EVENT_MODULE_MISSING = 4;
export const EVENT_ALARM = 5;
