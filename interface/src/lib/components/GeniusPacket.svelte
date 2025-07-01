<script lang="ts">
	import type { Packet, CommissioningInfo } from '$lib/types/models';
	import { PacketTypes } from '$lib/types/models';
	import GeniusPacketDataBlock from '$lib/components/GeniusPacketDataBlock.svelte';
	import GeniusPacketContentComissioning from './GeniusPacketContentComissioning.svelte';
	import GeniusPacketContentLineTestStart from './GeniusPacketContentLineTestStart.svelte';
	import GeniusPacketContentDiscoveryRequest from './GeniusPacketContentDiscoveryRequest.svelte';
	import GeniusPacketContentDiscoveryResponse from './GeniusPacketContentDiscoveryResponse.svelte';

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
				<div class="meta-data {packet.type.cssClass}">{packet.type.name}</div>
			{/if}
		</div>
	{/if}
	<div class="packet-data-container">
		{#if packet.type.name === PacketTypes.Commissioning.name}
			<GeniusPacketContentComissioning {packet} {showDetails} />
		{:else if packet.type.name === PacketTypes.LineTestStart.name}
			<GeniusPacketContentLineTestStart {packet} {showDetails} />
		{:else if packet.type.name === PacketTypes.DiscoveryRequest.name}
			<GeniusPacketContentDiscoveryRequest {packet} {showDetails} />
		{:else if packet.type.name === PacketTypes.DiscoveryResponse.name}
			<GeniusPacketContentDiscoveryResponse {packet} {showDetails} />
		{:else}
			{#each packet.data as byte}
				<GeniusPacketDataBlock {showDetails} data={new Uint8Array([byte])} />
			{/each}
		{/if}
	</div>
</div>
