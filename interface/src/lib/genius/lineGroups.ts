import type { GeniusDevice } from '$lib/types/models';

/** Smoke detectors of one alarm line (the line is a property of the radio module). */
export type LineGroup = {
	/** Alarm line ID (unsigned); 0 = the detectors without a line. */
	key: number;
	name: string;
	devices: GeniusDevice[];
};

/** Alarm line ID as 8 hex digits, like the gateway shows it elsewhere. */
export const lineHex = (key: number): string => key.toString(16).toUpperCase().padStart(8, '0');

/** Alarm line of a device (0 = none). */
export const lineKeyOf = (device: GeniusDevice): number => (device.radioModule?.lineId ?? 0) >>> 0;

/**
 * Group the devices by alarm line. Named lines come first, alphabetically; a line without a known name
 * is called "Line <hex id>"; the detectors without a line form the last group. Inside a group the
 * order of `devices` is kept.
 */
export function groupByAlarmLine(
	devices: GeniusDevice[],
	names: Record<number, string>
): LineGroup[] {
	const byKey = new Map<number, GeniusDevice[]>();
	for (const d of devices) {
		const key = lineKeyOf(d);
		const list = byKey.get(key);
		if (list) list.push(d);
		else byKey.set(key, [d]);
	}
	const groups: LineGroup[] = [...byKey.entries()].map(([key, list]) => ({
		key,
		devices: list,
		name: key === 0 ? 'No alarm line' : names[key] || `Line ${lineHex(key)}`
	}));
	groups.sort((a, b) => (a.key === 0 ? 1 : b.key === 0 ? -1 : a.name.localeCompare(b.name)));
	return groups;
}
