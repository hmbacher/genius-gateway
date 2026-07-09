<script lang="ts">
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import { hasReadout, isStaleReadout, getDeviceFaults } from '$lib/utils/deviceStatus';
	import IconOK from '~icons/tabler/heart';
	import IconEmpty from '~icons/tabler/heart-off';
	import IconWarning from '~icons/tabler/heart-exclamation';
	import IconAlert from '~icons/tabler/alert-hexagon-filled';
	import Loader from '~icons/tabler/loader-2';

	// No device list received yet - avoid flashing the green "all good" heart on first paint
	let loading = $derived(!geniusDevices.isLoaded);

	let hasWarning = $derived(
		!geniusDevices.isAlarming &&
			geniusDevices.devices.some((d) => {
				if (!hasReadout(d)) return true;
				if (isStaleReadout(d)) return true;
				return getDeviceFaults(d).length > 0;
			})
	);

	let isEmpty = $derived(geniusDevices.isLoaded && geniusDevices.devices.length === 0);

	let tooltip = $derived(
		loading
			? 'Loading smoke detectors…'
			: geniusDevices.isAlarming
			? 'Smoke detected!'
			: isEmpty
				? 'No smoke detectors configured yet.'
				: hasWarning
					? 'One or more smoke detectors need attention.'
					: 'All smoke detectors in standby.'
	);
</script>

<div class="tooltip tooltip-left" data-tip={tooltip}>
	<a
		href={isEmpty ? '/gateway/smoke-detectors' : '/'}
		aria-label={isEmpty ? 'Go to smoke detector configuration' : 'Go to overview'}
		class="flex-none block hover:scale-110 active:scale-95 transition-transform"
	>
		{#if loading}
			<Loader class="h-7 w-7 animate-spin opacity-50" />
		{:else if geniusDevices.isAlarming}
			<IconAlert class="text-error h-7 w-7" />
		{:else if isEmpty}
			<IconEmpty class="h-7 w-7" />
		{:else if hasWarning}
			<IconWarning class="text-warning h-7 w-7" />
		{:else}
			<IconOK class="text-success h-7 w-7" />
		{/if}
	</a>
</div>
