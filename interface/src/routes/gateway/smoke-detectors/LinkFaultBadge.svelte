<script lang="ts">
	import type { GeniusLinkFault } from '$lib/types/models';
	import { formatDateTime } from '$lib/utils/formatDate';
	import AntennaOff from '~icons/tabler/antenna-off';
	import Warning from '~icons/tabler/alert-triangle';

	/**
	 * Radio link fault of one device (RAM-only runtime state of the gateway).
	 *
	 *  - warning: another module was reported missing, the fault itself has not been signalled yet
	 *  - active:  the detector signals a radio network fault
	 *  - acked:   the fault was acknowledged on the detector (stays visible until it expires after 48 h)
	 *
	 * The fault is shown on the device that reported it. If the report named the missing module, that
	 * module is shown as the cause.
	 */
	interface Props {
		fault: GeniusLinkFault;
		isAlarming?: boolean;
		compact?: boolean;
	}

	let { fault, isAlarming = false, compact = false }: Props = $props();

	const color = $derived(
		isAlarming
			? 'text-current'
			: fault.state === 'active'
				? 'text-error'
				: 'text-warning'
	);
	const iconSize = $derived(compact ? 'h-6 w-6' : 'h-5 w-5');

	const label = $derived(
		fault.state === 'warning'
			? 'Link warning'
			: fault.state === 'acked'
				? 'Link fault · acknowledged'
				: 'Link fault'
	);

	const cause = $derived(
		fault.causeSn
			? fault.causeLocation
				? `${fault.causeLocation} (${fault.causeSn})`
				: `radio module ${fault.causeSn}`
			: ''
	);

	const tip = $derived.by(() => {
		const parts: string[] = [
			fault.state === 'warning'
				? 'Radio link warning'
				: fault.state === 'acked'
					? 'Radio network fault, acknowledged'
					: 'Radio network fault'
		];
		if (cause) parts.push(`Missing: ${cause}`);
		if (fault.startUnknown) parts.push('Start unknown');
		else if (fault.since) parts.push(`Since ${formatDateTime(fault.since)}`);
		if (fault.acknowledgedAt) parts.push(`Acknowledged ${formatDateTime(fault.acknowledgedAt)}`);
		if (fault.lastReceived) parts.push(`Last received ${formatDateTime(fault.lastReceived)}`);
		if (fault.expiresInS !== undefined)
			parts.push(`Clears in ~${Math.max(1, Math.round(fault.expiresInS / 3600))} h unless received again`);
		return parts.join(' · ');
	});
</script>

<div class="tooltip tooltip-top inline-flex" data-tip={tip}>
	<span class="flex items-center gap-1 {color}" aria-label={tip}>
		{#if fault.state === 'warning'}
			<Warning class={iconSize} />
		{:else}
			<AntennaOff class={iconSize} />
		{/if}
		{#if !compact}<span class="truncate">{label}</span>{/if}
	</span>
</div>
