<script lang="ts">
	import type { PageData } from './$types';
	import { goto } from '$app/navigation';
	import { modals } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import EditSomeDetector from './EditSmokeDetector.svelte';
	import AlarmLog from './AlarmLog.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import Delete from '~icons/tabler/trash';
	import Add from '~icons/tabler/circle-plus';
	import Edit from '~icons/tabler/pencil';
	import Logs from '~icons/tabler/logs';
	import SmokeDetector from '~icons/custom-icons/hekatron';
	//import SmokeDetector from '~icons/my-icons/test';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Number from '~icons/tabler/number';
	import Factory from '~icons/tabler/building-factory-2';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	type HekatronAlarm = {
		startTime: Date;
		endTime: Date;
		endingReason: number;
	}

	type HekatronComponent = {
		model: number;
		sn: number;
		productionDate: Date;
	};

	type HekatronDevice = {
		smokeDetector: HekatronComponent;
		radioModule: HekatronComponent;
		location: string;
		alarms: HekatronAlarm[];
	};

	type HekatronDevices = {
		devices: HekatronDevice[];
	};

	let hekatronDevices: HekatronDevices = $state();

	function jsonDateReviver(key, value) {
		// plug this regex into regex101.com to understand how it works
		// matches 2019-06-20T12:29:43.288Z (with milliseconds) and 2019-06-20T12:29:43Z (without milliseconds)
		var dateFormat = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d{1,}|)Z$/;

		if (typeof value === 'string' && dateFormat.exec(value)) {
			return new Date(value);
		}

		return value;
	}

	async function getHekatronDevices() {
		try {
			const response = await fetch('/rest/gateway-devices', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			hekatronDevices = JSON.parse(await response.text(), jsonDateReviver);
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function postHekatronDevices(data: HekatronDevices) {
		try {
			const response = await fetch('/rest/gateway-devices', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(data)
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
			hekatronDevice: { ...hekatronDevices.devices[index] }, // Shallow Copy
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
			hekatronDevice: { ...hekatronDevices.devices[index] } // Shallow Copy
		});
	}
</script>

{#if $user.admin}
	<div
		class="mx-0 my-1 flex flex-col space-y-4
     sm:mx-8 sm:my-8"
	>
		<SettingsCard collapsible={false}>
			{#snippet icon()}
				<SmokeDetector class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
			{/snippet}
			{#snippet title()}
				<span>Hekatron devices</span>
			{/snippet}
			{#await getHekatronDevices()}
				<Spinner />
			{:then nothing}
				<div class="relative w-full overflow-visible">
					<button
						class="btn btn-primary text-primary-content btn-md absolute -top-14 right-0"
						onclick={handleNewHekatronDevice}
					>
						<Add class="h-6 w-6" />
					</button>

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
												<Number class="lex-shrink-0 mr-2 h-4 w-4 self-end" />{device.smokeDetector.sn}
											</span><br/>
											<span class="inline-flex items-baseline">
												<Factory class="lex-shrink-0 mr-2 h-4 w-4 self-end" />{device.smokeDetector.productionDate.toLocaleDateString()}
											</span>
										</td>
										<td align="left"
											>SN {device.radioModule.sn}<br
											/>{device.radioModule.productionDate.toLocaleDateString()}</td
										>
										<td align="center">
											{device.alarms.length}<br />
											{#if device.alarms.length > 0}
												{device.alarms[device.alarms.length - 1].startTime.toLocaleDateString()}
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
