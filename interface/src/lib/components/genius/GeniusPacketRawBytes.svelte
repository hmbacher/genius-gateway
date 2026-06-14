<script lang="ts">
	import GeniusPacketDataBlock from './GeniusPacketDataBlock.svelte';

	interface Props {
		data: Uint8Array;
		showDetails?: boolean;
	}

	let { data, showDetails = true }: Props = $props();
</script>

{#each data as byte}
	<GeniusPacketDataBlock {showDetails} data={new Uint8Array([byte])} />
{/each}
