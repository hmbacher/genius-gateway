<script lang="ts">
	import type { LineGroup } from '$lib/genius/lineGroups';
	import { deviceCondition } from '$lib/utils/deviceStatus';
	import IconLine from '~icons/tabler/topology-ring-2';
	import ChevronDown from '~icons/tabler/chevron-down';
	import ChevronRight from '~icons/tabler/chevron-right';

	// Heading of one alarm line above its smoke detectors (overview and smoke detector list).
	// With onToggle it is a button that folds the group away and shows it again.
	interface Props {
		group: LineGroup;
		class?: string;
		collapsed?: boolean;
		onToggle?: () => void;
	}

	let { group, class: className = '', collapsed = false, onToggle }: Props = $props();

	let alarming = $derived(group.devices.filter((d) => d.isAlarming).length);
	let faulty = $derived(group.devices.filter((d) => deviceCondition(d) === 'fault').length);
	// The default names of a line already contain its ID
	let showId = $derived(group.key !== 0 && !group.name.includes(String(group.key)));
</script>

{#snippet content()}
	<h2 class="flex items-center gap-2 text-lg font-semibold">
		{#if onToggle}
			{#if collapsed}
				<ChevronRight class="h-5 w-5 shrink-0" />
			{:else}
				<ChevronDown class="h-5 w-5 shrink-0" />
			{/if}
		{/if}
		<IconLine class="h-5 w-5 shrink-0" />{group.name}
	</h2>
	{#if showId}
		<span class="text-sm text-base-content/40">#{group.key}</span>
	{/if}
	<span class="text-sm text-base-content/60">
		{group.devices.length}
		{group.devices.length === 1 ? 'detector' : 'detectors'}
	</span>
	{#if alarming > 0}
		<span class="badge badge-error badge-sm">{alarming} alarming</span>
	{/if}
	{#if faulty > 0}
		<span class="badge badge-warning badge-sm">{faulty} {faulty === 1 ? 'fault' : 'faults'}</span>
	{/if}
{/snippet}

{#if onToggle}
	<button
		type="button"
		class="flex w-full cursor-pointer flex-wrap items-center gap-x-3 gap-y-1 text-left {className}"
		data-line={group.key}
		aria-expanded={!collapsed}
		onclick={onToggle}
	>
		{@render content()}
	</button>
{:else}
	<div class="flex w-full flex-wrap items-center gap-x-3 gap-y-1 {className}" data-line={group.key}>
		{@render content()}
	</div>
{/if}
