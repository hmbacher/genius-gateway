<script lang="ts">
	import type { Packet, CommissioningInfo } from '$lib/types/models';
	import { PacketTypes, PacketTypeNames } from '$lib/types/models';
	import GeniusPacketDataBlock from '$lib/components/GeniusPacketDataBlock.svelte';
	import GeniusPacketContentComissioning from './GeniusPacketContentComissioning.svelte';
	import GeniusPacketContentLineTestStart from './GeniusPacketContentLineTestStart.svelte';
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
