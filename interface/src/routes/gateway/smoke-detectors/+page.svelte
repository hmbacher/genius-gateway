<script lang="ts">
	import type { PageData } from './$types';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import { modals } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import type {
		GeniusDevices,
		GeniusDevice,
		GeniusSmokeDetectorInfo,
		GeniusRadioModuleInfo,
		AlarmLines,
		AlarmLine,
		ReportSettings,
		StaticSystemInformation,
		WifiSettings
	} from '$lib/types/models';
	import {
		GeniusDeviceRegistration,
		AlarmLineAcquisition,
		GeniusAlarmEnding
	} from '$lib/types/enums';
	import type { TunerData } from '$lib/audio/tuner-pipeline';
	import { jsonDateReviver, downloadObjectAsJson } from '$lib/utils/misc';
	import { getSmokeDetectorFaults, getRadioModuleFaults } from '$lib/utils/deviceStatus';
	import { isOldFmModule } from '$lib/genius/line';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import DraggableList from '$lib/components/DraggableList.svelte';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import InfoDialog from '$lib/components/InfoDialog.svelte';
	import EditSmokeDetector from './EditSmokeDetector.svelte';
	import AlarmLog from './AlarmLog.svelte';
	import AcousticDetectionDialog from './AcousticDetectionDialog.svelte';
	import DeviceImportDialog from './DeviceImportDialog.svelte';
	import DeviceDetailsDialog from './DeviceDetailsDialog.svelte';
	import ManualLineEntry from './ManualLineEntry.svelte';
	import PdfReportDialog from './PdfReportDialog.svelte';
	import SmokeDetectorRow from './SmokeDetectorRow.svelte';
	import { matchAcousticResult, matchAcousticUpdate } from './acousticMatch';
	import DeleteAll from '~icons/tabler/trash-x';
	import Add from '~icons/tabler/circle-plus';
	import SmokeDetector from '~icons/custom-icons/smoke-detector-m';
	import Cancel from '~icons/tabler/x';
	import ClipboardList from '~icons/tabler/clipboard-list';
	import Check from '~icons/tabler/check';
	import Save from '~icons/tabler/device-floppy';
	import Load from '~icons/tabler/folder-open';
	import Microphone from '~icons/tabler/microphone';
	import BellRinging from '~icons/tabler/bell-ringing';
	import BellOff from '~icons/tabler/bell-off';

	interface Props {
		data: PageData;
	}

	const GENIUS_DEVICE_DEFAULT_LOCATION = 'Unknown location';

	// Backend rejects request bodies > 16 KB (PsychicHttp MAX_REQUEST_BODY_SIZE).
	// Budget set well below the hard limit because larger chunks have been observed to
	// stall the HTTP task before our handler runs (likely body-read / JsonDocument
	// memory pressure under WS keepalive load — TBC via server-side log).
	// 6 KB ≈ 3 fully-populated devices per chunk; 50-device imports = ~17 round-trips,
	// well within an acceptable interactive latency budget.
	const IMPORT_CHUNK_BYTE_BUDGET = 6000;

	let { data }: Props = $props();

	$effect(() => {
		if (!$user.admin) goto('/');
	});

	function authHeaders(): Record<string, string> {
		return {
			Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
			'Content-Type': 'application/json'
		};
	}

	/** Upsert a single device. Server returns the canonical device on success; the local store
	 *  entry is replaced so anything the server normalized propagates back into the UI. */
	async function apiPutDevice(device: GeniusDevice): Promise<boolean> {
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

	async function apiDeleteDevice(id: number): Promise<boolean> {
		try {
			const response = await fetch('/rest/gateway-devices/device/delete', {
				method: 'POST',
				headers: authHeaders(),
				body: JSON.stringify({ id })
			});
			if (response.status === 200) {
				geniusDevices.devices = geniusDevices.devices.filter((d) => d.id !== id);
				notifications.success('Smoke detector deleted.', 3000);
				return true;
			}
			notifications.error('Deleting smoke detector failed.', 3000);
			return false;
		} catch (error) {
			console.error('Error:', error);
			notifications.error('Deleting smoke detector failed.', 3000);
			return false;
		}
	}

	async function apiReorderDevices(order: number[]): Promise<boolean> {
		try {
			const response = await fetch('/rest/gateway-devices/reorder', {
				method: 'POST',
				headers: authHeaders(),
				body: JSON.stringify({ order })
			});
			if (response.status === 200) return true;
			notifications.error('Reordering failed.', 3000);
			return false;
		} catch (error) {
			console.error('Error:', error);
			notifications.error('Reordering failed.', 3000);
			return false;
		}
	}

	/** Group devices into chunks whose serialized JSON byte size stays under the budget.
	 *  Each chunk always contains at least one device — a device larger than the budget
	 *  is sent on its own and the server may reject it (surfaced as an error). */
	function chunkByBytes(devices: GeniusDevice[]): GeniusDevice[][] {
		const encoder = new TextEncoder();
		const chunks: GeniusDevice[][] = [];
		let current: GeniusDevice[] = [];
		let currentBytes = 2; // outer "[]"
		for (const d of devices) {
			const devBytes = encoder.encode(JSON.stringify(d)).length;
			const separatorBytes = current.length > 0 ? 1 : 0; // comma between elements
			if (current.length > 0 && currentBytes + separatorBytes + devBytes > IMPORT_CHUNK_BYTE_BUDGET) {
				chunks.push(current);
				current = [];
				currentBytes = 2;
			}
			current.push(d);
			currentBytes += (current.length > 1 ? 1 : 0) + devBytes;
		}
		if (current.length > 0) chunks.push(current);
		return chunks;
	}

	/** Atomically replace the full device list using the chunked-import protocol.
	 *  Used for file import and for "delete all" — both cases where holding the whole
	 *  payload in one HTTP body would exceed the backend's body-size limit.
	 *
	 *  `onProgress` lets a progress dialog drive its UI from the chunk loop.
	 *  `signal` cancels in-flight fetches and breaks out of the chunk loop; on user
	 *  abort the function still POSTs /import/abort with an UNbound fetch so the
	 *  server-side session slot is freed even after the signal has fired. */
	async function replaceAllDevices(
		devices: GeniusDevice[],
		onProgress?: (p: import('./DeviceImportDialog.svelte').ImportProgress) => void,
		signal?: AbortSignal
	): Promise<{ ok: boolean; error?: string; aborted?: boolean }> {
		let sessionId = '';
		// Cleanup uses a fresh fetch (no signal) so it completes even after the caller
		// aborted the controller — otherwise we'd leave the server-side session slot held.
		const cleanupSession = async () => {
			if (!sessionId) return;
			await fetch('/rest/gateway-devices/import/abort', {
				method: 'POST',
				headers: authHeaders(),
				body: JSON.stringify({ sessionId })
			}).catch(() => {});
			sessionId = '';
		};
		const isAborted = () => signal?.aborted === true;
		try {
			onProgress?.({ phase: 'starting' });
			const beginRes = await fetch('/rest/gateway-devices/import/begin', {
				method: 'POST',
				headers: authHeaders(),
				signal
			});
			if (beginRes.status !== 200) {
				const msg = beginRes.status === 409
					? 'Another import is in progress. Please try again in a minute.'
					: 'Could not start import.';
				return { ok: false, error: msg };
			}
			sessionId = (await beginRes.json()).sessionId as string;

			const chunks = chunkByBytes(devices);
			let devicesSent = 0;
			for (let i = 0; i < chunks.length; i++) {
				if (isAborted()) {
					await cleanupSession();
					return { ok: false, aborted: true };
				}
				const chunk = chunks[i];
				const chunkRes = await fetch('/rest/gateway-devices/import/chunk', {
					method: 'POST',
					headers: authHeaders(),
					body: JSON.stringify({ sessionId, devices: chunk }),
					signal
				});
				if (chunkRes.status !== 200) {
					const detail = await chunkRes.text().catch(() => '');
					await cleanupSession();
					return {
						ok: false,
						error: `Chunk rejected (${chunkRes.status})` + (detail ? `: ${detail}` : '')
					};
				}
				devicesSent += chunk.length;
				onProgress?.({
					phase: 'uploading',
					chunkIndex: i + 1,
					totalChunks: chunks.length,
					devicesSent,
					devicesTotal: devices.length
				});
			}

			if (isAborted()) {
				await cleanupSession();
				return { ok: false, aborted: true };
			}

			onProgress?.({ phase: 'committing' });
			const commitRes = await fetch('/rest/gateway-devices/import/commit', {
				method: 'POST',
				headers: authHeaders(),
				body: JSON.stringify({ sessionId }),
				signal
			});
			if (commitRes.status !== 200) {
				await cleanupSession();
				return { ok: false, error: 'Commit failed.' };
			}
			sessionId = ''; // committed — no abort needed

			// Server is now authoritative — adopt the submitted list locally.
			geniusDevices.devices = devices;
			return { ok: true };
		} catch (error) {
			// AbortController.abort() causes fetch to throw a DOMException with name 'AbortError'.
			const aborted = isAborted() || (error instanceof DOMException && error.name === 'AbortError');
			if (!aborted) console.error('Error:', error);
			await cleanupSession();
			if (aborted) return { ok: false, aborted: true };
			return { ok: false, error: error instanceof Error ? error.message : String(error) };
		}
	}

	/** Thin wrapper for callers that don't want the progress dialog (delete-all). */
	async function replaceAllDevicesQuiet(devices: GeniusDevice[]): Promise<boolean> {
		const result = await replaceAllDevices(devices);
		if (!result.ok) notifications.error(result.error ?? 'Import failed.', 4000);
		return result.ok;
	}

	function confirmDeleteAll() {
		const count = geniusDevices.devices.length;
		modals.open(ConfirmDialog, {
			title: 'Delete all smoke detectors',
			message:
				`You are about to permanently delete all <strong>${count} smoke detector${count !== 1 ? 's' : ''}</strong> ` +
				`including their complete alarm history.<br><br>` +
				`<strong>This action cannot be undone.</strong> Export a backup using the save button before proceeding.`,
			labels: {
				cancel: { label: 'Cancel', icon: Cancel },
				confirm: { label: 'Delete all', icon: DeleteAll }
			},
			confirmClass: 'btn-error',
			onConfirm: async () => {
				modals.close();
				await replaceAllDevicesQuiet([]);
			}
		});
	}

	function confirmDelete(index: number) {
		modals.open(ConfirmDialog, {
			title: 'Confirm to delete Genius device',
			message:
				'Are you sure you want to delete the Genius device "' +
				geniusDevices.devices[index].smokeDetector.sn +
				' (' +
				geniusDevices.devices[index].location +
				')"?',
			labels: {
				cancel: { label: 'Abort', icon: Cancel },
				confirm: { label: 'Yes', icon: Check }
			},
			onConfirm: async () => {
				const id = geniusDevices.devices[index].id;
				modals.close();
				await apiDeleteDevice(id);
			}
		});
	}

	function handleEdit(index: number) {
		modals.open(EditSmokeDetector, {
			title: 'Edit smoke detector',
			//geniusDevice: { ...geniusDevices.devices[index] }, // Shallow Copy
			geniusDevice: $state.snapshot(geniusDevices.devices[index]), // Deep copy
			onSaveGeniusDevice: async (editedGeniusDevice: GeniusDevice) => {
				await apiPutDevice(editedGeniusDevice);
				modals.close();
				afterDeviceSaved(editedGeniusDevice);
			}
		});
	}

	function handleNewGeniusDevice() {
		modals.open(EditSmokeDetector, {
			title: 'Add smoke detector',
			onSaveGeniusDevice: async (newGeniusDevice: GeniusDevice) => {
				await apiPutDevice(newGeniusDevice);
				modals.close();
				afterDeviceSaved(newGeniusDevice);
			}
		});
		//
	}

	function handleAlarmLog(index: number) {
		modals.open(AlarmLog, {
			title: 'Alarms log',
			geniusDevice: geniusDevices.devices[index]
		});
	}

	function handleDeviceReorder(reorderedDevices: GeniusDevice[]) {
		const orderChanged = reorderedDevices.some(
			(d, i) => d.smokeDetector.sn !== geniusDevices.devices[i]?.smokeDetector.sn
		);
		geniusDevices.devices = reorderedDevices;
		if (orderChanged) apiReorderDevices(reorderedDevices.map((d) => d.id));
	}

	function handleAcousticDetection() {
		if (page.url.protocol !== 'https:') {
			notifications.error('Acoustic device detection requires a secure (HTTPS) connection.', 5000);
			return;
		}
		modals.open(AcousticDetectionDialog, {
			title: 'Acoustic Device Detection',
			onSuccess: (data: TunerData) => handleAcousticResult(data)
		});
	}

	function tunerDataToGeniusDevice(data: TunerData): GeniusDevice {
		const sd: GeniusSmokeDetectorInfo = {
			model: data.productType,
			sn: data.serialNumber,
			productionDate: data.productionDate,
			lastSelftest: data.lastSelftest ?? undefined,
			lastAlarm: data.lastAlarm ?? undefined,
			deinstallationCount: data.deinstallationCount,
			alarmCountTotal: data.alarmCount,
			alarmCountLast3Months: data.alarmCountLast3Months,
			hoursInStorageMode: data.hoursInStorageMode,
			warrantyFlags: data.warrantyFlagsRaw,
			batteryLowFault: data.batteryLowFault,
			deviceFault: data.deviceFault,
			driftState: data.driftState,
			dirtForecastNegative: data.dirtForecastNegative
		};
		// Old FM modules (FM.Basis / FM.Pro) transmit lineId == 0 and an unreliable
		// line byte over SmartSonic — the line must be entered by hand, so we do not
		// trust the readout's line fields for them (see lib/genius/line.ts).
		const oldModule = isOldFmModule(data.radioProductType);
		const rm: GeniusRadioModuleInfo = {
			model: data.radioProductType,
			sn: data.radioSerialNumber ?? 0,
			lineId: oldModule ? 0 : data.lineId,
			lineCharacter: oldModule ? undefined : data.lineCharacter,
			lineNumber: oldModule ? undefined : data.lineNumber,
			radioStateMask: data.radioStateMask,
			radioSwitchMask: data.radioSwitchMask,
			radioInterference: data.radioInterference,
			radioNetworkFault: data.radioNetworkFault
		};
		return {
			id: 0,
			smokeDetector: sd,
			radioModule: rm,
			location: GENIUS_DEVICE_DEFAULT_LOCATION,
			registration: GeniusDeviceRegistration.Acoustic,
			isAlarming: false,
			alarms: [],
			readoutTime: new Date(),
			readoutProtocolVersion: data.protocolVersion
		} as GeniusDevice;
	}

	/** Check if the device's alarm line ID is already configured; if not, offer to add it */
	async function checkAndOfferAlarmLine(device: GeniusDevice) {
		const lineId = device.radioModule.lineId;
		// 0x00000000 = unassigned (ALARMLINES_ID_NONE), 0xFFFFFFFF = broadcast — neither is a user line
		if (!lineId || lineId === 0xffffffff) return;

		try {
			const response = await fetch('/rest/alarm-lines', {
				method: 'GET',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			const alarmLines: AlarmLines = JSON.parse(await response.text(), jsonDateReviver);

			if (alarmLines.lines.some((l) => l.id === lineId)) return;

			modals.open(ConfirmDialog, {
				title: 'Unknown Alarm Line detected',
				message: `The device contains Alarm Line ID ${lineId} which is not yet configured. Would you like to add it?`,
				labels: {
					cancel: { label: 'Skip', icon: Cancel },
					confirm: { label: 'Add alarm line', icon: Check }
				},
				onConfirm: async () => {
					const newLine: AlarmLine = {
						id: lineId,
						name: `Line ${lineId}`,
						created: new Date(),
						acquisition: AlarmLineAcquisition.Acoustic
					};
					alarmLines.lines = [...alarmLines.lines, newLine];
					try {
						const postResponse = await fetch('/rest/alarm-lines', {
							method: 'POST',
							headers: {
								Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
								'Content-Type': 'application/json'
							},
							body: JSON.stringify(alarmLines)
						});
						if (postResponse.status === 200) {
							notifications.success('Alarm line added.', 3000);
						} else {
							notifications.error('Failed to add alarm line.', 3000);
						}
					} catch (error) {
						console.error('Error:', error);
					}
					modals.close();
				}
			});
		} catch (error) {
			console.error('Error checking alarm lines:', error);
		}
	}

	/** Old FM module that still has no manually-entered alarm line — needs attention. */
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

	/**
	 * Post-save follow-ups after a readout/add: warn on faults, then either route old
	 * modules to manual line entry or offer to add a newly-seen alarm line.
	 */
	function afterDeviceSaved(device: GeniusDevice) {
		checkAndWarnDiagnostics(device, () => {
			if (needsManualLineEntry(device)) {
				offerManualLineEntry(device);
			} else {
				checkAndOfferAlarmLine(device);
			}
		});
	}

	/**
	 * Carry a previously manually-entered alarm line from `source` onto `target`'s old
	 * FM module, but only when `source`'s radio module serial confirms it's the SAME
	 * physical hardware. The rotary switch position is a property of the radio module,
	 * not the smoke detector — it should follow the module (e.g. when it's matched on a
	 * different / new device base), but must never leak onto an unrelated module just
	 * because the serial didn't match (e.g. the module was swapped on this base).
	 */
	function preserveManualLineIfSameModule(target: GeniusDevice, source: GeniusDevice) {
		if (
			isOldFmModule(target.radioModule.model) &&
			source.radioModule.lineManual &&
			(source.radioModule.sn ?? 0) > 0 &&
			source.radioModule.sn === target.radioModule.sn
		) {
			target.radioModule.lineId = source.radioModule.lineId;
			target.radioModule.lineCharacter = source.radioModule.lineCharacter;
			target.radioModule.lineNumber = source.radioModule.lineNumber;
			target.radioModule.lineManual = source.radioModule.lineManual;
		}
	}

	/** Silently update an existing device at the given index, preserving location, alarms, and position */
	async function silentUpdateDevice(index: number, newDevice: GeniusDevice) {
		const existing = geniusDevices.devices[index];
		newDevice.id = existing.id;
		newDevice.location = existing.location;
		newDevice.alarms = existing.alarms;
		newDevice.isAlarming = existing.isAlarming;
		preserveManualLineIfSameModule(newDevice, existing);
		const ok = await apiPutDevice(newDevice);
		if (ok) notifications.success('Readout data updated successfully.', 3000);
		afterDeviceSaved(newDevice);
	}

	function handleAcousticResult(data: TunerData) {
		const newDevice = tunerDataToGeniusDevice(data);
		const rwmSN = newDevice.smokeDetector.sn;
		const fmSN = newDevice.radioModule.sn;
		const match = matchAcousticResult(geniusDevices.devices, newDevice);

		switch (match.kind) {
			case 'all-new':
				acousticAddNew(newDevice);
				return;

			case 'rwm-match': {
				const existing = geniusDevices.devices[match.rwmIndex];
				modals.open(ConfirmDialog, {
					title: 'Device already exists',
					message:
						`Smoke detector SN ${rwmSN} already exists at "${existing.location}". ` +
						`Update with acoustic readout data? Location and alarm history will be preserved.`,
					labels: {
						cancel: { label: 'Cancel', icon: Cancel },
						confirm: { label: 'Update', icon: Check }
					},
					onConfirm: async () => {
						modals.close();
						await silentUpdateDevice(match.rwmIndex, newDevice);
					}
				});
				return;
			}

			case 'fm-match': {
				const existing = geniusDevices.devices[match.fmIndex];
				modals.open(ConfirmDialog, {
					title: 'Radio module already assigned',
					message:
						`Radio module SN ${fmSN} is currently assigned to device at "${existing.location}". ` +
						`Replacing will delete the alarm history of the previous device.`,
					labels: {
						cancel: { label: 'Cancel', icon: Cancel },
						confirm: { label: 'Replace', icon: Check }
					},
					onConfirm: () => {
						// Keep location and position, but reset alarms
						newDevice.id = existing.id;
						newDevice.location = existing.location;
						// Same RM serial confirms this is the same physical module, just
						// moved to a different smoke-detector base — pre-fill its known
						// rotary line so the installer doesn't have to recall/re-enter it.
						preserveManualLineIfSameModule(newDevice, existing);
						modals.close();
						modals.open(EditSmokeDetector, {
							title: 'Replace smoke detector',
							geniusDevice: newDevice,
							saveButtonLabel: 'Replace',
							onSaveGeniusDevice: async (editedDevice: GeniusDevice) => {
								await apiPutDevice(editedDevice);
								modals.close();
								afterDeviceSaved(editedDevice);
							}
						});
					}
				});
				return;
			}

			case 'cross-match': {
				const deviceA = geniusDevices.devices[match.rwmIndex];
				const deviceB = geniusDevices.devices[match.fmIndex];
				const idsToDelete = [deviceA.id, deviceB.id];
				modals.open(ConfirmDialog, {
					title: 'Components found in different devices',
					message:
						`Smoke detector SN ${rwmSN} exists at "${deviceA.location}" and ` +
						`radio module SN ${fmSN} exists at "${deviceB.location}". ` +
						`Continuing will delete both devices and create a new one. ` +
						`Location and alarm history will be reset.`,
					labels: {
						cancel: { label: 'Cancel', icon: Cancel },
						confirm: { label: 'Continue', icon: Check }
					},
					onConfirm: () => {
						// Same RM serial confirms deviceB's module is the one in this
						// reading — carry its known rotary line into the new device.
						preserveManualLineIfSameModule(newDevice, deviceB);
						modals.close();
						acousticAddNew(newDevice, 'Delete previous & add device', idsToDelete);
					}
				});
				return;
			}
		}
	}

	function acousticAddNew(device: GeniusDevice, saveButtonLabel: string = 'Add', idsToDeleteFirst: number[] = []) {
		modals.open(EditSmokeDetector, {
			title: 'Add smoke detector',
			geniusDevice: device,
			saveButtonLabel,
			onSaveGeniusDevice: async (newGeniusDevice: GeniusDevice) => {
				// Cross-match path: clean up superseded devices first. Not atomic across requests —
				// if a delete fails mid-flight the user sees the error and can retry.
				for (const id of idsToDeleteFirst) await apiDeleteDevice(id);
				await apiPutDevice(newGeniusDevice);
				modals.close();
				afterDeviceSaved(newGeniusDevice);
			}
		});
	}

	function handleDeviceReadout(targetIndex: number) {
		if (page.url.protocol !== 'https:') {
			notifications.error('Acoustic device detection requires a secure (HTTPS) connection.', 5000);
			return;
		}
		const target = geniusDevices.devices[targetIndex];
		modals.open(AcousticDetectionDialog, {
			title: 'Acoustic Device Readout',
			onSuccess: (data: TunerData) => {
				const newDevice = tunerDataToGeniusDevice(data);
				const rwmSN = newDevice.smokeDetector.sn;
				const fmSN = newDevice.radioModule.sn;
				const match = matchAcousticUpdate(geniusDevices.devices, newDevice, targetIndex);

				switch (match.kind) {
					case 'exact-match':
						silentUpdateDevice(targetIndex, newDevice);
						return;

					case 'fm-mismatch':
						modals.open(ConfirmDialog, {
							title: 'Radio module serial number mismatch',
							message:
								`The smoke detector SN ${rwmSN} matches the device at "${target.location}", but the received radio module SN (${fmSN}) differs from the stored one (${match.storedFmSN}). ` +
								`Replace the stored radio module SN with the received one and update the device?`,
							labels: {
								cancel: { label: 'Cancel', icon: Cancel },
								confirm: { label: 'Update', icon: Check }
							},
							onConfirm: async () => {
								modals.close();
								await silentUpdateDevice(targetIndex, newDevice);
							}
						});
						return;

					case 'different-device': {
						const other = geniusDevices.devices[match.rwmIndex];
						modals.open(ConfirmDialog, {
							title: 'Different device detected',
							message:
								`The received data matches the device at "${other.location}" (SN ${rwmSN}), not the selected device at "${target.location}". ` +
								`Update the detected device instead?`,
							labels: {
								cancel: { label: 'Cancel', icon: Cancel },
								confirm: { label: 'Update', icon: Check }
							},
							onConfirm: async () => {
								modals.close();
								await silentUpdateDevice(match.rwmIndex, newDevice);
							}
						});
						return;
					}

					case 'unknown':
						modals.open(ConfirmDialog, {
							title: 'Unknown device detected',
							message:
								`The received data does not match the device at "${target.location}" and is not known. ` +
								`Would you like to add it as a new device?`,
							labels: {
								cancel: { label: 'Cancel', icon: Cancel },
								confirm: { label: 'Add', icon: Check }
							},
							onConfirm: () => {
								modals.close();
								acousticAddNew(newDevice);
							}
						});
						return;

					case 'partial-mismatch':
						modals.open(InfoDialog, {
							title: 'Mismatched device',
							message:
								`The received data does not match the device at "${target.location}". ` +
								`The detected serial numbers partially match other known devices. ` +
								`You will be guided through the matching process.`,
							variant: 'warning',
							dismiss: { label: 'Continue', icon: Check },
							onDismiss: () => {
								modals.close();
								handleAcousticResult(data);
							}
						});
						return;
				}
			}
		});
	}

	const isSecureContext = $derived(page.url.protocol === 'https:');

	function checkAndWarnDiagnostics(device: GeniusDevice, onDismiss: () => void): void {
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

	function checkImportedDeviceFaults(devices: GeniusDevice[]): void {
		const faulty = devices
			.filter((d) => d.readoutTime)
			.map((d) => ({
				sn: d.smokeDetector.sn,
				faults: [...getSmokeDetectorFaults(d.smokeDetector), ...getRadioModuleFaults(d.radioModule)]
			}))
			.filter((x) => x.faults.length > 0);
		if (faulty.length === 0) return;
		const items = faulty
			.map((x) => '<li><strong>SN ' + x.sn + '</strong>: ' + x.faults.join(', ') + '</li>')
			.join('');
		modals.open(InfoDialog, {
			title:
				faulty.length === 1 ? 'Smoke Detector Fault Detected' : 'Smoke Detector Faults Detected',
			message:
				'<ul>' +
				items +
				'</ul>' +
				'<br><strong>If these detectors are still in use, they should be replaced as soon as possible.</strong>',
			variant: 'error',
			onDismiss: () => modals.close()
		});
	}

	function openDeviceDetails(index: number) {
		modals.open(DeviceDetailsDialog, {
			title: 'Device Details',
			device: geniusDevices.devices[index],
			onReadout: () => handleDeviceReadout(index),
			onSetLine: () => handleEdit(index)
		});
	}

	let pdfGenerating = $state(false);

	function handleGenerateReport() {
		pdfGenerating = true;
		const devices = $state.snapshot(geniusDevices).devices;
		const headers: Record<string, string> = {
			Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
			'Content-Type': 'application/json'
		};
		modals.open(PdfReportDialog, {
			task: async (onProgress) => {
				try {
					onProgress('Fetching report data');
					const [reportRes, systemRes, wifiRes] = await Promise.all([
						fetch('/rest/report-settings', { headers }),
						fetch('/rest/systemStatus', { headers }),
						fetch('/rest/wifiSettings', { headers })
					]);
					const reportSettings: ReportSettings = await reportRes.json();
					const systemInfo: StaticSystemInformation = await systemRes.json();
					const wifiSettings: WifiSettings = await wifiRes.json();
					const { generateSmokeDetectorReport } = await import('./pdfReport');
					await generateSmokeDetectorReport(
						devices,
						reportSettings,
						{ hostname: wifiSettings.hostname, firmwareVersion: systemInfo.firmware_version },
						onProgress
					);
				} finally {
					pdfGenerating = false;
				}
			}
		});
	}

	let files = $state<FileList | undefined>(undefined);
	let fileInput = $state<HTMLInputElement | undefined>(undefined);

	/**
	 * v0 schema kept around so migrations can read it without `any` casts.
	 * - model 0 in v0 == GeniusPlusX (now 3) / FmBasisX (now 4)
	 * - radioModule.productionDate was removed entirely in v1
	 */
	type LegacyGeniusDevice = {
		smokeDetector: { model?: number; [k: string]: unknown };
		radioModule: { model?: number; productionDate?: unknown; [k: string]: unknown };
		[k: string]: unknown;
	};

	function migrateGeniusDevices(data: GeniusDevices, fromVersion: number): void {
		if (fromVersion < 1) {
			for (const dev of data.devices as unknown as LegacyGeniusDevice[]) {
				if (dev.smokeDetector.model === 0) dev.smokeDetector.model = 3;
				if (dev.radioModule.model === 0) dev.radioModule.model = 4;
				delete dev.radioModule.productionDate;
			}
		}
		if (fromVersion < 2) {
			// Old FM modules (FM.Basis / FM.Pro) cannot expose a trustworthy line. Earlier
			// backups stored the unreliable acoustic line; purge it unless entered by hand,
			// so the device surfaces as "Line required".
			for (const dev of data.devices) {
				const rm = dev.radioModule;
				if (isOldFmModule(rm.model) && !rm.lineManual) {
					rm.lineId = undefined;
					rm.lineCharacter = undefined;
					rm.lineNumber = undefined;
				}
			}
		}
	}

	function clearAlarmingDevices(devices: GeniusDevice[]): void {
		for (const device of devices) {
			if (!device.isAlarming) continue;
			device.isAlarming = false;
			if (device.alarms.length > 0) {
				const lastAlarm = device.alarms[device.alarms.length - 1];
				if (lastAlarm.endingReason === GeniusAlarmEnding.AlarmActive) {
					lastAlarm.endTime = new Date();
					lastAlarm.endingReason = GeniusAlarmEnding.ByImport;
				}
			}
		}
	}

	$effect(() => {
		if (!files) return;
		// Note that `files` is of type `FileList`, not an Array:
		// https://developer.mozilla.org/en-US/docs/Web/API/FileList
		const reader = new FileReader();
		let cancelled = false;
		reader.onload = async () => {
			if (cancelled) return;
			const CURRENT_VERSION = 2;
			const fileContent = reader.result as string;
			try {
				const importedGeniusDevices = JSON.parse(fileContent, jsonDateReviver) as GeniusDevices;
				const fileVersion = importedGeniusDevices?.version ?? 0;

				if (!importedGeniusDevices || !Array.isArray(importedGeniusDevices.devices)) {
					notifications.error('Invalid smoke detectors format.', 3000);
				} else if (fileVersion > CURRENT_VERSION) {
					modals.open(InfoDialog, {
						title: 'Incompatible File Version',
						message: `The file has version <strong>v${fileVersion}</strong>, but version <strong>v${CURRENT_VERSION}</strong> is required.<br><br>Please export a fresh backup from a current device before importing.`,
						variant: 'error',
						onDismiss: () => modals.close()
					});
				} else {
					if (fileVersion < CURRENT_VERSION) {
						migrateGeniusDevices(importedGeniusDevices, fileVersion);
					}

					const finishImport = () => {
						modals.open(DeviceImportDialog, {
							totalDevices: importedGeniusDevices.devices.length,
							task: (onProgress, signal) =>
								replaceAllDevices(importedGeniusDevices.devices, onProgress, signal),
							onSuccess: () => {
								if (fileVersion < CURRENT_VERSION) {
									modals.open(InfoDialog, {
										title: 'Migration Successful',
										message: `The file was from an older backup (v${fileVersion}) and has been automatically migrated to the current format (v${CURRENT_VERSION}).<br><br>Please verify your smoke detector configuration and export a fresh backup.`,
										variant: 'info',
										onDismiss: () => {
											modals.close();
											checkImportedDeviceFaults(importedGeniusDevices.devices);
										}
									});
								} else {
									notifications.success('Smoke detectors imported.', 3000);
									checkImportedDeviceFaults(importedGeniusDevices.devices);
								}
							}
						});
					};

					const alarmingCount = importedGeniusDevices.devices.filter((d) => d.isAlarming).length;
					if (alarmingCount > 0) {
						modals.open(ConfirmDialog, {
							title: 'Alarming Devices in Import',
							message: `<strong>${alarmingCount}</strong> device(s) in the import file are marked as alarming.<br><br>Keeping the alarm state may trigger automations in connected integrations (e.g. Home Assistant) — useful for testing purposes.`,
							labels: {
								cancel: { label: 'Keep Alarm State', icon: BellRinging },
								confirm: { label: 'Clear Alarm State', icon: BellOff }
							},
							confirmClass: 'btn-success',
							cancelClass: 'btn-warning',
							onCancel: () => {
								modals.close();
								finishImport();
							},
							onConfirm: () => {
								modals.close();
								clearAlarmingDevices(importedGeniusDevices.devices);
								finishImport();
							}
						});
					} else {
						finishImport();
					}
				}
			} catch (error) {
				console.error('Error parsing file:', error);
				notifications.error('Error parsing file.', 3000);
			}

			// Reset files and clear input value to allow re-selection of the same file
			files = undefined;
			if (fileInput) {
				fileInput.value = '';
			}
		};

		reader.onerror = (ev) => {
			if (cancelled) return;
			console.log('Error reading the file:', ev);
			notifications.error('Error reading file.', 3000);
			files = undefined;
			if (fileInput) fileInput.value = '';
		};

		reader.readAsText(files[0]);

		// Cleanup: if the component unmounts (or `files` changes) before the
		// read finishes, abort the FileReader and ignore any late callbacks.
		return () => {
			cancelled = true;
			reader.onload = null;
			reader.onerror = null;
			if (reader.readyState === FileReader.LOADING) reader.abort();
		};
	});
</script>

{#if $user.admin}
	<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
		<SettingsCard collapsible={false} maxwidth="max-w-xl md:max-w-3xl" overflowY="visible">
			{#snippet icon()}
				<SmokeDetector class="h-6 w-6" />
			{/snippet}
			{#snippet title()}
				<span>Genius Devices</span>
			{/snippet}
			{#snippet actions()}
				<div class="tooltip tooltip-bottom" data-tip="Add smoke detector">
					<button
						class="btn btn-primary btn-md"
						aria-label="Add smoke detector"
						disabled={!geniusDevices.isLoaded}
						onclick={handleNewGeniusDevice}
					>
						<Add class="h-6 w-6" />
					</button>
				</div>
				<div
					class="tooltip tooltip-bottom"
					data-tip={isSecureContext
						? 'Add smoke detector via acoustic detection'
						: 'Acoustic device detection requires a secure (HTTPS) connection'}
				>
					<button
						class={isSecureContext
							? 'btn btn-primary btn-md'
							: 'btn btn-warning btn-md'}
						aria-label={isSecureContext
							? 'Add smoke detector via acoustic detection'
							: 'Acoustic device detection requires a secure (HTTPS) connection'}
						disabled={!geniusDevices.isLoaded}
						onclick={handleAcousticDetection}
					>
						<span class="relative inline-flex">
							<Microphone class="h-6 w-6" />
							{#if isSecureContext}
								<Add class="absolute -bottom-1 -right-1 h-3.5 w-3.5" />
							{:else}
								<Cancel class="absolute -bottom-1 -right-1 h-3.5 w-3.5" />
							{/if}
						</span>
					</button>
				</div>
				<div class="tooltip tooltip-left" data-tip="Load smoke detector configuration from file">
					<label
						for="upload"
						class="btn btn-primary btn-md"
						class:btn-disabled={!geniusDevices.isLoaded}
						aria-label="Load smoke detector configuration from file"
					>
						<Load class="h-6 w-6" />
					</label>
					<input
						bind:files
						bind:this={fileInput}
						id="upload"
						type="file"
						class="hidden"
						disabled={!geniusDevices.isLoaded}
					/>
				</div>
				<div class="tooltip tooltip-left" data-tip="Save smoke detector configuration to file">
					<button
						class="btn btn-primary btn-md"
						aria-label="Save smoke detector configuration to file"
						disabled={!geniusDevices.isLoaded}
						onclick={() =>
							downloadObjectAsJson(
								{ version: 2, devices: geniusDevices.devices },
								'genius-smoke-detectors'
							)}
					>
						<Save class="h-6 w-6" />
					</button>
				</div>
				<div class="tooltip tooltip-left" data-tip="Generate PDF report">
					<button
						class="btn btn-primary btn-md"
						aria-label="Generate PDF report"
						disabled={!geniusDevices.isLoaded ||
							geniusDevices.devices.length === 0 ||
							pdfGenerating}
						onclick={handleGenerateReport}
					>
						{#if pdfGenerating}
							<span class="loading loading-spinner loading-sm"></span>
						{:else}
							<ClipboardList class="h-6 w-6" />
						{/if}
					</button>
				</div>
				<div class="tooltip tooltip-left" data-tip="Delete all smoke detectors">
					<button
						class="btn btn-error btn-md"
						aria-label="Delete all smoke detectors"
						disabled={!geniusDevices.isLoaded || geniusDevices.devices.length === 0}
						onclick={confirmDeleteAll}
					>
						<DeleteAll class="h-6 w-6" />
					</button>
				</div>
			{/snippet}
			{#if !geniusDevices.isLoaded}
				<div class="divider my-0"></div>
				<div class="flex justify-center p-6">
					<Spinner text="" />
				</div>
			{:else if geniusDevices.devices.length === 0}
				<div class="divider my-0"></div>
				<div class="flex flex-col items-center justify-center p-4 text-sm text-gray-500">
					<p class="mb-4 font-semibold">No smoke detectors configured yet.</p>
					<p class="mx-20 text-center">Click the "+" button to add your first smoke detector.</p>
				</div>
			{:else}
				<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
					<!-- Header row -->
					<div
						class="hidden md:grid grid-cols-[30px_1fr_1fr_1fr_65px_50px_120px] gap-2 bg-base-200 px-4 py-2 rounded-t-lg font-bold text-sm"
					>
						<div></div>
						<!-- Space for grip icon -->
						<div>Location</div>
						<div>Smoke Detector</div>
						<div>Radio Module</div>
						<div class="text-center">Alarms</div>
						<div class="text-center">Service</div>
						<div class="text-right">Manage</div>
					</div>

					<!-- Draggable device list -->
					<DraggableList
						items={geniusDevices.devices}
						onReorder={handleDeviceReorder}
						useHandleMode={true}
						class="space-y-1"
					>
						{#snippet children({ item: device, index }: { item: GeniusDevice; index: number })}
							<SmokeDetectorRow
								{device}
								{index}
								onEdit={handleEdit}
								onDelete={confirmDelete}
								onAlarmLog={handleAlarmLog}
								onDetails={openDeviceDetails}
							/>
						{/snippet}
					</DraggableList>
				</div>
			{/if}
		</SettingsCard>
	</div>
{/if}
