<script lang="ts">
	interface Props {
		data: Uint8Array;
		showDetails?: boolean;
		endianess?: string;
		details?: any;
	}

	let { data, showDetails = true, endianess, details }: Props = $props();

	function combineToUnsigned(data: Uint8Array, endianess: string): number {
		if (endianess === 'little') {
			return data.reduce((acc, byte, index) => acc + (byte << (index * 8)), 0);
		} else if (endianess === 'big') {
			return data.reduce((acc, byte, index) => acc + (byte << ((data.length - 1 - index) * 8)), 0);
		} else {
			throw new Error('Invalid endianess specified. Use "little" or "big".');
		}
	}

</script>

<div class="grid items-center gap-1 text-sm" data-type={showDetails && details && details.type ? details.type : ''} style="grid-template-columns: repeat({data.length}, minmax(0, 1fr));">
	{#each data as byte}
		<div class="packet-data-1">{byte.toString(16).padStart(2, '0').toUpperCase()}</div>
	{/each}
	{#if showDetails}
		{#if endianess}
			<div class="col-span-{data.length}">{combineToUnsigned(data, endianess) >>> 0}</div>
		{:else}
			{#each data as byte}
				<div>{byte.toString()}</div>
			{/each}
		{/if}
		{#if details}
			{#if details.icon && details.text}
				<div class="col-span-{data.length} flex items-center justify-center">
					<details.icon class="flex-none mr-[2px] h-4 w-4" />
					<span class="truncate">{details.text}</span>
				</div>
			{:else if details.text}
				<div class="col-span-{data.length} truncate">{details.text}</div>
			{:else if details.icon}
				<div class="col-span-{data.length}">
					<details.icon class="flex-none h-4 w-4" />
				</div>
			{/if}
		{/if}
	{/if}
</div>
