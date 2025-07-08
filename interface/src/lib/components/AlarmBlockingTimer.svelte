<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import { socket } from '$lib/stores/socket';
	import { notifications } from './toasts/notifications';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import IconAlertingOff from '~icons/tabler/bell-off';
	import IconYes from '~icons/tabler/circle-check';
	import IconNo from '~icons/tabler/x';

	type AlarmBlockingState = {
		isBlocked: boolean;
		remainingBlockingTimeS: number;
	};

	let alarmBlockingState: AlarmBlockingState = $state({
		isBlocked: true,
		remainingBlockingTimeS: 1234
	});

	onMount(() => {
		socket.on<AlarmBlockingState>('alarm', handleRemainingBlockingTimeEvent);
	});

	const handleRemainingBlockingTimeEvent = (abState: AlarmBlockingState) => {
		if (abState) {
			alarmBlockingState = abState;
		}
	};

	function confirmAlarmBlockingEnd() {
		modals.open(ConfirmDialog, {
			title: 'Confirm Power Down',
			message: 'Are you sure you want to switch off the device?',
			labels: {
				cancel: { label: 'No', icon: IconNo },
				confirm: { label: 'Yes', icon: IconYes }
			},
			onConfirm: () => {
				modals.close();
				postAlarmBlockingEnd();
			}
		});
	}

	async function postAlarmBlockingEnd() {
		try {
			const response = await fetch('/rest/end-alarmblocking', {
				method: 'POST',
				headers: {
					'Content-Type': 'application/json'
				}
			});

			if (response.ok) {
				notifications.success('Alarm blocking ended successfully.', 3000);
			} else {
				notifications.error('Failed to end alarm blocking.', 3000);
			}
		} catch (error) {
			console.error('Error ending alarm blocking:', error);
		}
	}
</script>

{#if alarmBlockingState.isBlocked}
	<div
		class="tooltip tooltip-left"
		data-tip="End alarm blocking now."
	>
		<button class="btn rounded-xl btn-error h-9" onclick={confirmAlarmBlockingEnd}>
			<div class="inline-flex items-center">
				<IconAlertingOff class="h-5 w-5" />
				<span class="ml-2 font-bold text-lg">
					{String(Math.floor(alarmBlockingState.remainingBlockingTimeS / 60)).padStart(2, '0')}:{String(Math.floor(alarmBlockingState.remainingBlockingTimeS % 60)).padStart(2, '0')}
				</span>
			</div>
		</button>
	</div>
{/if}
