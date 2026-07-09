<script lang="ts">
	import { formatDate } from '$lib/utils/formatDate';
	import Wifi from '~icons/tabler/wifi';
	import Wifi2 from '~icons/tabler/wifi-2';
	import Wifi1 from '~icons/tabler/wifi-1';
	import Wifi0 from '~icons/tabler/wifi-0';
	import WifiOff from '~icons/tabler/wifi-off';
	import RadarOff from '~icons/tabler/radar-off';

	interface Props {
		/** Direct-link RSSI in dBm from a ConfigCheckProbe range test. `< 0` = reached; `0` (with a
		 *  timestamp) = tested but no response. */
		rssi?: number;
		/** Timestamp of the last range test this module was included in. Undefined = never tested. */
		lastRangeTest?: Date;
	}

	let { rssi, lastRangeTest }: Props = $props();

	// Three states surfaced to the list:
	//  'untested'  - never included in a range test        (lastRangeTest unset)   → radar-off
	//  'unreached' - tested but no response (out of range) (tested, rssi 0)        → wifi-off
	//  'reached'   - direct link measured                  (tested, rssi < 0)      → wifi-N (success)
	const state = $derived.by(() => {
		if (lastRangeTest === undefined) return 'untested';
		return rssi !== undefined && rssi < 0 ? 'reached' : 'unreached';
	});

	// Reached: pick the wifi glyph by signal strength (0..3 → wifi-0/1/2/wifi). Thresholds are for
	// the 868 MHz link at this data rate; they only drive the icon, the exact dBm is in the tooltip.
	const level = $derived.by(() => {
		if (state !== 'reached' || rssi === undefined) return 0;
		if (rssi >= -60) return 3; // wifi (full)
		if (rssi >= -70) return 2; // wifi-2
		if (rssi >= -80) return 1; // wifi-1
		return 0; // wifi-0
	});

	const qualityLabel = $derived(['Weak', 'Fair', 'Good', 'Excellent'][level]);
	const iconSize = 'h-6 w-6';

	const tip = $derived.by(() => {
		switch (state) {
			case 'reached':
				return `${rssi} dBm (${qualityLabel}) · ${formatDate(lastRangeTest!)}`;
			case 'unreached':
				return `Out of direct range`;
			default:
				return 'Not range-tested yet, run a device discovery';
		}
	});
</script>

<div class="tooltip tooltip-left inline-flex" data-tip={tip}>
	<span class="inline-flex items-center gap-1" aria-label={tip}>
		{#if state === 'reached'}
			<!-- Full glyph in light gray as a fixed-size backdrop; active arcs overlaid in success
			     green. Keeps the icon a consistent, legible size even at weak signal (wifi-0 = dot). -->
			<span class="relative inline-flex {iconSize}">
				<Wifi class="absolute inset-0 {iconSize} text-current/25" />
				{#if level === 3}
					<Wifi class="absolute inset-0 {iconSize} text-success" />
				{:else if level === 2}
					<Wifi2 class="absolute inset-0 {iconSize} text-success" />
				{:else if level === 1}
					<Wifi1 class="absolute inset-0 {iconSize} text-success" />
				{:else}
					<Wifi0 class="absolute inset-0 {iconSize} text-success" />
				{/if}
			</span>
		{:else if state === 'unreached'}
			<WifiOff class="{iconSize} text-current/40" />
		{:else}
			<RadarOff class="{iconSize} text-current/40" />
		{/if}
	</span>
</div>
