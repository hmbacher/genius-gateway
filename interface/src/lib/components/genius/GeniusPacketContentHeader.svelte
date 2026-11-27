<script lang="ts">
	import type { Packet } from '$lib/types/models';
	import { entityStyleSuffix } from '$lib/genius/entity';
	import { lineCodeText } from '$lib/genius/line';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import IconWifi from '~icons/tabler/wifi';
	import IconRing from '~icons/tabler/topology-ring-2';
	import IconHops from '~icons/tabler/arrow-forward-up';
	import IconHash from '~icons/tabler/hash';

	interface Props {
		packet: Packet;
		showDetails?: boolean;
	}

	let { packet, showDetails = true }: Props = $props();

	// Addressed radio module (offset 4-7): FF FF FF FF = all modules.
	let toAllModules = $derived(packet.data.subarray(4, 8).every((b) => b === 0xff));
</script>

<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(0, 1)} />
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(1, 3)}
	endianess="little"
	details={{
		text: 'Remaining TX Time',
		type: 'remaining-tx-time'
	}}
/>
<!-- 3: line code (rotary position) of the addressed line -->
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(3, 4)}
	details={{ text: lineCodeText(packet.data[3]), type: 'line-code' }}
/>
<!-- 4..7: addressed radio module -->
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(4, 8)}
	endianess="big"
	details={{ text: toAllModules ? 'All modules' : undefined }}
/>
<!-- 8: line code of the originator -->
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(8, 9)}
	details={{ text: lineCodeText(packet.data[8]), type: 'line-code' }}
/>
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(9, 13)}
	endianess="big"
	details={{
		icon: IconWifi,
		text: packet.generalInfo?.firstLocation,
		type: 'serialnumber-radiomodule'.concat(entityStyleSuffix(packet.generalInfo?.firstLocation))
	}}
/>
<!-- 13: line code of the forwarder -->
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(13, 14)}
	details={{ text: lineCodeText(packet.data[13]), type: 'line-code' }}
/>
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(14, 18)}
	endianess="big"
	details={{
		icon: IconWifi,
		text: packet.generalInfo?.secondLocation,
		type: 'serialnumber-radiomodule'.concat(entityStyleSuffix(packet.generalInfo?.secondLocation))
	}}
/>
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(18, 22)}
	endianess="big"
	details={{
		icon: IconRing,
		text: packet.generalInfo?.lineName,
		type: 'line'.concat(packet.generalInfo?.lineName === 'Unknown' ? '-unknown' : '')
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
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(23, 24)}
	details={{
		icon: IconHash,
		type: 'sequence-nr'
	}}
/>
