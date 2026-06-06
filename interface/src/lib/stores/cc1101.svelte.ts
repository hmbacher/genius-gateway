import type { CC1101RadioState, CC1101RadioMode, CC1101Status } from '$lib/types/models';

export function createCC1101Status() {
	let state: CC1101RadioState = $state('unconfigured');
	let mode: CC1101RadioMode = $state('idle');
	let configured: boolean = $state(false);
	let loaded: boolean = $state(false);

	return {
		get state() {
			return state;
		},
		get mode() {
			return mode;
		},
		get configured() {
			return configured;
		},
		get loaded() {
			return loaded;
		},
		set(status: CC1101Status) {
			state = status.state;
			mode = status.mode ?? 'idle';
			configured = status.configured;
			loaded = true;
		}
	};
}

export const cc1101Status = createCC1101Status();
