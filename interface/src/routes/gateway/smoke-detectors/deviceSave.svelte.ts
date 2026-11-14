/**
 * Shared save pipeline for smoke detectors.
 *
 * Used by the smoke-detector list page and by the Overview (start page), so that editing a
 * detector behaves identically no matter where the edit dialog was opened from:
 * PUT the device, warn about faults, then either route old FM modules to manual line entry
 * or offer to register a newly seen alarm line.
 */
import { get } from 'svelte/store';
import { modals } from 'svelte-modals';
import { page } from '$app/state';
import { user } from '$lib/stores/user';
import { notifications } from '$lib/components/toasts/notifications';
import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
import type { AlarmLine, AlarmLines, GeniusDevice } from '$lib/types/models';
import { AlarmLineAcquisition } from '$lib/types/enums';
import { jsonDateReviver } from '$lib/utils/misc';
import { getSmokeDetectorFaults, getRadioModuleFaults } from '$lib/utils/deviceStatus';
import { isOldFmModule } from '$lib/genius/line';
import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
import InfoDialog from '$lib/components/InfoDialog.svelte';
import EditSmokeDetector from './EditSmokeDetector.svelte';
import ManualLineEntry from './ManualLineEntry.svelte';
import Cancel from '~icons/tabler/x';
import Check from '~icons/tabler/check';
import Edit from '~icons/tabler/pencil';

export function authHeaders(): Record<string, string> {
	return {
		Authorization: page.data.features.security ? 'Bearer ' + get(user).bearer_token : 'Basic',
		'Content-Type': 'application/json'
	};
}

/** Upsert a single device. Server returns the canonical device on success; the local store
 *  entry is replaced so anything the server normalized propagates back into the UI. */
export async function apiPutDevice(device: GeniusDevice): Promise<boolean> {
	try {
		const response = await fetch('/rest/gateway-devices/device', {
			method: 'PUT',
			headers: authHeaders(),
			body: JSON.stringify(device)
		});
		if (response.status === 200) {
			const saved = JSON.parse(await response.text(), jsonDateReviver) as GeniusDevice;
			const idx = geniusDevices.devices.findIndex((d) => d.id === saved.id);
			if (idx >= 0) geniusDevices.devices[idx] = saved;
			else geniusDevices.devices = [...geniusDevices.devices, saved];
			notifications.success('Smoke detector saved.', 3000);
			return true;
		}
		notifications.error('Saving smoke detector failed.', 3000);
		return false;
	} catch (error) {
		console.error('Error:', error);
		notifications.error('Saving smoke detector failed.', 3000);
		return false;
	}
}

/** Fetch the IDs of the alarm lines currently configured on the gateway (empty on failure). */
export async function fetchRegisteredLineIds(): Promise<number[]> {
	const alarmLines = await fetchAlarmLines();
	return alarmLines ? alarmLines.lines.map((l) => l.id >>> 0) : [];
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

/** Check if the device's alarm line ID is already configured; if not, offer to add it */
async function checkAndOfferAlarmLine(device: GeniusDevice) {
	const lineId = device.radioModule.lineId;
	// 0x00000000 = unassigned (ALARMLINES_ID_NONE), 0xFFFFFFFF = broadcast - neither is a user line
	if (!lineId || lineId === 0xffffffff) return;

	const registeredIds = await fetchRegisteredLineIds();
	if (registeredIds.includes(lineId >>> 0)) return;

	modals.open(ConfirmDialog, {
		title: 'Unknown Alarm Line detected',
		message: `The device contains Alarm Line ID ${lineId} which is not yet configured. Would you like to add it?`,
		labels: {
			cancel: { label: 'Skip', icon: Cancel },
			confirm: { label: 'Add alarm line', icon: Check }
		},
		onConfirm: async () => {
			await registerAlarmLine(lineId);
			modals.close();
		}
	});
}

/** Old FM module that still has no manually-entered alarm line - needs attention. */
function needsManualLineEntry(device: GeniusDevice): boolean {
	return isOldFmModule(device.radioModule.model) && !device.radioModule.lineCharacter;
}

/** Open the manual line-entry dialog for an old FM module and persist the chosen line. */
function offerManualLineEntry(device: GeniusDevice) {
	modals.open(ManualLineEntry, {
		title: 'Set alarm line',
		geniusDevice: device,
		saveButtonLabel: 'Save line',
		onSave: async (updated: GeniusDevice) => {
			const ok = await apiPutDevice(updated);
			modals.close();
			if (ok) notifications.success('Alarm line set.', 3000);
		}
	});
}

export function checkAndWarnDiagnostics(device: GeniusDevice, onDismiss: () => void): void {
	if (!device.readoutTime) {
		onDismiss();
		return;
	}
	const faults = [
		...getSmokeDetectorFaults(device.smokeDetector),
		...getRadioModuleFaults(device.radioModule)
	];
	if (faults.length === 0) {
		onDismiss();
		return;
	}
	modals.open(InfoDialog, {
		title: 'Smoke Detector Fault Detected',
		message:
			'<ul>' +
			faults.map((f) => '<li>' + f + '</li>').join('') +
			'</ul>' +
			'<br><strong>This smoke detector is malfunctioning and should be replaced as soon as possible.</strong>',
		variant: 'error',
		onDismiss: () => {
			modals.close();
			onDismiss();
		}
	});
}

/**
 * Post-save follow-ups after a readout/add/edit: warn on faults, then either route old
 * modules to manual line entry or offer to add a newly-seen alarm line.
 */
export function afterDeviceSaved(device: GeniusDevice) {
	checkAndWarnDiagnostics(device, () => {
		if (needsManualLineEntry(device)) {
			offerManualLineEntry(device);
		} else {
			checkAndOfferAlarmLine(device);
		}
	});
}

/** Open the edit dialog for a device (by id) and run the full save pipeline on save. */
export function openEditDevice(device: GeniusDevice) {
	modals.open(EditSmokeDetector, {
		title: 'Edit smoke detector',
		titleIcon: Edit,
		geniusDevice: $state.snapshot(device), // Deep copy
		onSaveGeniusDevice: async (editedGeniusDevice: GeniusDevice) => {
			await apiPutDevice(editedGeniusDevice);
			modals.close();
			afterDeviceSaved(editedGeniusDevice);
		}
	});
}
