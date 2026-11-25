import type { GeniusDevice } from '$lib/types/models';

/** Smoke detectors of one alarm line. The line is a property of the radio module, known from a readout. */
export type LineGroup = {
	/** Alarm line ID (unsigned); 0 = the detectors whose alarm line is not known. */
	key: number;
	name: string;
	devices: GeniusDevice[];
};

/** Alarm line of a device (0 = not known). */
export const lineKeyOf = (device: GeniusDevice): number => (device.radioModule?.lineId ?? 0) >>> 0;

/** Number of different alarm lines the devices are on; devices without a known line do not count. */
export function lineCount(devices: GeniusDevice[]): number {
	return new Set(devices.map(lineKeyOf).filter((key) => key !== 0)).size;
}

/**
 * Group the devices by alarm line. The lines are sorted by name; a line without a known name is
 * called "Line <id>"; the detectors without a known line form the last group. Inside a group the
 * order of `devices` is kept.
 */
export function groupByAlarmLine(
	devices: GeniusDevice[],
	names: Record<number, string>
): LineGroup[] {
	const byKey = new Map<number, GeniusDevice[]>();
	for (const device of devices) {
		const key = lineKeyOf(device);
		const list = byKey.get(key);
		if (list) list.push(device);
		else byKey.set(key, [device]);
	}
	const groups: LineGroup[] = [...byKey.entries()].map(([key, list]) => ({
		key,
		devices: list,
		name: key === 0 ? 'Alarm line unknown' : names[key] || `Line ${key}`
	}));
	groups.sort((a, b) => (a.key === 0 ? 1 : b.key === 0 ? -1 : a.name.localeCompare(b.name)));
	return groups;
}

/**
 * The global device order after the devices of one group were reordered: the group's devices
 * keep their slots in the list, only their order among themselves changes.
 */
export function reorderWithinGroup(all: GeniusDevice[], reordered: GeniusDevice[]): GeniusDevice[] {
	const ids = new Set(reordered.map((d) => d.id));
	const queue = [...reordered];
	return all.map((d) => (ids.has(d.id) ? queue.shift()! : d));
}
