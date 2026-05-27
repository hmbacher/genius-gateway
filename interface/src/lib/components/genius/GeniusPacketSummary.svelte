<script lang="ts">
	import type {
		Packet,
		CommissioningInfo,
		DiscoveryResponseInfo,
		AlarmStartInfo,
		AlarmStopInfo
	} from '$lib/types/models';
	import { PacketTypeNames } from '$lib/types/models';
	import IconWifi from '~icons/tabler/wifi';
	import IconHops from '~icons/tabler/arrow-forward-up';
	import IconClock from '~icons/tabler/clock';
	import IconRing from '~icons/tabler/topology-ring-2';
	import IconAlarmStart from '~icons/tabler/bell';
	import IconAlarmStop from '~icons/tabler/bell-off';
	import IconDetector from '~icons/tabler/alarm-smoke';

	interface Props {
		packet: Packet;
	}

	let { packet }: Props = $props();

	let typeName = $derived(packet.type?.name);

	function chipClass(value: string | undefined, base: string): string {
		return value === 'Unknown' ? `summary-chip ${base}-unknown` : `summary-chip ${base}`;
	}
</script>

{#if packet.generalInfo}
	<div class="summary-row">
		{#if typeName === PacketTypeNames.StartAlarm && packet.specificInfo}
			<span class="summary-chip chip-alarm-start">
				<IconAlarmStart class="h-4 w-4 shrink-0" />
				<IconDetector class="h-4 w-4 shrink-0" />
				<span class="truncate">{(packet.specificInfo as AlarmStartInfo).startingLocation}</span>
			</span>
		{:else if typeName === PacketTypeNames.StopAlarm && packet.specificInfo}
			<span class="summary-chip chip-alarm-stop">
				<IconAlarmStop class="h-4 w-4 shrink-0" />
				<IconDetector class="h-4 w-4 shrink-0" />
				<span class="truncate">{(packet.specificInfo as AlarmStopInfo).silencingLocation}</span>
			</span>
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
		{:else if typeName === PacketTypeNames.DiscoveryResponse && packet.specificInfo}
			<span
				class={chipClass(
					(packet.specificInfo as DiscoveryResponseInfo).requestingLocation,
					'chip-radio'
				)}
			>
				<span class="chip-prefix">to</span>
				<IconWifi class="h-4 w-4 shrink-0" />
				<span class="truncate">{(packet.specificInfo as DiscoveryResponseInfo).requestingLocation}</span>
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
</style>
