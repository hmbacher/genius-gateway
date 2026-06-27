<script lang="ts">
	import type { Packet, AlarmStopInfo } from '$lib/types/models';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import IconDetector from '~icons/tabler/alarm-smoke';
	import IconAlarmStop from '~icons/tabler/bell-off';

	interface Props {
		packet: Packet;
		showDetails?: boolean;
	}

	let { packet, showDetails = true }: Props = $props();
</script>

<GeniusPacketContentHeader {packet} {showDetails} />
<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(24, 30)} />
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(30, 31)}
	endianess="big"
	details={{
		icon: IconAlarmStop,
		type: 'alarm-stop'
	}}
/>
<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(31, 32)} />
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(32, 36)}
	endianess="little"
	details={{
		icon: IconDetector,
		text: (packet.specificInfo as AlarmStopInfo)?.silencingLocation,
		type: 'serialnumber-smokedetector'.concat(
			(packet.specificInfo as AlarmStopInfo)?.silencingLocation === 'Unknown' ? '-unknown' : ''
		)
	}}
/>
