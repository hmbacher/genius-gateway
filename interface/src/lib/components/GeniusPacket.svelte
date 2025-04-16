<script lang="ts">
	import type {
		VisualizerSettings,
		PacketType,
		CommissioningPacket,
		Packet
	} from '$lib/types/models';
	import { PacketTypes } from '$lib/types/models';
	import GeniusPacketDataBlock from '$lib/components/GeniusPacketDataBlock.svelte';
	import IconWifi from '~icons/tabler/wifi';
	import IconHops from '~icons/tabler/arrow-forward-up';
	import IconClock from '~icons/tabler/clock';

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
			<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(0, 1)} />
			<GeniusPacketDataBlock
				{showDetails}
				data={packet.data.subarray(1, 3)}
				endianess="little"
				details={{
					text: 'Counter',
					type: 'counter'
				}}
			/>
			<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(3, 9)} />
			<GeniusPacketDataBlock
				{showDetails}
				data={packet.data.subarray(9, 13)}
				endianess="big"
				details={{
					icon: IconWifi,
					text: packet.interpreted?.firstLocation,
					type: 'serialnumber-radiomodule'.concat(packet.interpreted?.firstLocation === 'Unknown' ? '-unknown' : '')
				}}
			/>
			<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(13, 14)} />
			<GeniusPacketDataBlock
				{showDetails}
				data={packet.data.subarray(14, 18)}
				endianess="big"
				details={{
					icon: IconWifi,
					text: packet.interpreted?.secondLocation,
					type: 'serialnumber-radiomodule'.concat(packet.interpreted?.secondLocation === 'Unknown' ? '-unknown' : '')
				}}
			/>
			<GeniusPacketDataBlock
				{showDetails}
				data={packet.data.subarray(18, 22)}
				endianess="big"
				details={{
					text: 'Curr. Line ID',
					type: 'line'
				}}
			/>
			<GeniusPacketDataBlock
				{showDetails}
				data={packet.data.subarray(22, 23)}
				details={{
					icon: IconHops,
					text: packet.interpreted?.hops.toString(),
					type: 'hops'
				}}
			/>
			<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(23, 28)} />
			<GeniusPacketDataBlock
				{showDetails}
				data={packet.data.subarray(28, 32)}
				endianess="big"
				details={{
					text: 'New Line ID',
					type: 'line'
				}}
			/>
			<GeniusPacketDataBlock
				{showDetails}
				data={packet.data.subarray(32, 35)}
				details={{
					icon: IconClock,
					text: packet.interpreted?.timeStr,
					type: 'time'
				}}
			/>
			<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(35, 37)} />
		{:else}
			{#each packet.data as byte}
				<GeniusPacketDataBlock {showDetails} data={new Uint8Array([byte])} />
			{/each}
		{/if}
	</div>
</div>
