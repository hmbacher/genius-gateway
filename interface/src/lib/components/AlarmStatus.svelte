<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { socket } from '$lib/stores/socket';
	import { notifications } from '$lib/components/toasts/notifications';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import type { AlarmState } from '$lib/types/models';
	import IconOK from '~icons/tabler/heart';
	import IconAlert from '~icons/tabler/alert-hexagon-filled';
	import Loader from '~icons/tabler/loader-2';

	let alarmState: AlarmState = $state({ alarmingDevices: [] });
	let isAlerting = $state(false);

	async function getAlarmState() {
		try {
			//console.log('AlarmStatus: getAlarmState()');
			const response = await fetch('/rest/alarm', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			alarmState = await response.json();
			processAlarmState();

		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	onMount(() => {
		//console.log('AlarmStatus: onMount()');
		socket.on<AlarmState>('alarm', (data) => {
			alarmState = data;
			processAlarmState();
		});
	});

	onDestroy(() => socket.off('alarm'));

	function processAlarmState() {
		isAlerting = alarmState.alarmingDevices.length > 0;
		if (isAlerting) {
			notifications.error('Smoke detected!', 5000);
		}
	}
</script>

<div
	class="tooltip tooltip-bottom"
	data-tip={isAlerting ? 'Smoke detected!' : 'All smoke detectors idling.'}
>
	<div class="flex-none">
		{#await getAlarmState()}
			<Loader class="text-primary/50 h-9 w-auto animate-spin stroke-1" />
		{:then nothing}
			{#if isAlerting}
				<IconAlert class="text-error h-9 w-9" />
			{:else}
				<IconOK class="text-success h-9 w-9" />
			{/if}
		{/await}
	</div>
</div>
