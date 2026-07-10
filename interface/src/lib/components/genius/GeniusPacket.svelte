<script lang="ts">
	import type { Packet } from '$lib/types/models';
	import { PacketTypeNames } from '$lib/types/models';
	import GeniusPacketDataBlock from '$lib/components/genius/GeniusPacketDataBlock.svelte';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import GeniusPacketSummary from './GeniusPacketSummary.svelte';
	import GeniusPacketContentComissioning from './GeniusPacketContentComissioning.svelte';
	import GeniusPacketContentLineTestStart from './GeniusPacketContentLineTestStart.svelte';
	import GeniusPacketContentLineTestStop from './GeniusPacketContentLineTestStop.svelte';
	import GeniusPacketContentCommissioningProbeRequest from './GeniusPacketContentCommissioningProbeRequest.svelte';
	import GeniusPacketContentCommissioningProbeResponse from './GeniusPacketContentCommissioningProbeResponse.svelte';
	import GeniusPacketContentAlarmStart from './GeniusPacketContentAlarmStart.svelte';
	import GeniusPacketContentAlarmStop from './GeniusPacketContentAlarmStop.svelte';
	import GeniusPacketContentConfigCheckProbeResponse from './GeniusPacketContentConfigCheckProbeResponse.svelte';
	import IconChevronDown from '~icons/tabler/chevron-down';

	interface Props {
		packet: Packet;
		showMeta?: boolean;
		showDetails?: boolean;
		expanded?: boolean;
		onToggle?: () => void;
	}

	let {
		packet,
		showMeta = true,
		showDetails = true,
		expanded = false,
		onToggle = () => {}
	}: Props = $props();

	// When meta is hidden there is no chevron, so bytes are always visible (matches v1 showMeta=false).
	let bytesShown = $derived(!showMeta || expanded);
	let bytesWrapClass = $derived(bytesShown ? 'block' : 'hidden');
	let chevronClass = $derived(
		bytesShown ? 'h-4 w-4 transition-transform rotate-180' : 'h-4 w-4 transition-transform'
	);
</script>

<div class="genius-packet">
	{#if showMeta}
		<div class="meta-row-outer">
			<div class="meta-row-chips">
				<div class="meta-data">#{packet.id}</div>
				<div class="meta-data">{(packet.timestampFirst / 1000) >>> 0}</div>
				{#if packet.counter > 1}
					<div class="meta-data">{(packet.timestampLast / 1000) >>> 0}</div>
				{/if}
				<div class="meta-data">{packet.counter}×</div>
				<div class="meta-data">{packet.data.length} B</div>
				{#if showDetails}
					{#if packet.type}
						<div class="meta-data {packet.type.cssClass}">{packet.type.name}</div>
					{:else}
						<div class="meta-data type-unknown">Unknown</div>
					{/if}
				{/if}
			</div>
			<button
				type="button"
				class="byte-toggle"
				onclick={onToggle}
				aria-label={bytesShown ? 'Hide bytes' : 'Show bytes'}
				aria-expanded={bytesShown}
			>
				<IconChevronDown class={chevronClass} />
			</button>
		</div>
	{/if}

	{#if showDetails && packet.generalInfo}
		<GeniusPacketSummary {packet} />
	{/if}

	<div class="bytes-wrap {bytesWrapClass}">
		<div class="packet-data-container">
			{#if packet.type?.name === PacketTypeNames.Comissioning}
				<GeniusPacketContentComissioning {packet} {showDetails} />
			{:else if packet.type?.name === PacketTypeNames.StartLineTest}
				<GeniusPacketContentLineTestStart {packet} {showDetails} />
			{:else if packet.type?.name === PacketTypeNames.StopLineTest}
				<GeniusPacketContentLineTestStop {packet} {showDetails} />
			{:else if packet.type?.name === PacketTypeNames.CommissioningProbeRequest}
				<GeniusPacketContentCommissioningProbeRequest {packet} {showDetails} />
			{:else if packet.type?.name === PacketTypeNames.CommissioningProbeResponse}
				<GeniusPacketContentCommissioningProbeResponse {packet} {showDetails} />
			{:else if packet.type?.name === PacketTypeNames.StartAlarm}
				<GeniusPacketContentAlarmStart {packet} {showDetails} />
			{:else if packet.type?.name === PacketTypeNames.StopAlarm}
				<GeniusPacketContentAlarmStop {packet} {showDetails} />
			{:else if packet.type?.name === PacketTypeNames.ConfigCheckProbeResponse}
				<GeniusPacketContentConfigCheckProbeResponse {packet} {showDetails} />
			{:else if packet.generalInfo && packet.data.length >= 24}
				<GeniusPacketContentHeader {packet} {showDetails} />
				<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(24)} />
			{:else}
				{#each packet.data as byte}
					<GeniusPacketDataBlock {showDetails} data={new Uint8Array([byte])} />
				{/each}
			{/if}
		</div>
	</div>
</div>

<style>
	@reference "$src/app.css";

	:global(div.genius-packet) {
		@apply my-2 pb-2 border-b border-base-300/60 dark:border-base-content/20;
	}

	:global(div.genius-packet>div.meta-row-outer) {
		@apply flex items-start gap-2;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips) {
		@apply flex flex-wrap items-center gap-2 text-sm py-1 grow min-w-0;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.meta-data) {
		@apply text-center rounded-box font-light h-min py-[2px] px-2 bg-slate-300 dark:bg-slate-700 text-slate-900 dark:text-slate-100;
	}

	:global(div.genius-packet>div.meta-row-outer>button.byte-toggle) {
		@apply shrink-0 inline-flex items-center justify-center h-7 w-7 rounded-full hover:bg-base-300 transition-colors mt-[1px];
	}

	/* Packet type field (mirrors v1) */
	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-comissioning) {
		@apply bg-amber-500 dark:bg-amber-700 font-normal;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-commissioningprobe-request) {
		@apply bg-blue-300 dark:bg-blue-700 font-normal;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-commissioningprobe-response) {
		@apply bg-blue-700 font-normal text-white;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-linetest-start) {
		@apply bg-lime-500 dark:bg-lime-700 font-normal;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-linetest-stop) {
		@apply bg-lime-700 font-normal text-white;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-alarm-start) {
		@apply bg-pink-500 dark:bg-pink-700 font-normal;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-alarm-stop) {
		@apply bg-green-500 dark:bg-green-700 font-normal;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-configcheckprobe-request) {
		@apply bg-violet-400 dark:bg-violet-700 font-normal;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-configcheckprobe-response) {
		@apply bg-violet-700 font-normal text-white;
	}

	:global(div.genius-packet>div.meta-row-outer>div.meta-row-chips>div.type-unknown) {
		@apply bg-slate-800 text-white font-normal;
	}

	/* Visible byte view gets its own contained surface for separation from
	   the meta/summary rows above. Hidden state (.bytes-wrap.hidden) is unaffected. */
	:global(div.genius-packet>div.bytes-wrap.block) {
		@apply bg-base-300 rounded-box p-1 mt-1;
	}

	/* Packet content - wraps so a wide packet does not blow out narrow viewports. */
	:global(div.genius-packet>div.bytes-wrap>div.packet-data-container) {
		@apply flex flex-wrap items-start justify-start gap-1 text-sm p-1;
	}

	:global(div.genius-packet div.packet-data-container>div>div) {
		@apply text-center rounded-box font-light h-min py-[2px] px-1 bg-base-100;
	}

	:global(div.genius-packet div.packet-data-container>div>div.packet-data-1) {
		@apply font-medium;
	}

	/* Force dark text on every typed byte chip - the bg-X-Y palette colors are
	   static (not theme-aware) so default light text in dark mode would be unreadable.
	   Same specificity as the per-type bg rules below, but earlier in source - any
	   per-type rule that explicitly sets text-white overrides this. Untyped fillers
	   omit the data-type attribute and keep theme-aware default text on bg-base-100. */
	:global(div.genius-packet div.packet-data-container>div[data-type]>div) {
		@apply text-slate-900 dark:text-slate-100;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='remaining-tx-time']>div.packet-data-1) {
		@apply bg-stone-400 dark:bg-stone-600;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='remaining-tx-time']>div) {
		@apply bg-stone-300 dark:bg-stone-800;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='serialnumber-radiomodule']>div.packet-data-1) {
		@apply bg-cyan-400 dark:bg-cyan-700;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='serialnumber-radiomodule']>div) {
		@apply bg-cyan-200 dark:bg-cyan-900;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='serialnumber-radiomodule-unknown']>div.packet-data-1) {
		@apply bg-red-400 dark:bg-red-700;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='serialnumber-radiomodule-unknown']>div) {
		@apply bg-red-200 dark:bg-red-900;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='serialnumber-smokedetector']>div.packet-data-1) {
		@apply bg-fuchsia-400 dark:bg-fuchsia-700;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='serialnumber-smokedetector']>div) {
		@apply bg-fuchsia-200 dark:bg-fuchsia-900;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='serialnumber-smokedetector-unknown']>div.packet-data-1) {
		@apply bg-red-400 dark:bg-red-700;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='serialnumber-smokedetector-unknown']>div) {
		@apply bg-red-200 dark:bg-red-900;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line']>div.packet-data-1) {
		@apply bg-amber-400 dark:bg-amber-700;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line']>div) {
		@apply bg-amber-200 dark:bg-amber-900;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line-unknown']>div.packet-data-1) {
		@apply bg-red-400 dark:bg-red-700;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line-unknown']>div) {
		@apply bg-red-200 dark:bg-red-900;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line-unset']>div.packet-data-1) {
		@apply bg-gray-400 dark:bg-gray-600;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line-unset']>div) {
		@apply bg-gray-300 dark:bg-gray-800;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='hops']>div.packet-data-1) {
		@apply bg-purple-400 dark:bg-purple-600;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='hops']>div) {
		@apply bg-purple-300 dark:bg-purple-800;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='sequence-nr']>div.packet-data-1) {
		@apply bg-stone-400 dark:bg-stone-600;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='sequence-nr']>div) {
		@apply bg-stone-300 dark:bg-stone-800;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='alarm-start']>div.packet-data-1) {
		@apply bg-pink-600 dark:bg-pink-500;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='alarm-start']>div) {
		@apply bg-pink-400 dark:bg-pink-800;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='alarm-stop']>div.packet-data-1) {
		@apply bg-green-600 dark:bg-green-500;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='alarm-stop']>div) {
		@apply bg-green-400 dark:bg-green-800;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line-test-start']>div.packet-data-1) {
		@apply bg-lime-500 dark:bg-lime-700;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line-test-start']>div) {
		@apply bg-lime-300 dark:bg-lime-900;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line-test-stop']>div.packet-data-1) {
		@apply bg-lime-700 text-white;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='line-test-stop']>div) {
		@apply bg-lime-600 text-white;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='time']>div.packet-data-1) {
		@apply bg-emerald-400 dark:bg-emerald-600;
	}

	:global(div.genius-packet div.packet-data-container>div[data-type='time']>div) {
		@apply bg-emerald-200 dark:bg-emerald-900;
	}
</style>
