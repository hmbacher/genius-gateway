<script lang="ts">
	import StatusOk from '~icons/tabler/circle-check';
	import StatusFault from '~icons/tabler/alert-circle';

	/**
	 * Compact OK / Fault badge used on each row of the smoke-detector list.
	 *
	 * The status icon is rendered as a button (so the row's "details" dialog
	 * can be opened on click). Compact mode (mobile) omits the text label; the
	 * full mode (desktop) shows it.
	 *
	 * Color rules:
	 *   - alarming row → inherit `text-current` so the icon contrasts on the red bg
	 *   - stale readout + OK → dimmed to indicate "data may be outdated"
	 *   - otherwise → success/error theme colors
	 */
	interface Props {
		hasReadout: boolean;
		hasFaults: boolean;
		stale: boolean;
		isAlarming: boolean;
		faults: string[];
		compact?: boolean;
		onclick?: () => void;
	}

	let {
		hasReadout,
		hasFaults,
		stale,
		isAlarming,
		faults,
		compact = false,
		onclick
	}: Props = $props();

	const successColor = $derived(isAlarming ? 'text-current' : 'text-success');
	const errorColor = $derived(isAlarming ? 'text-current' : 'text-error');
	const dimmed = $derived(stale ? 'text-current/40' : successColor);

	const iconSize = compact ? 'h-6 w-6' : 'h-5 w-5';
</script>

{#if !hasReadout}
	{#if compact}
		<span class="text-current/40 italic">No readout</span>
	{:else}
		<div class="text-current/40 italic truncate">Status not available</div>
	{/if}
{:else if !hasFaults}
	<div class="tooltip tooltip-top" data-tip="Status OK">
		<button class="flex items-center gap-1 cursor-pointer hover:opacity-80" {onclick}>
			<StatusOk class="{iconSize} {dimmed}" />
			{#if !compact}<span class={dimmed}>OK</span>{/if}
		</button>
	</div>
{:else}
	<div class="tooltip tooltip-top" data-tip={faults.join(', ')}>
		<button class="flex items-center gap-1 cursor-pointer hover:opacity-80" {onclick}>
			<StatusFault class="{iconSize} {errorColor}" />
			{#if !compact}<span class={errorColor}>Fault</span>{/if}
		</button>
	</div>
{/if}
