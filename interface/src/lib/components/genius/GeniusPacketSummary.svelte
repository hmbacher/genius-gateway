<script lang="ts">
	import type {
		Packet,
		CommissioningInfo,
		CommissioningProbeResponseInfo,
		StatusChangeInfo
	} from '$lib/types/models';
	import { PacketTypeNames, StatusChangePacketTypeNames } from '$lib/types/models';
	import { entityStyleSuffix } from '$lib/genius/entity';
	import { STATUS_ALARM, statusFlagLabels } from '$lib/genius/status';
	import IconWifi from '~icons/tabler/wifi';
	import IconHops from '~icons/tabler/arrow-forward-up';
	import IconClock from '~icons/tabler/clock';
	import IconRing from '~icons/tabler/topology-ring-2';
	import IconAlarmStart from '~icons/tabler/bell';
	import IconAlarmStop from '~icons/tabler/bell-off';
	import IconDetector from '~icons/tabler/alarm-smoke';
	import IconStatusSet from '~icons/tabler/alert-triangle';
	import IconStatusCleared from '~icons/tabler/circle-check';

	interface Props {
		packet: Packet;
	}

	let { packet }: Props = $props();

	let typeName = $derived(packet.type?.name);

	// Status change: the alarm bit is shown by the chip type, every other flag as its own chip
	let statusInfo = $derived(
		typeName && StatusChangePacketTypeNames.includes(typeName)
			? (packet.specificInfo as StatusChangeInfo | null)
			: null
	);
	let setLabels = $derived(statusInfo ? statusFlagLabels(statusInfo.setMask & ~STATUS_ALARM) : []);
	let clearedLabels = $derived(
		statusInfo ? statusFlagLabels(statusInfo.clearedMask & ~STATUS_ALARM) : []
	);

	function chipClass(value: string | undefined, base: string): string {
		return `summary-chip ${base}${entityStyleSuffix(value)}`;
	}
</script>

{#if packet.generalInfo}
	<div class="summary-row">
		{#if statusInfo}
			{#if typeName === PacketTypeNames.StartAlarm}
				<span class="summary-chip chip-alarm-start">
					<IconAlarmStart class="h-4 w-4 shrink-0" />
					<IconDetector class="h-4 w-4 shrink-0" />
					<span class="truncate">{statusInfo.location}</span>
				</span>
			{:else if typeName === PacketTypeNames.StopAlarm}
				<span class="summary-chip chip-alarm-stop">
					<IconAlarmStop class="h-4 w-4 shrink-0" />
					<IconDetector class="h-4 w-4 shrink-0" />
					<span class="truncate">{statusInfo.location}</span>
				</span>
			{:else}
				<span class="summary-chip chip-status">
					<IconDetector class="h-4 w-4 shrink-0" />
					<span class="truncate">{statusInfo.location}</span>
				</span>
			{/if}
			{#each setLabels as label}
				<span class="summary-chip chip-status-set">
					<IconStatusSet class="h-4 w-4 shrink-0" />
					<span class="truncate">{label}</span>
				</span>
			{/each}
			{#each clearedLabels as label}
				<span class="summary-chip chip-status-cleared">
					<IconStatusCleared class="h-4 w-4 shrink-0" />
					<span class="truncate">{label}</span>
				</span>
			{/each}
		{/if}

		<span class={chipClass(packet.generalInfo.firstLocation, 'chip-radio')}>
			<IconWifi class="h-4 w-4 shrink-0" />
			<span class="truncate">{packet.generalInfo.firstLocation}</span>
		</span>
		<span class="summary-arrow">→</span>
		<span class={chipClass(packet.generalInfo.secondLocation, 'chip-radio')}>
			<IconWifi class="h-4 w-4 shrink-0" />
			<span class="truncate">{packet.generalInfo.secondLocation}</span>
		</span>
		<span class={chipClass(packet.generalInfo.lineName, 'chip-line')}>
			<IconRing class="h-4 w-4 shrink-0" />
			<span class="truncate">{packet.generalInfo.lineName}</span>
		</span>
		<span class="summary-chip chip-hops">
			<IconHops class="h-4 w-4 shrink-0" />
			<span>{packet.generalInfo.hops}</span>
		</span>

		{#if typeName === PacketTypeNames.Comissioning && packet.specificInfo}
			<span class="summary-chip chip-time">
				<IconClock class="h-4 w-4 shrink-0" />
				<span>{(packet.specificInfo as CommissioningInfo).timeStr}</span>
			</span>
		{:else if typeName === PacketTypeNames.CommissioningProbeResponse && packet.specificInfo}
			<span
				class={chipClass(
					(packet.specificInfo as CommissioningProbeResponseInfo).requestingLocation,
					'chip-radio'
				)}
			>
				<span class="chip-prefix">to</span>
				<IconWifi class="h-4 w-4 shrink-0" />
				<span class="truncate">{(packet.specificInfo as CommissioningProbeResponseInfo).requestingLocation}</span>
			</span>
		{/if}
	</div>
{/if}

<style>
	@reference "$src/app.css";

	div.summary-row {
		@apply flex flex-wrap items-center gap-x-2 gap-y-1 text-sm py-1;
	}

	span.summary-chip {
		@apply inline-flex items-center gap-1 px-2 py-[2px] rounded-box font-light max-w-full text-slate-900 dark:text-slate-100;
	}

	span.summary-arrow {
		@apply opacity-40 select-none;
	}

	span.chip-prefix {
		@apply text-slate-500 dark:text-slate-400 font-normal select-none;
	}

	span.chip-radio {
		@apply bg-cyan-200 dark:bg-cyan-900;
	}

	span.chip-radio-unknown {
		@apply bg-red-200 dark:bg-red-900;
	}

	span.chip-radio-gateway {
		@apply bg-cyan-600 text-white dark:text-white;
	}

	span.chip-line {
		@apply bg-amber-200 dark:bg-amber-900;
	}

	span.chip-line-unknown {
		@apply bg-red-200 dark:bg-red-900;
	}

	span.chip-hops {
		@apply bg-purple-200 dark:bg-purple-900;
	}

	span.chip-time {
		@apply bg-emerald-200 dark:bg-emerald-900;
	}

	span.chip-alarm-start {
		@apply bg-pink-400 dark:bg-pink-800;
	}

	span.chip-alarm-stop {
		@apply bg-green-400 dark:bg-green-800;
	}

	span.chip-status {
		@apply bg-orange-200 dark:bg-orange-900;
	}

	span.chip-status-set {
		@apply bg-orange-400 dark:bg-orange-800;
	}

	span.chip-status-cleared {
		@apply bg-teal-300 dark:bg-teal-800;
	}
</style>
