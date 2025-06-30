<script lang="ts">
	import type { Packet, CommissioningInfo } from '$lib/types/models';
	import GeniusPacketDataBlock from '$lib/components/GeniusPacketDataBlock.svelte';
	import IconWifi from '~icons/tabler/wifi';
	import IconRing from '~icons/tabler/topology-ring-2';

	interface Props {
		packet: Packet;
		showDetails?: boolean;
	}

	let { packet, showDetails = true }: Props = $props();
</script>

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
		text: packet.generalInfo?.firstLocation,
		type: 'serialnumber-radiomodule'.concat(
			packet.generalInfo?.firstLocation === 'Unknown' ? '-unknown' : ''
		)
	}}
/>
<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(13, 14)} />
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(14, 18)}
	endianess="big"
	details={{
		icon: IconWifi,
		text: packet.generalInfo?.secondLocation,
		type: 'serialnumber-radiomodule'.concat(
			packet.generalInfo?.secondLocation === 'Unknown' ? '-unknown' : ''
		)
	}}
/>
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(18, 22)}
	endianess="big"
	details={{
		icon: IconRing,
		text: packet.generalInfo?.lineName,
		type: 'line'
	}}
/>
<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(22, 29)} />
