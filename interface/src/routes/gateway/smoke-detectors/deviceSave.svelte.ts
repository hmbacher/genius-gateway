/**
 * Shared editor and save pipeline for smoke detectors.
 *
 * Used by the smoke-detector list page and by the Overview (start page), so that adding or
 * editing a detector behaves identically no matter where the editor was opened from:
 * PUT the device, warn about faults, then either route old FM modules to manual line entry
 * or offer to register a newly seen alarm line.
 */
import type { Component } from 'svelte';
import { modals } from 'svelte-modals';
import { notifications } from '$lib/components/toasts/notifications';
import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
import type { GeniusDevice, HASettings } from '$lib/types/models';
import { jsonDateReviver } from '$lib/utils/misc';
import { getSmokeDetectorFaults, getRadioModuleFaults } from '$lib/utils/deviceStatus';
import { isOldFmModule } from '$lib/genius/line';
import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
import InfoDialog from '$lib/components/InfoDialog.svelte';
import EditSmokeDetector from './EditSmokeDetector.svelte';
import ManualLineEntry from './ManualLineEntry.svelte';
import { authHeaders, fetchRegisteredLineIds, registerAlarmLine } from './restApi';
import Cancel from '~icons/tabler/x';
import Check from '~icons/tabler/check';
import Edit from '~icons/tabler/pencil';

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

// ── Home Assistant entity-ID rename ────────────────────────────────────────────
// With the HA option "update entity IDs" on, saving a changed location makes the gateway
// rename that detector's entities. The editor warns about it beforehand; the gateway reports
// the rename itself through a WebSocket event (see watchHaEntityRename).

/** Last known answer; kept if a later request fails. */
let haRenamesEntityIds = false;

/**
 * Ask the gateway whether a changed location renames the Home Assistant entities.
 *
 * Asked again right before the editor opens: the option can be changed elsewhere (another
 * tab, REST) while a page stays open, and the editor's warning has to match what saving will
 * really do.
 */
async function loadHaRenameOption(): Promise<boolean> {
	try {
		const response = await fetch('/rest/haSettings', { method: 'GET', headers: authHeaders() });
		if (response.status === 200) {
			const settings = (await response.json()) as HASettings;
			haRenamesEntityIds = settings.enabled && settings.rename_entity_ids;
		}
	} catch (error) {
		console.error('Failed to load the Home Assistant settings:', error);
	}
	return haRenamesEntityIds;
}

export interface DeviceEditorOptions {
	/** Dialog title. Default: "Edit smoke detector". */
	title?: string;
	/** Icon left of the title, matching the button that opened the editor. Default: pencil. */
	titleIcon?: Component<{ class?: string }>;
	/** Label of the save button. Default: the editor's own ("Save"). */
	saveButtonLabel?: string;
	/** Runs right before the device is saved, e.g. to delete the devices it replaces. */
	beforeSave?: () => Promise<void>;
	/** Runs after a successful save, before the follow-up dialogs. */
	onSaved?: (device: GeniusDevice) => void;
}

/**
 * Open the smoke detector editor and run the full save pipeline when the user saves.
 *
 * @param device Detector to edit, or the pre-filled data of a new one. Omit it for an empty editor.
 *
 * A failed save closes the editor and stops there - no follow-up dialogs for a device the
 * gateway did not store.
 */
export async function openDeviceEditor(device?: GeniusDevice, options: DeviceEditorOptions = {}) {
	// Copy first: the device list may change while the HA option is being fetched.
	const copy = device ? $state.snapshot(device) : undefined;

	// Only a detector the gateway already knows can have entities to rename.
	const exists = copy !== undefined && geniusDevices.devices.some((d) => d.id === copy.id);
	const warnOnRename = exists ? await loadHaRenameOption() : false;

	modals.open(EditSmokeDetector, {
		title: options.title ?? 'Edit smoke detector',
		titleIcon: options.titleIcon ?? Edit,
		haRenamesEntityIds: warnOnRename,
		geniusDevice: copy,
		...(options.saveButtonLabel ? { saveButtonLabel: options.saveButtonLabel } : {}),
		onSaveGeniusDevice: async (edited: GeniusDevice) => {
			await options.beforeSave?.();
			const saved = await apiPutDevice(edited);
			modals.close();
			if (!saved) return;
			options.onSaved?.(edited);
			afterDeviceSaved(edited);
		}
	});
}
