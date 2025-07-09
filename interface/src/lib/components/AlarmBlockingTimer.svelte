<script lang="ts">
	import { page } from '$app/state';
	import { onMount, onDestroy } from 'svelte';
	import { socket } from '$lib/stores/socket';
	import { notifications } from './toasts/notifications';
	import { modals } from 'svelte-modals';
	import { user } from '$lib/stores/user';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import IconAlertingOff from '~icons/tabler/bell-off';
	import IconYes from '~icons/tabler/circle-check';
	import IconNo from '~icons/tabler/x';

	type AlarmBlockingState = {
		isBlocked: boolean;
		remainingBlockingTimeS: number;
	};

	const defaultAlarmBlockingState: AlarmBlockingState = {
		isBlocked: false,
		remainingBlockingTimeS: 0
	};

	let alarmBlockingState: AlarmBlockingState = $state(defaultAlarmBlockingState);

	onMount(() => {
		socket.on<AlarmBlockingState>('rem-alarm-block-time', handleRemainingBlockingTimeEvent);
	});

	onDestroy(() => {
		socket.off('rem-alarm-block-time', handleRemainingBlockingTimeEvent);
	});

	const handleRemainingBlockingTimeEvent = (abState: AlarmBlockingState) => {
		if (abState) {
			alarmBlockingState = abState;
		}
	};

	function confirmAlarmBlockingEnd() {
		modals.open(ConfirmDialog, {
			title: 'End alarm blocking',
			message: 'Are you sure you want to end alarm blocking?',
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
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			const jsonData = await response.json();
			if (jsonData || jsonData.success === true) {
				alarmBlockingState = defaultAlarmBlockingState; // Reset the state
				notifications.success('Alarm blocking ended successfully.', 3000);
			} else {
				notifications.error('Failed to end alarm blocking.', 3000);
			}
		} catch (error) {
			console.error('Error ending alarm blocking:', error);
			notifications.error('Failed to end alarm blocking.', 3000);
		}
	}
</script>

{#if alarmBlockingState.isBlocked}
	<div class="tooltip tooltip-left" data-tip="End alarm blocking now.">
		<button class="btn rounded-xl btn-error h-9" onclick={confirmAlarmBlockingEnd}>
			<div class="inline-flex items-center">
				<IconAlertingOff class="h-5 w-5" />
				<span class="ml-2 font-bold text-lg">
					{String(Math.floor(alarmBlockingState.remainingBlockingTimeS / 60)).padStart(
						2,
						'0'
					)}:{String(Math.floor(alarmBlockingState.remainingBlockingTimeS % 60)).padStart(2, '0')}
				</span>
			</div>
		</button>
	</div>
{/if}
