<script lang="ts">
	import type { Component } from 'svelte';
	import DirtyMarker from './DirtyMarker.svelte';
	import IconChevron from '~icons/tabler/chevron-down';

	export interface IconSelectOption {
		value: number | string;
		label: string;
		/** Icon component shown only in the open dropdown, not in the closed trigger. */
		prefixIcon?: Component;
		prefixClass?: string;
		suffix?: string;
		labelClass?: string;
		/** Applied to the entire option row (background, text color, hover). Overrides default hover. */
		itemClass?: string;
		disabled?: boolean;
	}

	interface Props {
		value?: number | string;
		options: IconSelectOption[];
		placeholder?: string;
		class?: string;
		dirty?: boolean;
		onrevert?: () => void;
	}

	let {
		value = $bindable(),
		options,
		placeholder = '— select —',
		class: cls = '',
		dirty = false,
		onrevert = () => {}
	}: Props = $props();

	let open = $state(false);
	let el = $state<HTMLElement>();
	const listboxId = `icon-select-listbox-${Math.random().toString(36).slice(2)}`;

	const selected = $derived(options.find((o) => o.value === value));

	function pick(opt: IconSelectOption) {
		if (opt.disabled) return;
		value = opt.value;
		open = false;
	}
</script>

<svelte:window
	onclick={(e) => {
		if (open && el && !el.contains(e.target as Node)) open = false;
	}}
	onkeydown={(e) => {
		if (e.key === 'Escape') open = false;
	}}
/>

<!-- svelte-ignore a11y_interactive_supports_focus -->
<div
	bind:this={el}
	class="input relative overflow-visible pl-3 pr-3 gap-2 cursor-pointer {cls}"
	role="combobox"
	aria-expanded={open}
	aria-haspopup="listbox"
	aria-controls={listboxId}
	tabindex="0"
	onclick={(e) => {
		if ((e.target as HTMLElement).closest('button')) return;
		open = !open;
	}}
	onkeydown={(e) => {
		if (e.key === 'Enter' || e.key === ' ') {
			e.preventDefault();
			open = !open;
		}
	}}
>
	{#if dirty}
		<div class="pointer-events-none absolute inset-y-0 left-0 z-10 w-1 rounded-l-[var(--radius-field)] bg-red-300"></div>
	{/if}
	<span class="flex-1 truncate {selected ? '' : 'text-base-content/40'}">
		{selected?.label ?? placeholder}
	</span>
	<IconChevron
		class="h-4 w-4 shrink-0 text-base-content/50 transition-transform duration-150 {open
			? 'rotate-180'
			: ''}"
	/>
	<DirtyMarker {dirty} {onrevert} />

	{#if open}
		<ul
			id={listboxId}
			class="absolute right-0 top-full mt-1 z-50 max-h-64 min-w-full w-max overflow-y-auto rounded-box border border-base-300 bg-base-100 shadow-lg"
			role="listbox"
		>
			{#each options as opt}
				<!-- svelte-ignore a11y_click_events_have_key_events -->
				<li role="option" aria-selected={opt.value === value}>
					<button
						type="button"
						class="flex w-full items-center gap-2 px-3 py-1.5 text-left text-sm
							{opt.disabled ? 'cursor-not-allowed' : 'cursor-pointer'}
							{opt.disabled && !opt.itemClass ? 'opacity-40' : ''}
							{opt.itemClass ?? 'hover:bg-base-200'}
							{opt.value === value ? 'font-medium' : ''}"
						disabled={opt.disabled}
						onclick={(e) => {
							e.stopPropagation();
							pick(opt);
						}}
					>
						{#if opt.prefixIcon}
							<opt.prefixIcon class="h-4 w-4 shrink-0 {opt.prefixClass ?? ''}" />
						{/if}
						<span class={opt.labelClass ?? ''}>{opt.label}</span>
						{#if opt.suffix}
							<span class="text-base-content/50 text-xs">{opt.suffix}</span>
						{/if}
					</button>
				</li>
			{/each}
		</ul>
	{/if}
</div>
