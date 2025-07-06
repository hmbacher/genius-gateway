<script lang="ts">
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import IconOK from '~icons/tabler/heart';
	import IconAlert from '~icons/tabler/alert-hexagon-filled';
</script>

<div
	class="tooltip tooltip-left"
	data-tip={geniusDevices.isAlarming ? 'Smoke detected!' : 'All smoke detectors in standby.'}
>
	<div class="flex-none">
		{#if geniusDevices.isAlarming}
			<IconAlert class="text-error h-9 w-9" />
		{:else}
			<IconOK class="text-success h-9 w-9" />
		{/if}
	</div>
</div>
