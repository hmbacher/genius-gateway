<script lang="ts">
	import type { Packet, NamedModuleInfo } from '$lib/types/models';
	import { entityStyleSuffix } from '$lib/genius/entity';
	import GeniusPacketContentHeader from './GeniusPacketContentHeader.svelte';
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';
	import GeniusPacketRawBytes from './GeniusPacketRawBytes.svelte';
	import IconWifi from '~icons/tabler/wifi';

	// Module Missing and Module Present (radio link supervision): bytes 28-31 name a radio
	// module by its serial number.
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
		text: (packet.specificInfo as NamedModuleInfo)?.location,
		type: 'serialnumber-radiomodule'.concat(
			entityStyleSuffix((packet.specificInfo as NamedModuleInfo)?.location)
		)
	}}
/>
