/**
 * REST helpers shared by the smoke-detector pages: the request headers and the calls for the
 * alarm-line set.
 */
import { get } from 'svelte/store';
import { page } from '$app/state';
import { user } from '$lib/stores/user';
import { notifications } from '$lib/components/toasts/notifications';
import type { AlarmLine, AlarmLines } from '$lib/types/models';
import { AlarmLineAcquisition } from '$lib/types/enums';
import { jsonDateReviver } from '$lib/utils/misc';

export function authHeaders(): Record<string, string> {
	return {
		Authorization: page.data.features.security ? 'Bearer ' + get(user).bearer_token : 'Basic',
		'Content-Type': 'application/json'
	};
}

/** Fetch the full alarm-line set (null on failure). */
export async function fetchAlarmLines(): Promise<AlarmLines | null> {
	try {
		const response = await fetch('/rest/alarm-lines', {
			method: 'GET',
			headers: authHeaders()
		});
		return JSON.parse(await response.text(), jsonDateReviver) as AlarmLines;
	} catch (error) {
		console.error('Error fetching alarm lines:', error);
		return null;
	}
}

/** Fetch the IDs of the alarm lines currently configured on the gateway (empty on failure). */
export async function fetchRegisteredLineIds(): Promise<number[]> {
	const alarmLines = await fetchAlarmLines();
	return alarmLines ? alarmLines.lines.map((l) => l.id >>> 0) : [];
}

/** Persist the alarm-line set. Returns true on success. */
export async function postAlarmLines(alarmLines: AlarmLines): Promise<boolean> {
	try {
		const response = await fetch('/rest/alarm-lines', {
			method: 'POST',
			headers: authHeaders(),
			body: JSON.stringify(alarmLines)
		});
		if (response.status === 200) {
			notifications.success('Alarm line added.', 3000);
			return true;
		}
		notifications.error('Failed to add alarm line.', 3000);
		return false;
	} catch (error) {
		console.error('Error:', error);
		notifications.error('Failed to add alarm line.', 3000);
		return false;
	}
}

/** Register an alarm line by ID with a default name (no editor). Refetches first so a concurrent
 *  edit isn't clobbered and an already-present line is treated as success. Used by the device-add
 *  confirm flow; the probe dialog uses openAddAlarmLine (list page) to let the user name the line. */
export async function registerAlarmLine(lineId: number): Promise<boolean> {
	// 0x00000000 = unassigned (ALARMLINES_ID_NONE), 0xFFFFFFFF = broadcast - neither is a user line
	if (!lineId || lineId === 0xffffffff) return false;
	const alarmLines = await fetchAlarmLines();
	if (!alarmLines) {
		notifications.error('Failed to add alarm line.', 3000);
		return false;
	}
	if (alarmLines.lines.some((l) => l.id === lineId)) return true;
	const newLine: AlarmLine = {
		id: lineId,
		name: `Line ${lineId}`,
		created: new Date(),
		acquisition: AlarmLineAcquisition.Acoustic
	};
	return postAlarmLines({ ...alarmLines, lines: [...alarmLines.lines, newLine] });
}
