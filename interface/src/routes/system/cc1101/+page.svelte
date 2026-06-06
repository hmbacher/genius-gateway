<script lang="ts">
	import { user } from '$lib/stores/user';
	import { goto } from '$app/navigation';
	import PinConfig from './PinConfig.svelte';
</script>

{#if $user.admin}
	<div class="mx-0 my-1 flex flex-col sm:mx-8 sm:my-8">
		<PinConfig />
	</div>
{:else}
	{goto('/')}
{/if}
