<script lang="ts">
	import type { Component, Snippet } from 'svelte';

	/**
	 * One row in the DeviceDetailsDialog: icon · label · value(s).
	 *
	 * The value can be supplied either as plain text (`value`) or as a snippet
	 * (`children`) for richer content (e.g. a status icon + colored text). When
	 * neither is set, the row renders just the label cell - useful as a section
	 * header above a group of indented sub-rows.
	 *
	 * `indent` shifts the icon column right for sub-rows (e.g. individual fault
	 * flags beneath their summary row).
	 */
	type IconComponent = Component<{ class?: string }>;

	interface Props {
		icon?: IconComponent;
		label?: string;
		value?: string | number;
		labelWidth?: string;
		indent?: 'none' | 'sm' | 'md';
		iconSize?: 'sm' | 'md';
		children?: Snippet;
	}

	let {
		icon: Icon,
		label,
		value,
		labelWidth = 'w-28',
		indent = 'none',
		iconSize = 'md',
		children
	}: Props = $props();

	const indentClass = $derived(indent === 'sm' ? 'pl-6' : indent === 'md' ? 'pl-8' : '');
	const iconClass = $derived(iconSize === 'sm' ? 'h-3.5 w-3.5' : 'h-4 w-4');
</script>

<div class="flex items-center gap-2 {indentClass}">
	{#if Icon}<Icon class="{iconClass} flex-shrink-0" />{/if}
	{#if label}<span class="text-base-content/60 {labelWidth} flex-shrink-0">{label}</span>{/if}
	{#if children}
		{@render children()}
	{:else if value !== undefined}
		<span class="font-medium">{value}</span>
	{/if}
</div>
