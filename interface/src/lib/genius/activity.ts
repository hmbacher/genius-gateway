/**
 * Activity log of a Genius device: what the gateway recorded about it, oldest first - the device
 * was added, acoustic readouts, status changes received over the air, status resets and alarms.
 * The log is kept by the gateway (ring buffer) and is read-only here.
 */

import type { GeniusAlarm, GeniusDevice, GeniusDeviceEvent } from '$lib/types/models';
import { GeniusAlarmEnding, GeniusDeviceRegistration } from '$lib/types/enums';
import { formatDateTimeSeconds } from '$lib/utils/formatDate';
import {
	EVENT_ALARM,
	EVENT_CREATED,
	EVENT_MODULE_MISSING,
	EVENT_READOUT,
	EVENT_STATUS_PACKET,
	EVENT_STATUS_RESET,
	statusFlagLabels
} from './status';

/** The alarms of a device, oldest first - the alarm entries of its activity log. */
export function deviceAlarms(d: GeniusDevice): GeniusAlarm[] {
	return (d.events ?? [])
		.filter((e) => e.type === EVENT_ALARM)
		.map((e) => ({
			startTime: e.time,
			endTime: e.end,
			endingReason: e.endingReason ?? GeniusAlarmEnding.AlarmActive
		}));
}

/**
 * Number of alarms of a device. The gateway counts them separately, as the log only keeps the
 * newest entries.
 */
export function alarmCount(d: GeniusDevice): number {
	return d.alarmCount ?? deviceAlarms(d).length;
}

/** The newest alarm still in the log, if any. */
export function lastAlarm(d: GeniusDevice): GeniusAlarm | undefined {
	const alarms = deviceAlarms(d);
	return alarms.length > 0 ? alarms[alarms.length - 1] : undefined;
}

/** How an alarm ended, as text. */
export function alarmEndingText(reason: number): string {
	switch (reason) {
		case GeniusAlarmEnding.BySmokeDetector:
			return 'no more smoke detected';
		case GeniusAlarmEnding.ByManual:
			return 'stopped by user';
		case GeniusAlarmEnding.ByImport:
			return 'cleared on device config import';
		default:
			return '';
	}
}

function registrationText(registration?: number): string {
	switch (registration) {
		case GeniusDeviceRegistration.GeniusPacket:
			return 'Registered from a received alarm';
		case GeniusDeviceRegistration.Manual:
			return 'Added manually';
		case GeniusDeviceRegistration.Acoustic:
			return 'Added from an acoustic readout';
		default:
			return '';
	}
}

/** Title and detail text of one activity log entry. */
export function describeEvent(e: GeniusDeviceEvent): { title: string; detail: string } {
	switch (e.type) {
		case EVENT_CREATED:
			return { title: 'Smoke detector added', detail: registrationText(e.registration) };
		case EVENT_READOUT: {
			const active = statusFlagLabels(e.set ?? 0);
			return {
				title: 'Acoustic readout',
				detail: active.length > 0 ? `Reported: ${active.join(', ')}` : 'No faults reported'
			};
		}
		case EVENT_STATUS_PACKET: {
			const parts: string[] = [];
			const set = statusFlagLabels(e.set ?? 0);
			const cleared = statusFlagLabels(e.cleared ?? 0);
			if (set.length > 0) parts.push(`Reported: ${set.join(', ')}`);
			if (cleared.length > 0) parts.push(`No longer active: ${cleared.join(', ')}`);
			return { title: 'Status change received', detail: parts.join(' - ') };
		}
		case EVENT_STATUS_RESET:
			return { title: 'Status reset', detail: 'Reported faults discarded' };
		case EVENT_MODULE_MISSING:
			return {
				title: 'Radio module missing',
				detail: `Reports radio module ${e.cause ?? '?'} as missing`
			};
		case EVENT_ALARM: {
			const reason = e.endingReason ?? GeniusAlarmEnding.AlarmActive;
			if (reason === GeniusAlarmEnding.AlarmActive) return { title: 'Alarm', detail: 'Still active' };
			const ending = alarmEndingText(reason);
			const end = e.end ? `Ended ${formatDateTimeSeconds(e.end)}` : 'Ended';
			return { title: 'Alarm', detail: ending ? `${end} - ${ending}` : end };
		}
		default:
			return { title: `Entry of type ${e.type}`, detail: '' };
	}
}
