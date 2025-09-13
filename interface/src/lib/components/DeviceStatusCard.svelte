<script lang="ts">
	import type { GeniusDevice } from '$lib/types/models';
	import IconDetector from '~icons/custom-icons/smoke-detector-2xl';
	import IconNumber from '~icons/tabler/number';
	import IconFactory from '~icons/tabler/building-factory-2';
	import IconBell from '~icons/tabler/bell';
	import IconHeart from '~icons/tabler/heart';
	import IconFire from '~icons/tabler/flame-filled';

	interface Props {
		detector: GeniusDevice;
	}

	let { detector }: Props = $props();
	let isUnknownLocation = detector.location === 'Unknown location';
</script>

<div
	class="rounded-box shadow-primary/50 relative max-w-120 overflow-hidden shadow-lg p-5 {detector.isAlarming
		? 'bg-error'
		: isUnknownLocation
			? 'bg-warning'
			: 'bg-base-200'}"
>
	<div class="flex">
		<div class="shrink-0">
			<IconDetector class="h-20 w-24 text-base-content/60" />
		</div>
	</div>

	<div class="absolute top-4 right-4">
		{#if !detector.isAlarming}
			<IconHeart class="h-6 w-6 text-success" />
		{:else}
			<IconFire class="h-6 w-6" />
		{/if}
	</div>

	<div class="text-xl font-medium {isUnknownLocation ? 'text-warning-content/30 italic' : ''}">
		{detector.location}
	</div>
	<div class="divider my-1"></div>
	<div class="flex text-base-content/50 gap-5">
		<span class="inline-flex">
			<IconNumber class="mr-1 h-5 w-5" />
			<span class="text-sm">{detector.smokeDetector.sn}</span>
		</span>
		<span class="inline-flex">
			{#if detector.smokeDetector.productionDate}
				<IconFactory class="mr-1 h-5 w-5" />
				<span class="text-sm"
					>{detector.smokeDetector.productionDate.toLocaleDateString('en-US', {
						month: '2-digit',
						year: '2-digit'
					})}</span
				>
			{/if}
		</span>
	</div>
	<div class="flex text-base-content/50 gap-5">
		<div class="inline-flex">
			<IconBell class="mr-1 h-5 w-5" />
			<span class="text-sm">
				{detector.alarms.length}
				{#if detector.alarms.length > 0}
					({detector.alarms[detector.alarms.length - 1].startTime.toLocaleDateString('de-DE', {
						day: '2-digit',
						month: '2-digit',
						year: 'numeric'
					})})
				{/if}
			</span>
		</div>
	</div>
</div>
