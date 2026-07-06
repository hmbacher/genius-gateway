<script lang="ts">
	import type { Packet, CommissioningProbeResponseInfo } from '$lib/types/models';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import IconWifi from '~icons/tabler/wifi';

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
		icon: IconWifi,
		text: (packet.specificInfo as CommissioningProbeResponseInfo)?.requestingLocation,
		type: 'serialnumber-radiomodule'.concat(
			(packet.specificInfo as CommissioningProbeResponseInfo)?.requestingLocation === 'Unknown' ? '-unknown' : ''
		)
	}}
/>
