/**
 * What the gateway heard of the supervised radio modules since it started.
 *
 * FM Pro X modules with radio link supervision send a daily series, so "last seen" tells
 * whether a module is still there. The gateway keeps these values in RAM only: after a
 * restart nothing is known until a module is heard again. Loaded once over REST and kept
 * current by the `device-runtime` WebSocket event.
 */
import type { DeviceRuntimeEvent, GeniusModuleRuntime } from '$lib/types/models';

function createDeviceRuntime() {
	let byId: Record<number, GeniusModuleRuntime> = $state({});
	// Ticks once a minute, so relative times ("3 h ago") stay current without new data
	let now = $state(Date.now());
	if (typeof window !== 'undefined') setInterval(() => (now = Date.now()), 60_000);

	return {
		/** Runtime values by device id; a device without an entry was not heard yet. */
		get byId() {
			return byId;
		},
		/** Current time in ms, updated once a minute - for relative times. */
		get now() {
			return now;
		},
		/** Take over the entries of a REST reply or an event; other devices keep theirs. */
		apply(payload: DeviceRuntimeEvent) {
			for (const entry of payload?.devices ?? []) {
				if (typeof entry.id !== 'number' || !entry.lastSeen) continue;
				byId[entry.id] = {
					id: entry.id,
					lastSeen: new Date(entry.lastSeen),
					rssi: typeof entry.rssi === 'number' ? entry.rssi : undefined,
					rssiTime: entry.rssiTime ? new Date(entry.rssiTime) : undefined
				};
			}
		},
		/** Forget everything, e.g. when the gateway was restarted. */
		clear() {
			byId = {};
		}
	};
}

export const deviceRuntime = createDeviceRuntime();
