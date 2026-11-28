<script lang="ts">
	import type { Packet } from '$lib/types/models';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import { lineCodeText } from '$lib/genius/line';

	interface Props {
		packet: Packet;
		showDetails?: boolean;
	}

	let { packet, showDetails = true }: Props = $props();

	// Line code of the responder (offset 30): the position of its rotary switches,
	// high nibble = letter A-H, low nibble = number 0-9.
	let lineCode = $derived(packet.data[30] ?? 0);
</script>

<GeniusPacketContentHeader {packet} {showDetails} />
<!-- 24..29: flags, const, 0x55 family marker, 0x08 subtype, const -->
<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(24, 29)} />
<!-- 29: per-device status / capability flag -->
<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(29, 30)} details={{ text: 'Status' }} />
<!-- 30: line code of the responder -->
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(30, 31)}
	details={{ text: lineCodeText(lineCode), type: 'line-code' }}
/>
<!-- 31..36: trailing (const 60 00 00 + checksum/metric) -->
<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(31, 36)} />
