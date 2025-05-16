<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { socket } from '$lib/stores/socket';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { AlarmState } from '$lib/types/models';
	import IconOK from '~icons/tabler/heart';
	import IconAlert from '~icons/tabler/alert-hexagon-filled';

	let alarmState: AlarmState = $state({ isAlarming: false });

	onMount(() => {
		socket.on<AlarmState>('alarm', (data) => {
			if (alarmState.isAlarming === false && data.isAlarming === true) {
				notifications.error('Smoke detected!', 5000);
			}
			alarmState = data;
		});
	});

	onDestroy(() => socket.off('alarm'));
</script>

<div
	class="tooltip tooltip-bottom"
	data-tip={alarmState.isAlarming ? 'Smoke detected!' : 'All smoke detectors in standby.'}
>
	<div class="flex-none">
		{#if alarmState.isAlarming}
			<IconAlert class="text-error h-9 w-9" />
		{:else}
			<IconOK class="text-success h-9 w-9" />
		{/if}
	</div>
</div>
