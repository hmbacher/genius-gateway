<script lang="ts">
	import type { Packet, AlarmStartInfo } from '$lib/types/models';
	import GeniusPacketDataBlock from '$lib/components/GeniusPacketDataBlock.svelte';
	import IconWifi from '~icons/tabler/wifi';
	import IconHops from '~icons/tabler/arrow-forward-up';
	import IconDetector from '~icons/tabler/alarm-smoke';
	import IconRing from '~icons/tabler/topology-ring-2';
	import IconAlarmStart from '~icons/tabler/bell';

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
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(22, 23)}
	details={{
		icon: IconHops,
		text: packet.generalInfo?.hops.toString(),
		type: 'hops'
	}}
/>
<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(23, 28)} />
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(28, 29)}
	endianess="big"
	details={{
		icon: IconAlarmStart,
		type: 'alarm-start'
	}}
/>
<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(29, 32)} />
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(32, 36)}
	endianess="little"
	details={{
		icon: IconDetector,
		text: (packet.specificInfo as AlarmStartInfo)?.startingLocation,
		type: 'serialnumber-smokedetector'.concat(
			(packet.specificInfo as AlarmStartInfo)?.startingLocation === 'Unknown' ? '-unknown' : ''
		)
	}}
/>
