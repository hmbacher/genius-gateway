<script lang="ts">
	import type { Packet } from '$lib/types/models';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import IconRing from '~icons/tabler/topology-ring-2';

	interface Props {
		packet: Packet;
		showDetails?: boolean;
	}

	let { packet, showDetails = true }: Props = $props();

	// Responder group/line (offset 30): high nibble = group A-H, low nibble = line 0-9.
	let groupLineByte = $derived(packet.data[30] ?? 0);
	let groupLineText = $derived(
		`${String.fromCharCode(65 + ((groupLineByte >> 4) & 0x0f))}.${groupLineByte & 0x0f}`
	);
</script>

<GeniusPacketContentHeader {packet} {showDetails} />
<!-- 24..29: flags, const, 0x55 family marker, 0x08 subtype, const -->
<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(24, 29)} />
<!-- 29: per-device status / capability flag -->
<GeniusPacketDataBlock {showDetails} data={packet.data.subarray(29, 30)} details={{ text: 'Status' }} />
<!-- 30: responder group / line -->
<GeniusPacketDataBlock
	{showDetails}
	data={packet.data.subarray(30, 31)}
	details={{ icon: IconRing, text: groupLineText, type: 'line' }}
/>
<!-- 31..36: trailing (const 60 00 00 + checksum/metric) -->
<GeniusPacketRawBytes {showDetails} data={packet.data.subarray(31, 36)} />
