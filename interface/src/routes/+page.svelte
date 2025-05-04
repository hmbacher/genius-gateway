<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { user } from '$lib/stores/user';
	import type { PageData } from './$types';
	import { socket } from '$lib/stores/socket';
	import type { AlarmState } from '$lib/types/models';
	import type { HekatronDevices } from '$lib/types/models';
	import { jsonDateReviver } from '$lib/utils';
	import DeviceStatusCard from '$lib/components/DeviceStatusCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import Info from '~icons/tabler/info-circle';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	let hekatronDevices: HekatronDevices = $state({ devices: [] });

	let alarmState: AlarmState = $state({ alarmingDevices: [] });

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

	async function getAlarmState() {
		try {
			const response = await fetch('/rest/alarm', {
				method: 'GET',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			alarmState = await response.json();

		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function getData() {
		await getHekatronDevices();
		await getAlarmState();

		return;
	}

	onMount(() => {
		socket.on<AlarmState>('alarm', (data) => {
			alarmState = data;
		});
	});

	onDestroy(() => socket.off('alarm'));

</script>

<div >
	<div class="flex flex-wrap gap-10 px-10 py-10" style="justify-content: center;">
		{#await getData()}
				<Spinner />
		{:then nothing}
			{#if hekatronDevices.devices.length > 0}
				{#each hekatronDevices.devices as device}
					<DeviceStatusCard detector={device} alerting={alarmState.alarmingDevices.includes(device.smokeDetector.sn)} />
				{/each}
			{:else}
			<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
				<div class="alert">
					<Info class="h-6 w-6 shrink-0 stroke-current" />
					<div>
						<p>No Genius devices set up yet.</p>
						<p>Wanna <a class="link link-primary" href="/gateway/smoke-detectors">add some smoke detectors</a> now?</p>
					</div>
				</div>
			</div>
			{/if}
		{/await}
	</div>
</div>
