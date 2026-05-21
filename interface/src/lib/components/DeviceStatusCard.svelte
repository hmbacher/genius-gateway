<script lang="ts">
	import type { GeniusDevice } from '$lib/types/models';
	import { GeniusDeviceRegistration } from '$lib/types/enums';
	import { hasReadout, isStaleReadout, getDeviceFaults } from '$lib/utils/deviceStatus';
	import { formatDate } from '$lib/utils/formatDate';
	import IconDetector from '~icons/custom-icons/smoke-detector-2xl';
	import IconNumber from '~icons/tabler/number';
	import IconFactory from '~icons/tabler/building-factory-2';
	import IconBell from '~icons/tabler/bell';
	import IconFire from '~icons/tabler/flame-filled';
	import IconExternal from '~icons/tabler/external-link';
	import IconFault from '~icons/tabler/alert-circle';
	import IconStale from '~icons/tabler/calendar-exclamation';
	import IconReadoutOk from '~icons/tabler/award';
	import IconNoReadout from '~icons/tabler/microphone-off';

	interface Props {
		detector: GeniusDevice;
	}

	let { detector }: Props = $props();

	let isForeign = $derived(detector.registration === GeniusDeviceRegistration.GeniusPacket);
	let readoutPresent = $derived(hasReadout(detector));
	let stale = $derived(isStaleReadout(detector));
	let faults = $derived(getDeviceFaults(detector));
	let hasFaults = $derived(faults.length > 0);
	let needsWarning = $derived(!readoutPresent || hasFaults || stale);

	let cardClass = $derived(
		detector.isAlarming
			? 'bg-error text-error-content'
			: needsWarning
				? 'bg-warning text-warning-content'
				: 'bg-primary text-primary-content'
	);
</script>

<div class="rounded-box shadow-lg relative w-full max-w-120 overflow-hidden p-5 {cardClass}">
	<div class="flex">
		<div class="shrink-0">
			<IconDetector class="h-20 w-24 text-current/60" />
		</div>
	</div>

	<!-- Status icon stack (top right): health · faults · stale · foreign -->
	<div class="absolute top-4 right-4 flex flex-col items-end gap-1.5">
		{#if detector.isAlarming}
			<IconFire class="h-6 w-6" />
		{/if}
		{#if !detector.isAlarming && !needsWarning}
			<IconReadoutOk class="h-6 w-6" />
		{/if}
		{#if !detector.isAlarming && !readoutPresent}
			<IconNoReadout class="h-6 w-6" />
		{/if}
		{#if hasFaults}
			<IconFault class="h-6 w-6" />
		{/if}
		{#if stale}
			<IconStale class="h-6 w-6" />
		{/if}
		{#if isForeign}
			<IconExternal class="h-6 w-6" />
		{/if}
	</div>

	<div class="text-xl font-medium">
		{detector.location}
	</div>
	<div class="divider my-1 before:bg-current/30 after:bg-current/30"></div>
	<div class="flex flex-wrap text-current/70 gap-x-5 gap-y-1">
		<span class="inline-flex">
			<IconNumber class="mr-1 h-5 w-5" />
			<span class="text-sm">{detector.smokeDetector.sn}</span>
		</span>
		{#if detector.smokeDetector.productionDate}
			<span class="inline-flex">
				<IconFactory class="mr-1 h-5 w-5" />
				<span class="text-sm">{formatDate(detector.smokeDetector.productionDate)}</span>
			</span>
		{/if}
		<span class="inline-flex">
			<IconBell class="mr-1 h-5 w-5" />
			<span class="text-sm">
				{detector.alarms.length}
				{#if detector.alarms.length > 0}
					({formatDate(detector.alarms[detector.alarms.length - 1].startTime)})
				{/if}
			</span>
		</span>
	</div>
</div>
