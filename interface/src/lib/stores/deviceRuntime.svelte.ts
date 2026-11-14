import type { GeniusDeviceRuntime, GeniusLinkFault } from '$lib/types/models';
import { socket } from '$lib/stores/socket';

/**
 * RAM-only runtime state of the devices (last seen, live RSSI, radio link fault), mirrored from the
 * gateway. Nothing here is persisted; after a gateway reboot the values start empty and fill up again
 * as frames arrive.
 */

type Raw = Record<string, unknown>;

const toDate = (v: unknown): Date | undefined => {
	if (typeof v !== 'string') return undefined;
	const d = new Date(v);
	return isNaN(d.getTime()) ? undefined : d;
};

/** Convert the JSON of the gateway (ISO strings) into a typed runtime object. */
export function toRuntime(raw: Raw): GeniusDeviceRuntime {
	const out: GeniusDeviceRuntime = { id: raw.id as number };
	if (raw.lastSeen) out.lastSeen = toDate(raw.lastSeen);
	if (typeof raw.lastSeenAgeS === 'number') {
		out.lastSeenAgeS = raw.lastSeenAgeS;
		// The gateway has no wall clock yet (no NTP): derive the time from the age, good enough for "x min ago"
		if (!out.lastSeen) out.lastSeen = new Date(Date.now() - raw.lastSeenAgeS * 1000);
	}
	if (typeof raw.rssi === 'number') {
		out.rssi = raw.rssi;
		out.rssiSource = raw.rssiSource === 'probe' ? 'probe' : 'live';
		if (raw.rssiTime) out.rssiTime = toDate(raw.rssiTime);
	}
	const f = raw.fault as Raw | undefined;
	if (f && typeof f === 'object') {
		const fault: GeniusLinkFault = {
			state: f.state as GeniusLinkFault['state'],
			acknowledged: !!f.acknowledged
		};
		if (f.startUnknown) fault.startUnknown = true;
		for (const k of ['since', 'activeSince', 'acknowledgedAt', 'lastReceived'] as const) {
			if (f[k]) fault[k] = toDate(f[k]);
		}
		if (typeof f.expiresInS === 'number') fault.expiresInS = f.expiresInS;
		if (typeof f.causeSn === 'number') fault.causeSn = f.causeSn;
		if (typeof f.causeDeviceId === 'number') fault.causeDeviceId = f.causeDeviceId;
		if (typeof f.causeLocation === 'string') fault.causeLocation = f.causeLocation;
		out.fault = fault;
	}
	return out;
}

export function createDeviceRuntime() {
	let byId: Record<number, GeniusDeviceRuntime> = $state({});
	/** Ticks so relative times ("3 min ago") stay current; started by the page via `startClock()`. */
	let now = $state(Date.now());

	return {
		get byId() {
			return byId;
		},
		get now() {
			return now;
		},
		/** Replace everything (answer of the REST endpoint). */
		replaceAll(list: Raw[]) {
			const next: Record<number, GeniusDeviceRuntime> = {};
			for (const raw of list) next[raw.id as number] = toRuntime(raw);
			byId = next;
		},
		/** Merge the changed devices of a `device-runtime` event; an entry without data clears the device. */
		update(list: Raw[]) {
			for (const raw of list) byId[raw.id as number] = toRuntime(raw);
		},
		/** Start the 30 s clock tick, returns the stop function. */
		startClock() {
			const t = setInterval(() => (now = Date.now()), 30000);
			return () => clearInterval(t);
		},
		/** Fetch the current runtime state of all devices (`GET /rest/gateway-devices/runtime`). */
		async load(headers: Record<string, string>) {
			try {
				const response = await fetch('/rest/gateway-devices/runtime', { method: 'GET', headers });
				if (response.status !== 200) return;
				const body = (await response.json()) as { devices?: Raw[] };
				this.replaceAll(body.devices ?? []);
			} catch (error) {
				console.error('Failed to load device runtime state:', error);
			}
		},
		/**
		 * Load the state and keep it current: follows the `device-runtime` WebSocket event and ticks the
		 * clock for the relative times. Returns the function that stops all of it (use as `$effect` result).
		 */
		start(headers: Record<string, string>): () => void {
			void this.load(headers);
			const onRuntime = (evt?: { devices?: Raw[] }) => this.update(evt?.devices ?? []);
			socket.on<{ devices?: Raw[] }>('device-runtime', onRuntime);
			const stopClock = this.startClock();
			return () => {
				socket.off('device-runtime', onRuntime);
				stopClock();
			};
		}
	};
}

export const deviceRuntime = createDeviceRuntime();
