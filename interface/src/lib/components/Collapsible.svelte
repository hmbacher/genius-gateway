<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import Down from '~icons/tabler/chevron-down';
	import IconRevert from '~icons/tabler/arrow-back-up';

	interface Props {
		open?: boolean;
		opened?: any;
		closed?: any;
		collapsible?: boolean;
		icon?: import('svelte').Snippet;
		title?: import('svelte').Snippet;
		children?: import('svelte').Snippet;
		class?: string;
		isDirty?: boolean;
		/** When set, the dirty indicator becomes a clickable button that reverts all changes. */
		onRevert?: () => void;
	}

	let {
		open = $bindable(false),
		opened,
		closed,
		icon,
		title,
		children,
		class: className = '',
		isDirty = false,
		onRevert
	}: Props = $props();

	function openCollapsible() {
		open = !open;
		if (open) {
			if (opened) opened();
		} else {
			if (closed) closed();
		}
	}
</script>

<div class="{className} relative grid w-full max-w-2xl self-center overflow-hidden">
	{#if isDirty}
		<div class="absolute left-0 top-0 w-1.5 h-full bg-red-300"></div>
	{/if}
	<div class="min-h-16 flex w-full items-center justify-between space-x-3 p-4 text-xl font-medium">
		<span class="inline-flex items-start gap-2">
			<span class="shrink-0 inline-flex mt-0.5">{@render icon?.()}</span>
			{@render title?.()}
			{#if isDirty}
				{#if onRevert}
					<button
						type="button"
						data-tip="Revert all changes"
						aria-label="Revert all changes"
						class="tooltip tooltip-right text-error self-center ml-2 flex shrink-0 cursor-pointer items-center"
						onclick={() => onRevert?.()}
					>
						<IconRevert class="h-6 w-6" />
					</button>
				{:else}
					<div data-tip="There are unsaved changes." class="tooltip tooltip-right tooltip-error">
						<IconRevert class="text-error flex-shrink-0 ml-2 h-6 w-6 self-center cursor-help" />
					</div>
				{/if}
			{/if}
		</span>
		<button class="btn btn-circle btn-ghost btn-sm self-start" onclick={() => openCollapsible()}>
			<Down
				class="text-base-content h-auto w-6 transition-transform duration-300 ease-in-out {open
					? 'rotate-180'
					: ''}"
			/>
		</button>
	</div>
	{#if open}
		<div
			class="flex flex-col gap-2 p-4 pt-0"
			transition:slide|local={{ duration: 300, easing: cubicOut }}
		>
			{@render children?.()}
		</div>
	{/if}
</div>
