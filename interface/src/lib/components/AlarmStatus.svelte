<script lang="ts">
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import { deviceCondition } from '$lib/utils/deviceStatus';
	import IconOK from '~icons/tabler/heart';
	import IconEmpty from '~icons/tabler/heart-off';
	import IconFault from '~icons/tabler/heart-exclamation';
	import IconUnknown from '~icons/tabler/heart-question';
	import IconAlert from '~icons/tabler/alert-hexagon-filled';
	import Loader from '~icons/tabler/loader-2';

	// No device list received yet - avoid flashing the green "all good" heart on first paint
	let loading = $derived(!geniusDevices.isLoaded);

	let isEmpty = $derived(geniusDevices.isLoaded && geniusDevices.devices.length === 0);

	// Same four conditions as the overview cards. A reported fault is the only thing that turns
	// the heart yellow; detectors whose status is unknown (no current readout, or reset) get a
	// neutral question mark.
	let numFaulty = $derived(
		geniusDevices.devices.filter((d) => deviceCondition(d) === 'fault').length
	);
	let numUnknown = $derived(
		geniusDevices.devices.filter((d) => deviceCondition(d) === 'unknown').length
	);

	let tooltip = $derived(
		loading
			? 'Loading smoke detectors…'
			: geniusDevices.isAlarming
				? 'Smoke detected!'
				: isEmpty
					? 'No smoke detectors configured yet.'
					: numFaulty > 0
						? numFaulty === 1
							? '1 smoke detector reports a fault.'
							: `${numFaulty} smoke detectors report a fault.`
						: numUnknown > 0
							? numUnknown === 1
								? 'The status of 1 smoke detector is unknown.'
								: `The status of ${numUnknown} smoke detectors is unknown.`
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
		{:else if numFaulty > 0}
			<IconFault class="text-warning h-7 w-7" />
		{:else if numUnknown > 0}
			<IconUnknown class="h-7 w-7 opacity-70" />
		{:else}
			<IconOK class="text-success h-7 w-7" />
		{/if}
	</a>
</div>
