<script lang="ts">
	import type { Packet, CommissioningInfo } from '$lib/types/models';
	import { PacketTypes, PacketTypeNames } from '$lib/types/models';
	import GeniusPacketDataBlock from '$lib/components/genius/GeniusPacketDataBlock.svelte';
	import GeniusPacketContentComissioning from './GeniusPacketContentComissioning.svelte';
	import GeniusPacketContentLineTestStart from './GeniusPacketContentLineTestStart.svelte';
	import GeniusPacketContentLineTestStop from './GeniusPacketContentLineTestStop.svelte';
	import GeniusPacketContentDiscoveryRequest from './GeniusPacketContentDiscoveryRequest.svelte';
	import GeniusPacketContentDiscoveryResponse from './GeniusPacketContentDiscoveryResponse.svelte';
	import GeniusPacketContentAlarmStart from './GeniusPacketContentAlarmStart.svelte';
	import GeniusPacketContentAlarmStop from './GeniusPacketContentAlarmStop.svelte';

	interface Props {
		packet: Packet;
		showMeta?: boolean;
		showDetails?: boolean;
	}

	let { packet, showMeta = true, showDetails = true }: Props = $props();
</script>

<div class="genius-packet">
	{#if showMeta}
		<div class="meta-data-container">
			<div class="meta-data w-16">#{packet.id}</div>
			<div class="meta-data w-16">{(packet.timestampFirst / 1000) >>> 0}</div>
			{#if packet.counter > 1}
				<div class="meta-data w-16">{(packet.timestampLast / 1000) >>> 0}</div>
			{/if}
			<div class="meta-data w-10">{packet.counter}x</div>
			<div class="meta-data w-16">{packet.data.length} bytes</div>
			{#if showDetails}
				{#if packet.type}
					<div class="meta-data {packet.type.cssClass}">{packet.type.name}</div>
				{:else}
					<div class="meta-data type-unknown">Unknown</div>
				{/if}
			{/if}
		</div>
	{/if}
	<div class="packet-data-container">
		{#if packet.type?.name === PacketTypeNames.Comissioning}
			<GeniusPacketContentComissioning {packet} {showDetails} />
		{:else if packet.type?.name === PacketTypeNames.StartLineTest}
			<GeniusPacketContentLineTestStart {packet} {showDetails} />
		{:else if packet.type?.name === PacketTypeNames.StopLineTest}
			<GeniusPacketContentLineTestStop {packet} {showDetails} />
		{:else if packet.type?.name === PacketTypeNames.DiscoveryRequest}
			<GeniusPacketContentDiscoveryRequest {packet} {showDetails} />
		{:else if packet.type?.name === PacketTypeNames.DiscoveryResponse}
			<GeniusPacketContentDiscoveryResponse {packet} {showDetails} />
		{:else if packet.type?.name === PacketTypeNames.StartAlarm}
			<GeniusPacketContentAlarmStart {packet} {showDetails} />
		{:else if packet.type?.name === PacketTypeNames.StopAlarm}
			<GeniusPacketContentAlarmStop {packet} {showDetails} />
		{:else}
			{#each packet.data as byte}
				<GeniusPacketDataBlock {showDetails} data={new Uint8Array([byte])} />
			{/each}
		{/if}
	</div>
</div>

<style>
	@reference "$src/app.css";
	
	:global(div.genius-packet) {
		@apply my-1;
	}

	:global(div.genius-packet>div.meta-data-container) {
		@apply flex items-center justify-start gap-2 text-sm py-1;
	}

	:global(div.genius-packet>div.meta-data-container>div.meta-data) {
		@apply text-center rounded-box font-light h-min py-[2px] px-1 bg-slate-300;
	}

	/* Packet type field */
	:global(div.genius-packet>div.meta-data-container>div.type-comissioning) {
		@apply bg-amber-500 font-normal;
	}

	:global(div.genius-packet>div.meta-data-container>div.type-discovery-request) {
		@apply bg-blue-300 font-normal;
	}

	:global(div.genius-packet>div.meta-data-container>div.type-discovery-response) {
		@apply bg-blue-700 font-normal text-white;
	}

	:global(div.genius-packet>div.meta-data-container>div.type-linetest-start) {
		@apply bg-lime-500 font-normal;
	}

	:global(div.genius-packet>div.meta-data-container>div.type-linetest-stop) {
		@apply bg-lime-700 font-normal text-white;
	}

	:global(div.genius-packet>div.meta-data-container>div.type-alarm-start) {
		@apply bg-pink-500 font-normal;
	}

	:global(div.genius-packet>div.meta-data-container>div.type-alarm-stop) {
		@apply bg-green-500 font-normal;
	}

	:global(div.genius-packet>div.meta-data-container>div.type-unknown) {
		@apply bg-slate-800 text-white font-normal;
	}

	/* Packet content */
	:global(div.genius-packet>div.packet-data-container) {
		@apply flex items-start justify-start gap-1 text-sm py-1;
	}

	:global(div.genius-packet>div.packet-data-container>div>div) {
		@apply text-center rounded-box font-light h-min py-[2px] px-1 bg-base-100;
	}

	:global(div.genius-packet>div.packet-data-container>div>div.packet-data-1) {
		@apply font-medium;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='counter']>div.packet-data-1) {
		@apply bg-stone-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='counter']>div) {
		@apply bg-stone-300;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='serialnumber-radiomodule']>div.packet-data-1) {
		@apply bg-cyan-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='serialnumber-radiomodule']>div) {
		@apply bg-cyan-200;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='serialnumber-radiomodule-unknown']>div.packet-data-1) {
		@apply bg-red-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='serialnumber-radiomodule-unknown']>div) {
		@apply bg-red-200;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='serialnumber-smokedetector']>div.packet-data-1) {
		@apply bg-fuchsia-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='serialnumber-smokedetector']>div) {
		@apply bg-fuchsia-200;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='serialnumber-smokedetector-unknown']>div.packet-data-1) {
		@apply bg-red-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='serialnumber-smokedetector-unknown']>div) {
		@apply bg-red-200;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line']>div.packet-data-1) {
		@apply bg-amber-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line']>div) {
		@apply bg-amber-200;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line-unknown']>div.packet-data-1) {
		@apply bg-red-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line-unknown']>div) {
		@apply bg-red-200;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line-unset']>div.packet-data-1) {
		@apply bg-gray-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line-unset']>div) {
		@apply bg-gray-300;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='hops']>div.packet-data-1) {
		@apply bg-purple-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='hops']>div) {
		@apply bg-purple-300;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='sequence-nr']>div.packet-data-1) {
		@apply bg-stone-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='sequence-nr']>div) {
		@apply bg-stone-300;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='alarm-start']>div.packet-data-1) {
		@apply bg-pink-600;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='alarm-start']>div) {
		@apply bg-pink-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='alarm-stop']>div.packet-data-1) {
		@apply bg-green-600;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='alarm-stop']>div) {
		@apply bg-green-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line-test-start']>div.packet-data-1) {
		@apply bg-lime-500;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line-test-start']>div) {
		@apply bg-lime-300;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line-test-stop']>div.packet-data-1) {
		@apply bg-lime-700 text-white;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='line-test-stop']>div) {
		@apply bg-lime-600 text-white;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='time']>div.packet-data-1) {
		@apply bg-emerald-400;
	}

	:global(div.genius-packet>div.packet-data-container>div[data-type='time']>div) {
		@apply bg-emerald-200;
	}
</style>
