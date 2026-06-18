<script lang="ts">
	import IconInfo from '~icons/tabler/info-circle';

	type Placement = 'bottom-start' | 'bottom-end' | 'top-start' | 'top-end';

	interface Props {
		/** Rich, multi-line HTML content (paragraphs, links, etc.) shown in the popover. */
		children?: import('svelte').Snippet;
		/** Accessible label for the trigger icon. */
		label?: string;
		placement?: Placement;
		class?: string;
		iconClass?: string;
	}

	let {
		children,
		label = 'More information',
		placement = 'bottom-start',
		class: className = '',
		iconClass = ''
	}: Props = $props();

	const PREFERRED_WIDTH = 288; // 18rem (w-72)
	const VIEWPORT_MARGIN = 16;

	let open = $state(false);
	let el = $state<HTMLElement>();
	let triggerEl = $state<HTMLElement>();
	let triggerCenter = $state(12);
	let maxWidth = $state(PREFERRED_WIDTH);
	const popoverId = `info-popover-${Math.random().toString(36).slice(2)}`;

	function updateGeometry() {
		if (!triggerEl || !el) return;
		triggerCenter = triggerEl.offsetWidth / 2;
		const rect = el.getBoundingClientRect();
		const isEnd = placement === 'bottom-end' || placement === 'top-end';
		const available = isEnd ? rect.right - VIEWPORT_MARGIN : window.innerWidth - rect.left - VIEWPORT_MARGIN;
		maxWidth = Math.max(160, Math.min(PREFERRED_WIDTH, available));
	}

	$effect(() => {
		if (open) updateGeometry();
	});

	const placementClass: Record<Placement, string> = {
		'bottom-start': 'top-full left-0 mt-2',
		'bottom-end': 'top-full right-0 mt-2',
		'top-start': 'bottom-full left-0 mb-2',
		'top-end': 'bottom-full right-0 mb-2'
	};

	const arrowVertical: Record<Placement, string> = {
		'bottom-start': '-top-[5px]',
		'bottom-end': '-top-[5px]',
		'top-start': '-bottom-[5px]',
		'top-end': '-bottom-[5px]'
	};

	const arrowSide: Record<Placement, 'left' | 'right'> = {
		'bottom-start': 'left',
		'bottom-end': 'right',
		'top-start': 'left',
		'top-end': 'right'
	};
</script>

<svelte:window
	onclick={(e) => {
		if (open && el && !el.contains(e.target as Node)) open = false;
	}}
	onkeydown={(e) => {
		if (e.key === 'Escape') open = false;
	}}
	onresize={() => {
		if (open) updateGeometry();
	}}
/>

<div bind:this={el} class="relative inline-flex {className}">
	<button
		bind:this={triggerEl}
		type="button"
		aria-label={label}
		aria-expanded={open}
		aria-controls={popoverId}
		class="btn btn-ghost btn-circle btn-xs text-base-content/60 hover:text-base-content"
		onclick={() => (open = !open)}
	>
		<IconInfo class="h-4 w-4 {iconClass}" />
	</button>

	{#if open}
		<div
			id={popoverId}
			role="dialog"
			class="absolute z-50 whitespace-normal rounded-field bg-neutral py-1 px-2 text-sm font-normal text-neutral-content {placementClass[
				placement
			]}"
			style="width: {PREFERRED_WIDTH}px; max-width: {maxWidth}px"
		>
			<span
				class="absolute h-2.5 w-2.5 rotate-45 bg-neutral {arrowVertical[placement]}"
				style="{arrowSide[placement]}: {triggerCenter - 5}px"
			></span>
			{@render children?.()}
		</div>
	{/if}
</div>
