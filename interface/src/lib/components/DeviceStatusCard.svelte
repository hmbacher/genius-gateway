<script lang="ts">
	import { page } from '$app/state';
	import type { GeniusDevice } from '$lib/types/models';
	import { modals } from 'svelte-modals';
	import { user } from '$lib/stores/user';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import IconDetector from '~icons/custom-icons/smoke-detector-2xl';
	import IconNumber from '~icons/tabler/number';
	import IconFactory from '~icons/tabler/building-factory-2';
	import IconBell from '~icons/tabler/bell';
	import IconAlarmOff from '~icons/tabler/bell-off';
	import IconHeart from '~icons/tabler/heart';
	import Mute from '~icons/tabler/volume-3';
	import Cancel from '~icons/tabler/x';

	interface Props {
		detector: GeniusDevice;
	}

	let { detector }: Props = $props();

	async function postMute() {
		const response = await fetch('/rest/alarm-mute', {
			method: 'POST',
			headers: {
				Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic'
			}
		});
	}

	function handleMute() {
		modals.open(ConfirmDialog, {
			title: 'Stop alarming',
			message: 'Are you sure you want to stop alarming?',
			labels: {
				cancel: { label: 'Abort', icon: Cancel },
				confirm: { label: 'Mute', icon: Mute }
			},
			onConfirm: () => {
				modals.close();
				postMute();
			}
		});
	}
</script>

<div
	class="rounded-box shadow-primary/50 relative max-w-120 overflow-hidden shadow-lg p-5 {detector.isAlarming ? 'bg-error' : 'bg-base-200'}"
>
	<div class="flex">
		<div class="shrink-0">
			<IconDetector class="h-20 w-24 text-base-content/60" />
		</div>
		{#if detector.isAlarming}
			<div class="grow flex items-center justify-end">
				<button class="btn btn-circle btn-lg" onclick={handleMute}>
					<IconAlarmOff class="h-12 w-12 text-error" />
				</button>
			</div>
		{/if}
	</div>

	{#if !detector.isAlarming}
		<div class="absolute top-4 right-4">
			<IconHeart class="h-6 w-6 text-success" />
		</div>
	{/if}

	<div class="text-xl font-medium">
		{detector.location}
	</div>
	<div class="divider my-1"></div>
	<div class="flex text-base-content/50 gap-5">
		<span class="inline-flex">
			<IconNumber class="mr-1 h-5 w-5" />
			<span class="text-sm">{detector.smokeDetector.sn}</span>
		</span>
		<span class="inline-flex">
			<IconFactory class="mr-1 h-5 w-5" />
			<span class="text-sm"
				>{detector.smokeDetector.productionDate.toLocaleDateString('en-US', {
					month: '2-digit',
					year: '2-digit'
				})}</span
			>
		</span>
	</div>
	<div class="flex text-base-content/50 gap-5">
		<div class="inline-flex">
			<IconBell class="mr-1 h-5 w-5" />
			<span class="text-sm">
				{detector.alarms.length}
				{#if detector.alarms.length > 0}
					({detector.alarms[detector.alarms.length - 1].startTime.toLocaleDateString()})
				{/if}
			</span>
		</div>
	</div>
</div>
