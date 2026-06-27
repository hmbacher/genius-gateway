<script lang="ts">
	import type { Packet, AlarmStartInfo } from '$lib/types/models';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import IconDetector from '~icons/tabler/alarm-smoke';
	import IconAlarmStart from '~icons/tabler/bell';

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
	data={packet.data.subarray(28, 29)}
	endianess="big"
	details={{
		icon: IconAlarmStart,
		type: 'alarm-start'
	}}
/>
<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(29, 32)} />
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
