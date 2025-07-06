<script lang="ts">
	import type { PageData } from './$types';
	import { goto } from '$app/navigation';
	import { modals } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { GeniusDevices, GeniusDevice } from '$lib/types/models';
	import { jsonDateReviver, downloadObjectAsJson } from '$lib/utils/misc';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import EditSmokeDetector from './EditSmokeDetector.svelte';
	import AlarmLog from './AlarmLog.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import Delete from '~icons/tabler/trash';
	import Add from '~icons/tabler/circle-plus';
	import Edit from '~icons/tabler/pencil';
	import Logs from '~icons/tabler/logs';
	import SmokeDetector from '~icons/custom-icons/smoke-detector-m';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Number from '~icons/tabler/number';
	import Factory from '~icons/tabler/building-factory-2';
	import Save from '~icons/tabler/device-floppy';
	import Load from '~icons/tabler/folder-open';
	import Manual from '~icons/tabler/forms';
	import Automatic from '~icons/tabler/access-point';

	interface Props {
		data: PageData;
	}

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

	let files: any = $state();

	$effect(() => {
		if (files) {
			// Note that `files` is of type `FileList`, not an Array:
			// https://developer.mozilla.org/en-US/docs/Web/API/FileList
			const reader = new FileReader();
			reader.onload = async () => {
				const fileContent = reader.result as string;
				try {
					const importedGeniusDevices = JSON.parse(fileContent, jsonDateReviver) as GeniusDevices;
					if (importedGeniusDevices) {
						geniusDevices.devices = importedGeniusDevices.devices;
						notifications.success('Smoke detectors imported.', 3000);
						await postGeniusDevices();
					} else {
						notifications.error('Invalid smoke detectors format.', 3000);
						files = null; // Reset files to allow re-selection
					}
				} catch (error) {
					console.error('Error parsing file:', error);
					notifications.error('Error parsing file.', 3000);
				}

				// Reset files after processing to allow re-selection of the same file
				files = null;
			};

			reader.onerror = (ev) => {
				console.log('Error reading the file:', ev);
				notifications.error('Error reading file.', 3000);
				// Reset files on error to allow re-selection
				files = null;
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
				<span>Installed Genius devices</span>
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
					<div class="tooltip tooltip-left" data-tip="Load smoke detector configuration from file">
						<label for="upload" class="btn btn-primary text-primary-content btn-md">
							<Load class="h-6 w-6" />
						</label>
						<input bind:files id="upload" type="file" class="hidden" />
					</div>
					<div class="tooltip tooltip-left" data-tip="Save smoke detector configuration to file">
						<button
							class="btn btn-primary text-primary-content btn-md"
							onclick={() => downloadObjectAsJson(geniusDevices, 'genius-devices')}
						>
							<Save class="h-6 w-6" />
						</button>
					</div>
				</div>

				{#if geniusDevices.devices.length === 0}
					<div class="divider my-0"></div>
					<div class="flex flex-col items-center justify-center p-4 text-sm text-gray-500">
						<p class="mb-4 font-semibold">No smoke detectors configured yet.</p>
						<p class="mx-20 text-center">Click the "+" button to add your first smoke detector.</p>
					</div>
				{:else}
					<div class="overflow-x-auto" transition:slide|local={{ duration: 300, easing: cubicOut }}>
						<table class="table w-full table-auto">
							<thead>
								<tr class="font-bold">
									<th align="left">Location</th>
									<th align="left">Smoke Detector</th>
									<th align="left">Radio Module</th>
									<th align="center">Alarms</th>
									<th align="center">Registration</th>
									<th align="right" class="pr-8">Manage</th>
								</tr>
							</thead>
							<tbody>
								{#each geniusDevices.devices as device, index}
									<tr>
										<td
											align="left"
											class="font-bold {device.location === 'Unknown location'
												? 'italic text-base-content/70'
												: ''}">{device.location}</td
										>
										<td align="left">
											<span class="inline-flex items-baseline">
												<Number class="lex-shrink-0 mr-2 h-4 w-4" />{device.smokeDetector.sn}
											</span><br />
											<span class="inline-flex items-baseline">
												<Factory class="lex-shrink-0 mr-2 h-4 w-4" />
												{#if !device.smokeDetector.productionDate}
													<span class="italic text-base-content/70">Unknown</span>
												{:else}
													{device.smokeDetector.productionDate.toLocaleDateString('de-DE', {
														day: '2-digit',
														month: '2-digit',
														year: 'numeric'
													})}
												{/if}
											</span>
										</td>
										<td align="left">
											<span class="inline-flex items-baseline">
												<Number class="lex-shrink-0 mr-2 h-4 w-4" />{device.radioModule.sn}
											</span><br />
											<span class="inline-flex items-baseline">
												<Factory class="lex-shrink-0 mr-2 h-4 w-4" />
												{#if !device.radioModule.productionDate}
													<span class="italic text-base-content/70">Unknown</span>
												{:else}
													{device.radioModule.productionDate.toLocaleDateString('de-DE', {
														day: '2-digit',
														month: '2-digit',
														year: 'numeric'
													})}
												{/if}
											</span>
										</td>
										<td align="center">
											{device.alarms.length}<br />
											{#if device.alarms.length > 0}
												{device.alarms[device.alarms.length - 1].startTime.toLocaleDateString(
													'de-DE',
													{ day: '2-digit', month: '2-digit', year: 'numeric' }
												)}
											{/if}
										</td>
										<td align="center">
											{#if device.registration === 2}
												<div class="tooltip tooltip-top" data-tip="Manually added Genius device">
													<Manual class="h-6 w-6" />
												</div>
											{:else if device.registration === 1}
												<div
													class="tooltip tooltip-top"
													data-tip="Genius device added from received alert packet"
												>
													<Automatic class="h-6 w-6" />
												</div>
											{:else}
												<span class="italic text-base-content/70">Unknown</span>
											{/if}
										</td>

										<td align="right">
											<span class="my-auto inline-flex flex-row space-x-2">
												{#if device.alarms.length > 0}
													<button
														class="btn btn-ghost btn-circle btn-xs"
														onclick={() => handleAlarmLog(index)}
													>
														<Logs class="h-6 w-6" />
													</button>
												{/if}
												<button
													class="btn btn-ghost btn-circle btn-xs"
													onclick={() => handleEdit(index)}
												>
													<Edit class="h-6 w-6" />
												</button>
												<button
													class="btn btn-ghost btn-circle btn-xs"
													onclick={() => confirmDelete(index)}
												>
													<Delete class="text-error h-6 w-6" />
												</button>
											</span>
										</td>
									</tr>
								{/each}
							</tbody>
						</table>
					</div>
				{/if}
			</div>
		</SettingsCard>
	</div>
{:else}
	{goto('/')}
{/if}
