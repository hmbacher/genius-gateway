<script lang="ts">
	import { cc1101Status } from '$lib/stores/cc1101.svelte';
	import IconEar from '~icons/tabler/ear';
	import IconBroadcast from '~icons/tabler/building-broadcast-tower';
	import IconSettingsOff from '~icons/tabler/settings-off';
	import IconAlert from '~icons/tabler/alert-hexagon-filled';
	import Loader from '~icons/tabler/loader-2';

	// No status received yet — avoid flashing the red "not configured" icon on first paint
	let loading = $derived(!cc1101Status.loaded);
	// Radio is up AND in RX mode → actively listening for packets
	let listening = $derived(cc1101Status.state === 'ok' && cc1101Status.mode === 'rx');
	// Radio is up AND sending (e.g. an alarm command from the Alarm Lines page)
	let transmitting = $derived(cc1101Status.state === 'ok' && cc1101Status.mode === 'tx');

	let tooltip = $derived(
		loading
			? 'Loading radio status…'
			: transmitting
			? 'Radio transmitting (TX)'
			: listening
			? 'Radio listening (RX)'
			: cc1101Status.state === 'ok'
				? 'Radio ready'
				: cc1101Status.state === 'initializing'
					? 'Radio initializing…'
					: cc1101Status.state === 'error'
						? 'Radio error — check pin configuration'
						: 'Radio not configured'
	);
</script>

<div class="tooltip tooltip-left" data-tip={tooltip}>
	<a
		href="/system/cc1101"
		aria-label="Radio status and configuration"
		class="flex-none block hover:scale-110 active:scale-95 transition-transform"
	>
		{#if loading}
			<Loader class="h-7 w-7 animate-spin opacity-50" />
		{:else if transmitting}
			<IconBroadcast class="text-info h-7 w-7" />
		{:else if listening}
			<IconEar class="h-7 w-7" />
		{:else if cc1101Status.state === 'initializing'}
			<Loader class="text-info h-7 w-7 animate-spin" />
		{:else if cc1101Status.state === 'error'}
			<IconAlert class="text-error h-7 w-7" />
		{:else}
			<IconSettingsOff class="text-error h-7 w-7" />
		{/if}
	</a>
</div>
