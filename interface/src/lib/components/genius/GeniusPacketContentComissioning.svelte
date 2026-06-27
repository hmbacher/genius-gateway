<script lang="ts">
	import type { Packet, CommissioningInfo } from '$lib/types/models';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import IconClock from '~icons/tabler/clock';
	import IconRing from '~icons/tabler/topology-ring-2';

	interface Props {
		packet: Packet;
		showDetails?: boolean;
	}

	let { packet, showDetails = true }: Props = $props();
</script>

<GeniusPacketContentHeader {packet} {showDetails} />
<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(24, 28)} />
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(28, 32)}
	endianess="big"
	details={{
		icon: IconRing,
		text: 'New Line ID',
		type: 'line'
	}}
/>
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(32, 35)}
	details={{
		icon: IconClock,
		text: (packet.specificInfo as CommissioningInfo)?.timeStr,
		type: 'time'
	}}
/>
<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(35, 37)} />
