<script lang="ts">
	import type { PageData } from './$types';
	import { goto } from '$app/navigation';
	import { modals } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { HekatronDevices } from '$lib/types/models';
	import { jsonDateReviver, downloadObjectAsJson } from '$lib/utils';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import InfoDialog from '$lib/components/InfoDialog.svelte';
	import EditSomeDetector from './EditSmokeDetector.svelte';
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
	import Download from '~icons/tabler/download';
	import Upload from '~icons/tabler/upload';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	let hekatronDevices: HekatronDevices = $state({ devices: [] });

	async function getHekatronDevices() {
		try {
			const response = await fetch('/rest/gateway-devices', {
				method: 'GET',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			hekatronDevices = JSON.parse(await response.text(), jsonDateReviver);
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function postHekatronDevices(devices: HekatronDevices) {
		try {
			const response = await fetch('/rest/gateway-devices', {
				method: 'POST',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(devices)
			});

			if (response.status == 200) {
				notifications.success('Hekatron devices updated.', 3000);
				hekatronDevices = JSON.parse(await response.text(), jsonDateReviver);
			} else {
				notifications.error('User not authorized.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	function confirmDelete(index: number) {
		modals.open(ConfirmDialog, {
			title: 'Confirm to delete Hekatron device',
			message:
				'Are you sure you want to delete the Hekatron device "' +
				hekatronDevices.devices[index].smokeDetector.sn +
				' (' +
				hekatronDevices.devices[index].location +
				')"?',
			labels: {
				cancel: { label: 'Abort', icon: Cancel },
				confirm: { label: 'Yes', icon: Check }
			},
			onConfirm: () => {
				hekatronDevices.devices.splice(index, 1);
				hekatronDevices = hekatronDevices;
				modals.close();
				postHekatronDevices(hekatronDevices);
			}
		});
	}

	function handleEdit(index: number) {
		modals.open(EditSomeDetector, {
			title: 'Edit smoke detector',
			//hekatronDevice: { ...hekatronDevices.devices[index] }, // Shallow Copy
			hekatronDevice: $state.snapshot(hekatronDevices.devices[index]), // Deep copy
			onSaveHekatronDevice: (editedHekatronDevice: HekatronDevice) => {
				hekatronDevices.devices[index] = editedHekatronDevice;
				modals.close();
				postHekatronDevices(hekatronDevices);
			}
		});
	}

	function handleNewHekatronDevice() {
		modals.open(EditSomeDetector, {
			title: 'Add smoke detector',
			onSaveHekatronDevice: (newHekatronDevice: HekatronDevice) => {
				hekatronDevices.devices = [...hekatronDevices.devices, newHekatronDevice];
				modals.close();
				postHekatronDevices(hekatronDevices);
			}
		});
		//
	}

	function handleAlarmLog(index: number) {
		modals.open(AlarmLog, {
			title: 'Alarms log',
			hekatronDevice: hekatronDevices.devices[index]
		});
	}

	let files = $state();

	$effect(() => {
		if (files) {
			// Note that `files` is of type `FileList`, not an Array:
			// https://developer.mozilla.org/en-US/docs/Web/API/FileList
			console.log(files);

			const reader = new FileReader();
			reader.onload = () => {
				console.log(reader.result);
			};
			reader.onerror = () => {
				console.log('Error reading the file. Please try again.', 'error');
			};

			for (const file of files) {
				console.log(`${file.name}: ${file.size} bytes`);
				reader.readAsText(file);
			}
		}
	});
</script>

{#if $user.admin}
	<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
		<SettingsCard collapsible={false}>
			{#snippet icon()}
				<SmokeDetector class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
			{/snippet}
			{#snippet title()}
				<span>Installed Devices</span>
			{/snippet}
			{#await getHekatronDevices()}
				<Spinner />
			{:then nothing}
				<div class="relative w-full overflow-visible">
					<div
						class="tooltip tooltip-left absolute -top-14 right-32"
						data-tip="Add smoke detector"
					>
						<button
							class="btn btn-primary text-primary-content btn-md"
							onclick={handleNewHekatronDevice}
						>
							<Add class="h-6 w-6" />
						</button>
					</div>
					<div
						class="tooltip tooltip-left absolute -top-14 right-16"
						data-tip="Export smoke detector configuration"
					>
						<button
							class="btn btn-primary text-primary-content btn-md"
							onclick={() => downloadObjectAsJson(hekatronDevices, 'genius-devices')}
						>
							<Download class="h-6 w-6" />
						</button>
					</div>
					<div
						class="tooltip tooltip-left absolute -top-14 right-0"
						data-tip="Import smoke detector configuration"
					>
						<label
							for="upload"
							class="btn btn-primary text-primary-content btn-md"
						>
							<Upload class="h-6 w-6" />
						</label>
						<input bind:files id="upload" type="file" class="hidden" />
					</div>

					<div class="overflow-x-auto" transition:slide|local={{ duration: 300, easing: cubicOut }}>
						<table class="table w-full table-auto">
							<thead>
								<tr class="font-bold">
									<th align="left">Location</th>
									<th align="left">Smoke Detector</th>
									<th align="left">Radio Module</th>
									<th align="center">Alarms</th>
									<th align="right" class="pr-8">Manage</th>
								</tr>
							</thead>
							<tbody>
								{#each hekatronDevices.devices as device, index}
									<tr>
										<td align="left" class="font-bold">{device.location}</td>
										<td align="left">
											<span class="inline-flex items-baseline">
												<Number class="lex-shrink-0 mr-2 h-4 w-4" />{device.smokeDetector.sn}
											</span><br />
											<span class="inline-flex items-baseline">
												<Factory
													class="lex-shrink-0 mr-2 h-4 w-4"
												/>{device.smokeDetector.productionDate.toLocaleDateString('de-DE', {
													day: '2-digit',
													month: '2-digit',
													year: 'numeric'
												})}
											</span>
										</td>
										<td align="left">
											<span class="inline-flex items-baseline">
												<Number class="lex-shrink-0 mr-2 h-4 w-4" />{device.radioModule.sn}
											</span><br />
											<span class="inline-flex items-baseline">
												<Factory
													class="lex-shrink-0 mr-2 h-4 w-4"
												/>{device.radioModule.productionDate.toLocaleDateString('de-DE', {
													day: '2-digit',
													month: '2-digit',
													year: 'numeric'
												})}
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
				</div>
			{/await}
		</SettingsCard>
	</div>
{:else}
	{goto('/')}
{/if}
