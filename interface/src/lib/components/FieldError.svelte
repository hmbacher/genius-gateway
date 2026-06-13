<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	let { show, message }: { show: boolean; message: string } = $props();
</script>

{#if show}
	<div transition:slide={{ duration: 300, easing: cubicOut }}>
		<span class="text-error text-xs">{message}</span>
	</div>
{/if}
