<script lang="ts">
	import type { PageData } from './$types';
	import { modals } from 'svelte-modals';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import DeviceStatusCard from '$lib/components/DeviceStatusCard.svelte';
	import AlarmEndingDialog from './AlarmEndingDialog.svelte';
	import Info from '~icons/tabler/info-circle';
	import BellOff from '~icons/tabler/bell-off';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	async function postEndAlarms(blockingTime: number) {
		try {
			const response = await fetch('/rest/end-alarms', {
				method: 'POST',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify({ alarmBlockingTime: blockingTime })	
			});

			const jsonData = await response.json();
			if (jsonData || jsonData.success === true) {
				notifications.success('Alarms ended successfully.', 3000);
			} else {
				notifications.error('Failed to end alarms.', 3000);
			}
		} catch (error) {
			console.error('Error ending alarms:', error);
			notifications.error('Failed to end alarms.', 3000);
		}
	}

	function handleEndAlarms() {
		modals.open(AlarmEndingDialog, {
			title: 'End alarms',
			onSubmit: async (time: number) => {
				await postEndAlarms(time);
				modals.close();
			}
		});
	}
</script>

<div>
	<div class="flex justify-center px-6 py-10">
		<div
			class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 xl:grid-cols-4 2xl:grid-cols-5 gap-10 justify-items-center"
		>
			{#if geniusDevices.devices.length > 0}
				{#if geniusDevices.isAlarming}
					<div class="col-span-full">
						<div class="tooltip tooltip-left" data-tip="End all alarms">
							<button class="btn btn-primary btn-wide" onclick={handleEndAlarms}>
								<BellOff class="mr-2 h-6 w-6" />
								<span>End all active alarms</span>
							</button>
						</div>
					</div>
				{/if}
				{#each geniusDevices.devices as device}
					<DeviceStatusCard detector={device} />
				{/each}
			{:else}
				<div class="col-span-full">
					<div class="alert alert-info shadow-lg">
						<Info class="h-6 w-6 shrink-0" />
						<div>
							<p>No Genius devices set up yet.</p>
							<p>
								Wanna <a class="link" href="/gateway/smoke-detectors">add some smoke detectors</a> now?
							</p>
						</div>
					</div>
				</div>
			{/if}
		</div>
	</div>
</div>
