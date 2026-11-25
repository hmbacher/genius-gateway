/**
 * Grouping of the smoke detectors by alarm line, on the overview and in the smoke detector list.
 *
 * Without a choice of the user the detectors are grouped only if they are on more than one alarm
 * line. The choice (button in the smoke detector list) is kept in this browser.
 * The names of the lines come from the alarm line settings, which only administrators may read;
 * for other users the groups are titled "Line <id>".
 */
import { get } from 'svelte/store';
import { page } from '$app/state';
import { user } from '$lib/stores/user';
import type { GeniusDevice } from '$lib/types/models';
import { lineCount } from '$lib/genius/lineGroups';

const STORAGE_KEY = 'gg-group-by-line';

function storedChoice(): boolean | null {
	try {
		const saved = localStorage.getItem(STORAGE_KEY);
		return saved === null ? null : saved === '1';
	} catch {
		return null; // no storage: no choice
	}
}

function createLineGrouping() {
	let choice: boolean | null = $state(storedChoice());
	let names: Record<number, string> = $state({});

	return {
		/** Names of the alarm lines by ID; empty until loaded and for users who may not read them. */
		get names() {
			return names;
		},
		/** Whether to group: the user's choice, otherwise only with more than one alarm line. */
		active(devices: GeniusDevice[]): boolean {
			return choice ?? lineCount(devices) > 1;
		},
		/** Switch the grouping on or off; remembered in this browser. */
		set(value: boolean) {
			choice = value;
			try {
				localStorage.setItem(STORAGE_KEY, value ? '1' : '0');
			} catch {
				/* not remembered */
			}
		},
		/** Load the names of the alarm lines (administrators only; a failure leaves them as they are). */
		async loadNames() {
			if (!get(user).admin) return;
			try {
				const response = await fetch('/rest/alarm-lines', {
					method: 'GET',
					headers: {
						Authorization: page.data.features.security ? 'Bearer ' + get(user).bearer_token : 'Basic',
						'Content-Type': 'application/json'
					}
				});
				if (response.status !== 200) return;
				const body = (await response.json()) as { lines?: { id: number; name: string }[] };
				const next: Record<number, string> = {};
				for (const line of body.lines ?? []) next[line.id >>> 0] = line.name;
				names = next;
			} catch (error) {
				console.error('Failed to load alarm line names:', error);
			}
		}
	};
}

export const lineGrouping = createLineGrouping();
