<script lang="ts">
	import type { Packet, LineClockInfo } from '$lib/types/models';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import IconClock from '~icons/tabler/clock';
	import IconCalendar from '~icons/tabler/calendar';

	// Line clock: hour, minute and second of the line's own clock (bytes 28-30) and a day
	// counter (bytes 31-32, little endian).
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
	data={packet.data.subarray(28, 31)}
	details={{
		icon: IconClock,
		text: (packet.specificInfo as LineClockInfo)?.timeStr,
		type: 'time'
	}}
/>
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(31, 33)}
	endianess="little"
	details={{
		icon: IconCalendar,
		text: 'Day',
		type: 'time'
	}}
/>
