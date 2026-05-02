<script lang="ts">
	import type { PageData } from './$types';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import { modals } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { GeniusDevices, GeniusDevice, GeniusSmokeDetectorInfo, GeniusRadioModuleInfo, AlarmLines, AlarmLine } from '$lib/types/models';
	import { GeniusDeviceRegistration, GeniusSmokeDetector, GeniusRadioModule, AlarmLineAcquisition } from '$lib/types/enums';
	import type { TunerData } from '$lib/audio/tuner-pipeline';
	import { jsonDateReviver, downloadObjectAsJson } from '$lib/utils/misc';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import DraggableList from '$lib/components/DraggableList.svelte';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import InfoDialog from '$lib/components/InfoDialog.svelte';
	import EditSmokeDetector from './EditSmokeDetector.svelte';
	import AlarmLog from './AlarmLog.svelte';
	import AcousticDetectionDialog from './AcousticDetectionDialog.svelte';
	import DeviceDetailsDialog from './DeviceDetailsDialog.svelte';
	import Delete from '~icons/tabler/trash';
	import Add from '~icons/tabler/circle-plus';
	import Edit from '~icons/tabler/pencil';
	import Logs from '~icons/tabler/logs';
	import SmokeDetector from '~icons/custom-icons/smoke-detector-m';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Grip from '~icons/tabler/grip-vertical';
	import Save from '~icons/tabler/device-floppy';
	import Load from '~icons/tabler/folder-open';
	import Microphone from '~icons/tabler/microphone';
	import ListDetails from '~icons/tabler/list-details';
	import CalendarExclamation from '~icons/tabler/calendar-exclamation';
	import MicrophoneOff from '~icons/tabler/microphone-off';
	import Award from '~icons/tabler/award';
	import StatusOk from '~icons/tabler/circle-check';
	import StatusFault from '~icons/tabler/circle-x';
	import AntennaOff from '~icons/tabler/antenna-off';
	import { dragHandle } from 'svelte-dnd-action';

	interface Props {
		data: PageData;
	}

	const GENIUS_DEVICE_DEFAULT_LOCATION = 'Unknown location';

	let { data }: Props = $props();

	async function postGeniusDevices() {
		try {
			const response = await fetch('/rest/gateway-devices', {
				method: 'POST',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify({ devices: geniusDevices.devices } as GeniusDevices)
			});

			if (response.status == 200) {
				notifications.success('Smoke detectors updated.', 3000);
				geniusDevices.devices = (
					JSON.parse(await response.text(), jsonDateReviver) as GeniusDevices
				).devices;
			} else {
				notifications.error('Updating smoke detectors failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
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
				geniusDevices.devices.splice(index, 1);
				modals.close();
				await postGeniusDevices();
			}
		});
	}

	function handleEdit(index: number) {
		modals.open(EditSmokeDetector, {
			title: 'Edit smoke detector',
			//geniusDevice: { ...geniusDevices.devices[index] }, // Shallow Copy
			geniusDevice: $state.snapshot(geniusDevices.devices[index]), // Deep copy
			onSaveGeniusDevice: async (editedGeniusDevice: GeniusDevice) => {
				geniusDevices.devices[index] = editedGeniusDevice;
				await postGeniusDevices();
				modals.close();
			}
		});
	}

	function handleNewGeniusDevice() {
		modals.open(EditSmokeDetector, {
			title: 'Add smoke detector',
			onSaveGeniusDevice: async (newGeniusDevice: GeniusDevice) => {
				geniusDevices.devices = [...geniusDevices.devices, newGeniusDevice];
				await postGeniusDevices();
				modals.close();
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
		if (orderChanged) postGeniusDevices();
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
		const rm: GeniusRadioModuleInfo = {
			model: data.radioProductType,
			sn: data.radioSerialNumber ?? 0,
			lineId: data.lineId,
			lineCharacter: data.lineCharacter,
			lineNumber: data.lineNumber,
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
		if (!lineId) return;

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

	/** Silently update an existing device at the given index, preserving location, alarms, and position */
	async function silentUpdateDevice(index: number, newDevice: GeniusDevice) {
		const existing = geniusDevices.devices[index];
		newDevice.id = existing.id;
		newDevice.location = existing.location;
		newDevice.alarms = existing.alarms;
		newDevice.isAlarming = existing.isAlarming;
		geniusDevices.devices[index] = newDevice;
		await postGeniusDevices();
		notifications.success('Readout data updated successfully.', 3000);
		await checkAndOfferAlarmLine(newDevice);
	}

	function handleAcousticResult(data: TunerData) {
		const newDevice = tunerDataToGeniusDevice(data);
		const rwmSN = newDevice.smokeDetector.sn;
		const fmSN = newDevice.radioModule.sn;

		const rwmMatch = geniusDevices.devices.findIndex((d) => d.smokeDetector.sn === rwmSN);
		const fmMatch = fmSN > 0 ? geniusDevices.devices.findIndex((d) => d.radioModule.sn === fmSN) : -1;

		if (rwmMatch === -1 && fmMatch === -1) {
			// Fall 1: Both new → open EditDialog to add
			acousticAddNew(newDevice);
		} else if (rwmMatch >= 0 && (fmMatch === -1 || fmMatch === rwmMatch)) {
			// Fall 2 & 3: RWM exists (same combo or FM is new) → confirm update
			const existing = geniusDevices.devices[rwmMatch];
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
					await silentUpdateDevice(rwmMatch, newDevice);
				}
			});
		} else if (rwmMatch === -1 && fmMatch >= 0) {
			// Fall 4: FM exists but RWM is new → confirm replace (alarm history lost)
			const existing = geniusDevices.devices[fmMatch];
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
					modals.close();
					modals.open(EditSmokeDetector, {
						title: 'Replace smoke detector',
						geniusDevice: newDevice,
						saveButtonLabel: 'Replace',
						onSaveGeniusDevice: async (editedDevice: GeniusDevice) => {
							geniusDevices.devices[fmMatch] = editedDevice;
							await postGeniusDevices();
							modals.close();
							await checkAndOfferAlarmLine(editedDevice);
						}
					});
				}
			});
		} else if (rwmMatch >= 0 && fmMatch >= 0 && rwmMatch !== fmMatch) {
			// Fall 5: Cross-duplicate (RWM in device A, FM in device B)
			const deviceA = geniusDevices.devices[rwmMatch];
			const deviceB = geniusDevices.devices[fmMatch];
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
					// Remove both old devices
					geniusDevices.devices = geniusDevices.devices.filter(
						(d) => d.smokeDetector.sn !== rwmSN && d.radioModule.sn !== fmSN
					);
					modals.close();
					acousticAddNew(newDevice, 'Delete previous & add device');
				}
			});
		}
	}

	function acousticAddNew(device: GeniusDevice, saveButtonLabel: string = 'Add') {
		modals.open(EditSmokeDetector, {
			title: 'Add smoke detector',
			geniusDevice: device,
			saveButtonLabel,
			onSaveGeniusDevice: async (newGeniusDevice: GeniusDevice) => {
				geniusDevices.devices = [...geniusDevices.devices, newGeniusDevice];
				await postGeniusDevices();
				modals.close();
				await checkAndOfferAlarmLine(newGeniusDevice);
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
				const rwmMatch = geniusDevices.devices.findIndex((d) => d.smokeDetector.sn === rwmSN);
				const fmMatch = fmSN > 0 ? geniusDevices.devices.findIndex((d) => d.radioModule.sn === fmSN) : -1;

				if (rwmMatch === targetIndex && (fmMatch === -1 || fmMatch === targetIndex)) {
					// Case A: RWM matches target device
					const storedFmSN = target.radioModule.sn;
					if (fmMatch === -1 && fmSN > 0 && storedFmSN > 0 && storedFmSN !== fmSN) {
						// Case A2: RWM matches but received FM-SN differs from stored FM-SN → confirm
						modals.open(ConfirmDialog, {
							title: 'Radio module serial number mismatch',
							message:
								`The smoke detector SN ${rwmSN} matches the device at "${target.location}", but the received radio module SN (${fmSN}) differs from the stored one (${storedFmSN}). ` +
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
					} else {
						// Case A1: Exact match (or no FM module) → silent update
						silentUpdateDevice(targetIndex, newDevice);
					}
				} else if (rwmMatch >= 0 && rwmMatch === fmMatch && rwmMatch !== targetIndex) {
					// Case B: Both SNs belong to a different known device → ask to update that one
					const other = geniusDevices.devices[rwmMatch];
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
							await silentUpdateDevice(rwmMatch, newDevice);
						}
					});
				} else if (rwmMatch === -1 && fmMatch === -1) {
					// Case C: Neither SN known → ask to add as new
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
				} else {
					// Case D: Partial match (one SN known, not this device) → warn + delegate to Feature 1 logic
					modals.open(InfoDialog, {
						title: 'Mismatched device',
						message:
							`The received data does not match the device at "${target.location}". ` +
							`The detected serial numbers partially match other known devices. ` +
							`You will be guided through the matching process.`,
						variant: 'warning',
						onDismiss: () => {
							modals.close();
							handleAcousticResult(data);
						}
					});
				}
			}
		});
	}

	const isSecureContext = $derived(page.url.protocol === 'https:');

	const ONE_YEAR_MS = 365.25 * 24 * 60 * 60 * 1000;

	function hasReadout(device: GeniusDevice): boolean {
		return device.registration === GeniusDeviceRegistration.Acoustic && !!device.readoutTime;
	}

	function isStaleReadout(device: GeniusDevice): boolean {
		if (!device.readoutTime) return false;
		return Date.now() - device.readoutTime.getTime() > ONE_YEAR_MS;
	}

	function getSmokeDetectorModelName(model?: number): string {
		switch (model) {
			case GeniusSmokeDetector.GeniusH: return 'Genius H';
			case GeniusSmokeDetector.GeniusHx: return 'Genius Hx';
			case GeniusSmokeDetector.GeniusPlus: return 'Genius Plus';
			case GeniusSmokeDetector.GeniusPlusX: return 'Genius Plus X';
			default: return '';
		}
	}

	function getRadioModuleModelName(model?: number): string {
		switch (model) {
			case GeniusRadioModule.FmBasis: return 'FM Basis';
			case GeniusRadioModule.FmPro: return 'FM Pro';
			case GeniusRadioModule.FmMcp: return 'FM MCP';
			case GeniusRadioModule.FmBasisX: return 'FM Basis X';
			case GeniusRadioModule.FmProX: return 'FM Pro X';
			default: return '';
		}
	}

	function hasRadioModule(rm: GeniusRadioModuleInfo): boolean {
		return rm.model !== GeniusRadioModule.None && (rm.sn ?? 0) > 0;
	}

	function getSmokeDetectorFaults(sd: GeniusSmokeDetectorInfo): string[] {
		const faults: string[] = [];
		if (sd.batteryLowFault) faults.push('Battery low');
		if (sd.deviceFault) faults.push('Device fault');
		const drift = sd.driftState ?? 0;
		if (drift >= 4) faults.push(`Drift defect (state ${drift})`);
		else if (drift >= 2) faults.push(`Drift warning (state ${drift})`);
		if (sd.dirtForecastNegative) faults.push('Dirt forecast negative');
		if ((sd.warrantyFlags ?? 0) > 0) faults.push('Warranty flag(s) set');
		return faults;
	}

	function getRadioModuleFaults(rm: GeniusRadioModuleInfo): string[] {
		const faults: string[] = [];
		if (rm.radioNetworkFault) faults.push('Radio network fault');
		return faults;
	}

	function openDeviceDetails(index: number) {
		modals.open(DeviceDetailsDialog, {
			title: 'Device Details',
			device: geniusDevices.devices[index],
			onReadout: () => handleDeviceReadout(index)
		});
	}

	let files: any = $state();
	let fileInput = $state<HTMLInputElement>();

	$effect(() => {
		if (files) {
			// Note that `files` is of type `FileList`, not an Array:
			// https://developer.mozilla.org/en-US/docs/Web/API/FileList
			const reader = new FileReader();
			reader.onload = async () => {
				const fileContent = reader.result as string;
				try {
					const CURRENT_VERSION = 1;
					const importedGeniusDevices = JSON.parse(fileContent, jsonDateReviver) as GeniusDevices;
					if (!importedGeniusDevices || !Array.isArray(importedGeniusDevices.devices)) {
						notifications.error('Invalid smoke detectors format.', 3000);
					} else if ((importedGeniusDevices.version ?? 0) !== CURRENT_VERSION) {
						modals.open(InfoDialog, {
							title: 'Incompatible File Version',
							message: `The file has version <strong>v${importedGeniusDevices.version ?? 0}</strong>, but version <strong>v${CURRENT_VERSION}</strong> is required.<br><br>Please export a fresh backup from a current device before importing or migrate the file to the required version.`,
							variant: 'error',
							onDismiss: () => modals.close()
						});
					} else {
						geniusDevices.devices = importedGeniusDevices.devices;
						notifications.success('Smoke detectors imported.', 3000);
						await postGeniusDevices();
					}
				} catch (error) {
					console.error('Error parsing file:', error);
					notifications.error('Error parsing file.', 3000);
				}

				// Reset files and clear input value to allow re-selection of the same file
				files = null;
				if (fileInput) {
					fileInput.value = '';
				}
			};

			reader.onerror = (ev) => {
				console.log('Error reading the file:', ev);
				notifications.error('Error reading file.', 3000);
				// Reset files and clear input value on error to allow re-selection
				files = null;
				if (fileInput) {
					fileInput.value = '';
				}
			};

			reader.readAsText(files[0]);
		}
	});
</script>

{#if $user.admin}
	<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
		<SettingsCard collapsible={false} maxwidth="max-w-3xl">
			{#snippet icon()}
				<SmokeDetector class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
			{/snippet}
			{#snippet title()}
				<span>Installed Genius Devices</span>
			{/snippet}
			<div class="relative w-full overflow-visible">
				<div class="flex flex-row absolute right-0 -top-13 gap-2 justify-end">
					<div class="tooltip tooltip-left" data-tip="Add smoke detector">
						<button
							class="btn btn-primary text-primary-content btn-md"
							onclick={handleNewGeniusDevice}
						>
							<Add class="h-6 w-6" />
						</button>
					</div>
					<div class="tooltip tooltip-left" data-tip={isSecureContext ? 'Add smoke detector via acoustic detection' : 'Acoustic device detection requires a secure (HTTPS) connection'}>
						<button
							class={isSecureContext ? 'btn btn-primary text-primary-content btn-md' : 'btn btn-warning text-warning-content btn-md'}
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
						<label for="upload" class="btn btn-primary text-primary-content btn-md">
							<Load class="h-6 w-6" />
						</label>
						<input bind:files bind:this={fileInput} id="upload" type="file" class="hidden" />
					</div>
					<div class="tooltip tooltip-left" data-tip="Save smoke detector configuration to file">
						<button
							class="btn btn-primary text-primary-content btn-md"
							onclick={() => downloadObjectAsJson({ version: 1, devices: geniusDevices.devices }, 'genius-smoke-detectors')}
						>
							<Save class="h-6 w-6" />
						</button>
					</div>
				</div>

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
							class="grid grid-cols-[30px_1fr_1fr_1fr_65px_50px_120px] gap-2 bg-base-200 px-4 py-2 rounded-t-lg font-bold text-sm"
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
								{@const sdModelName = getSmokeDetectorModelName(device.smokeDetector.model)}
								{@const sdFaults = getSmokeDetectorFaults(device.smokeDetector)}
								{@const sdReadout = hasReadout(device)}
								{@const stale = isStaleReadout(device)}
								{@const rmPresent = hasRadioModule(device.radioModule)}
								{@const rmModelName = getRadioModuleModelName(device.radioModule.model)}
								{@const rmFaults = getRadioModuleFaults(device.radioModule)}
								<div
									class="rounded-box bg-base-100 grid grid-cols-[30px_1fr_1fr_1fr_65px_50px_120px] gap-2 px-4 py-2 items-center"
								>
									<!-- Drag handle -->
									<div class="flex items-center justify-center" use:dragHandle>
										<Grip class="h-6 w-6 text-base-content/30 cursor-grab" />
									</div>

									<!-- Location -->
									<div
										class="text-sm font-bold {device.location === 'Unknown location'
											? 'italic text-base-content/70'
											: ''} truncate"
									>
										{device.location}
									</div>

									<!-- Smoke Detector -->
									<div class="text-sm min-w-0">
										<div class="truncate">
											{#if sdModelName}
												<span class="font-medium">{sdModelName}</span>
											{:else}
												<span class="italic text-base-content/50">Unknown model</span>
											{/if}
										</div>
										{#if !sdReadout}
											<div class="text-base-content/40 italic truncate">Status not available</div>
										{:else if sdFaults.length === 0}
											<div class="tooltip tooltip-top" data-tip="Status OK">
												<button
													class="flex items-center gap-1 cursor-pointer hover:opacity-80"
													onclick={() => openDeviceDetails(index)}
												>
													<StatusOk class="h-5 w-5 {stale ? 'text-base-content/40' : 'text-success'}" />
													<span class={stale ? 'text-base-content/40' : 'text-success'}>OK</span>
												</button>
											</div>
										{:else}
											<div class="tooltip tooltip-top" data-tip={sdFaults.join(', ')}>
												<button
													class="flex items-center gap-1 cursor-pointer hover:opacity-80"
													onclick={() => openDeviceDetails(index)}
												>
													<StatusFault class="h-5 w-5 text-error" />
													<span class="text-error">Fault</span>
												</button>
											</div>
										{/if}
									</div>

									<!-- Radio Module -->
									<div class="text-sm min-w-0">
										{#if !rmPresent}
											<div class="flex items-center text-base-content/40 italic">
												<AntennaOff class="flex-shrink-0 mr-1 h-4 w-4" />
												<span class="truncate">No radio module</span>
											</div>
										{:else}
											<div class="truncate">
												{#if rmModelName}
													<span class="font-medium">{rmModelName}</span>
												{:else}
													<span class="italic text-base-content/50">Unknown model</span>
												{/if}
											</div>
											{#if !sdReadout}
												<div class="text-base-content/40 italic truncate">Status not available</div>
											{:else if rmFaults.length === 0}
												<div class="tooltip tooltip-top" data-tip="Status OK">
													<button
														class="flex items-center gap-1 cursor-pointer hover:opacity-80"
														onclick={() => openDeviceDetails(index)}
													>
														<StatusOk class="h-5 w-5 {stale ? 'text-base-content/40' : 'text-success'}" />
														<span class={stale ? 'text-base-content/40' : 'text-success'}>OK</span>
													</button>
												</div>
											{:else}
												<div class="tooltip tooltip-top" data-tip={rmFaults.join(', ')}>
													<button
														class="flex items-center gap-1 cursor-pointer hover:opacity-80"
														onclick={() => openDeviceDetails(index)}
													>
														<StatusFault class="h-5 w-5 text-error" />
														<span class="text-error">Fault</span>
													</button>
												</div>
											{/if}
										{/if}
									</div>

									<!-- Alarms -->
									<div class="text-center text-sm">
										<div>{device.alarms.length}</div>
										{#if device.alarms.length > 0}
											<div>
												{device.alarms[device.alarms.length - 1].startTime.toLocaleDateString(
													'de-DE',
													{ day: '2-digit', month: '2-digit', year: 'numeric' }
												)}
											</div>
										{/if}
									</div>

									<!-- Service: readout age -->
									<div class="flex items-center justify-center">
										{#if sdReadout && !stale}
											<div class="tooltip tooltip-top" data-tip="Acoustic readout up to date">
												<Award class="h-6 w-6 text-success" />
											</div>
										{:else if sdReadout && stale}
											<div class="tooltip tooltip-top" data-tip="Last acoustic readout is more than 1 year ago">
												<CalendarExclamation class="h-6 w-6 text-error" />
											</div>
										{:else}
											<div class="tooltip tooltip-top" data-tip="No acoustic readout performed yet">
												<MicrophoneOff class="h-6 w-6 text-error" />
											</div>
										{/if}
									</div>

									<!-- Manage buttons -->
									<div class="flex items-center justify-end">
										<span class="inline-flex flex-row">
											{#if device.alarms.length > 0}
												<div class="tooltip tooltip-left" data-tip="Show alarm log">
													<button
														class="btn btn-ghost btn-circle btn-sm"
														onclick={() => {
															handleAlarmLog(index);
														}}
													>
														<Logs class="h-6 w-6" />
													</button>
												</div>
											{/if}
											<div class="tooltip tooltip-left" data-tip="Edit smoke detector">
												<button
													class="btn btn-ghost btn-circle btn-sm"
													onclick={() => {
														handleEdit(index);
													}}
												>
													<Edit class="h-6 w-6" />
												</button>
											</div>
											<div class="tooltip tooltip-left" data-tip="Device details">
												<button
													class="btn btn-ghost btn-circle btn-sm"
													onclick={() => openDeviceDetails(index)}
												>
													<ListDetails class="h-6 w-6" />
												</button>
											</div>
											<div class="tooltip tooltip-left" data-tip="Delete smoke detector">
												<button
													class="btn btn-ghost btn-circle btn-sm"
													onclick={() => {
														confirmDelete(index);
													}}
												>
													<Delete class="text-error h-6 w-6" />
												</button>
											</div>
										</span>
									</div>

								</div>
							{/snippet}
						</DraggableList>
					</div>
				{/if}
			</div>
		</SettingsCard>
	</div>
{:else}
	{goto('/')}
{/if}
