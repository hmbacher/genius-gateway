<script lang="ts">
	import type { GeniusDevice } from '$lib/types/models';
	import { GeniusDeviceRegistration } from '$lib/types/enums';
	import {
		activeFaults,
		deviceCondition,
		deviceHints,
		hasReadout
	} from '$lib/utils/deviceStatus';
	import { statusFlagLabel } from '$lib/genius/status';
	import { statusFlagIcon } from '$lib/genius/statusIcons';
	import { formatDate } from '$lib/utils/formatDate';
	import { alarmCount, lastAlarm } from '$lib/genius/activity';
	import IconDetector from '~icons/custom-icons/smoke-detector-2xl';
	import IconNumber from '~icons/tabler/number';
	import IconFactory from '~icons/tabler/building-factory-2';
	import IconBell from '~icons/tabler/bell';
	import IconFire from '~icons/tabler/flame-filled';
	import IconExternal from '~icons/tabler/external-link';
	import IconStale from '~icons/tabler/calendar-exclamation';
	import IconReadoutOk from '~icons/tabler/award';
	import IconNoReadout from '~icons/tabler/microphone-off';
	import IconHint from '~icons/tabler/info-circle';

	interface Props {
		detector: GeniusDevice;
	}

	let { detector }: Props = $props();

	let isForeign = $derived(detector.registration === GeniusDeviceRegistration.GeniusPacket);

	// One of four conditions, by priority: alarm, fault, ok, unknown. The card shows the most
	// important one only; everything else is in the details.
	let condition = $derived(deviceCondition(detector));
	let faults = $derived(activeFaults(detector));
	let hints = $derived(deviceHints(detector));
	let newestAlarm = $derived(lastAlarm(detector));

	let cardClass = $derived(
		{
			alarm: 'bg-error text-error-content',
			fault: 'bg-warning text-warning-content',
			ok: 'bg-primary text-primary-content',
			unknown: 'bg-base-300 text-base-content'
		}[condition]
	);

	/** Start of a running alarm, short enough for the status line: the time today, else the date. */
	function alarmStart(start: Date): string {
		const today = new Date().toDateString() === start.toDateString();
		return today
			? start.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
			: formatDate(start);
	}

	// Status line: one fault is named with its date, several are listed by name. The icons show
	// every fault even when the text is cut off.
	let statusText = $derived.by(() => {
		switch (condition) {
			case 'alarm':
				return newestAlarm?.startTime
					? `Smoke detected · since ${alarmStart(newestAlarm.startTime)}`
					: 'Smoke detected';
			case 'fault':
				if (faults.length === 1) {
					const label = statusFlagLabel(faults[0].flag);
					return faults[0].since ? `${label} · since ${formatDate(faults[0].since)}` : label;
				}
				return faults.map((f) => statusFlagLabel(f.flag)).join(', ');
			case 'ok':
				return `OK · readout ${formatDate(detector.readoutTime)}`;
			default:
				return hasReadout(detector) ? 'Readout outdated' : 'No readout';
		}
	});
</script>

<div class="rounded-box shadow-lg relative w-full max-w-120 overflow-hidden p-5 {cardClass}">
	<div class="flex">
		<div class="shrink-0">
			<IconDetector class="h-20 w-24 text-current/60" />
		</div>
	</div>

	<!-- Top right: only what is no condition - the detector was registered from a received packet -->
	{#if isForeign}
		<div class="absolute top-4 right-4">
			<IconExternal class="h-6 w-6" />
		</div>
	{/if}

	<div class="text-xl font-medium">
		{detector.location}
	</div>
	<div class="divider my-1 before:bg-current/30 after:bg-current/30"></div>

	<!-- Status line (always one line): icon(s) of the condition and its text -->
	<div class="flex h-6 items-center gap-1.5 font-medium" data-condition={condition}>
		{#if condition === 'alarm'}
			<IconFire class="h-5 w-5 shrink-0" />
		{:else if condition === 'fault'}
			{#each faults as fault (fault.flag)}
				{@const Icon = statusFlagIcon(fault.flag)}
				<Icon class="h-5 w-5 shrink-0" />
			{/each}
		{:else if condition === 'ok'}
			<IconReadoutOk class="h-5 w-5 shrink-0" />
		{:else if hasReadout(detector)}
			<IconStale class="h-5 w-5 shrink-0" />
		{:else}
			<IconNoReadout class="h-5 w-5 shrink-0" />
		{/if}
		<span class="min-w-0 truncate text-sm">{statusText}</span>
		{#if hints.length > 0 && condition !== 'alarm'}
			<span class="ml-auto shrink-0" title={hints.join(', ')} data-hints={hints.length}>
				<IconHint class="h-5 w-5 opacity-70" />
			</span>
		{/if}
	</div>

	<div class="mt-1 flex flex-wrap text-current/70 gap-x-5 gap-y-1">
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
				{alarmCount(detector)}
				{#if newestAlarm?.startTime}
					({formatDate(newestAlarm.startTime)})
				{/if}
			</span>
		</span>
	</div>
</div>
