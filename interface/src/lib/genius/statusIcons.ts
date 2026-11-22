/**
 * One icon per status flag - the same in the overview, the device list, the details dialog and
 * the activity log. Faults use a struck-through ("off") icon.
 */

import type { Component } from 'svelte';
import IconBatteryLow from '~icons/tabler/battery-off';
import IconDeviceFault from '~icons/custom-icons/smoke-detector-off';
import IconRadioFault from '~icons/tabler/access-point-off';
import IconDismounted from '~icons/tabler/pinned-off';
import IconReportedMissing from '~icons/tabler/antenna-off';
import IconMuted from '~icons/tabler/volume-off';
import IconUnknownFlag from '~icons/tabler/help-hexagon';
import {
	STATUS_BATTERY_LOW,
	STATUS_DEVICE_FAULT,
	STATUS_DISMOUNTED,
	STATUS_FAULT_MUTED,
	STATUS_RADIO_FAULT,
	STATUS_REPORTED_MISSING
} from './status';

export type StatusIcon = Component<{ class?: string }>;

const ICONS: Record<number, StatusIcon> = {
	[STATUS_BATTERY_LOW]: IconBatteryLow,
	[STATUS_DEVICE_FAULT]: IconDeviceFault,
	[STATUS_RADIO_FAULT]: IconRadioFault,
	[STATUS_DISMOUNTED]: IconDismounted,
	[STATUS_REPORTED_MISSING]: IconReportedMissing,
	[STATUS_FAULT_MUTED]: IconMuted
};

/** Icon of a status flag; flags not known (yet) get a generic one. */
export function statusFlagIcon(flag: number): StatusIcon {
	return ICONS[flag] ?? IconUnknownFlag;
}
