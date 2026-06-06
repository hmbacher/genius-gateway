<script lang="ts">
	import { cc1101Status } from '$lib/stores/cc1101.svelte';
	import IconAlert from '~icons/tabler/alert-triangle';
	import IconSettings from '~icons/tabler/settings';

	let show = $derived(
		cc1101Status.loaded &&
			(cc1101Status.state === 'unconfigured' || cc1101Status.state === 'error')
	);
</script>

{#if show}
	<div class="alert alert-error shadow-lg mx-6 mt-6">
		<IconAlert class="h-6 w-6 shrink-0" />
		<div>
			<p class="font-bold">
				{cc1101Status.state === 'error' ? 'Radio error' : 'Radio not configured'}
			</p>
			<p class="text-sm">
				{cc1101Status.state === 'error'
					? 'The CC1101 radio failed to initialize with the current pins.'
					: 'No SPI pins are configured for the CC1101 radio.'}
				No packets can be received or sent until this is resolved.
			</p>
		</div>
		<a href="/system/cc1101" class="btn btn-sm btn-primary">
			<IconSettings class="h-5 w-5" /> Configure
		</a>
	</div>
{/if}
