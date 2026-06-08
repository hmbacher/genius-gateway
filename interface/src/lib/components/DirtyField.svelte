<script lang="ts">
	import DirtyMarker from './DirtyMarker.svelte';

	interface Props {
		/** Whether the wrapped field differs from its saved value. */
		dirty: boolean;
		/** Reset the field back to its saved value. */
		onrevert: () => void;
		/** Accessible label / tooltip for the revert control. */
		title?: string;
		/** Extra classes for the relative wrapper (e.g. the red left-accent from fieldClass). */
		class?: string;
		children: import('svelte').Snippet;
	}

	let { dirty, onrevert, title = 'Revert change', class: className = '', children }: Props = $props();
</script>

<!--
  Wraps a single input so the per-field revert button sits *inside* the field on the right
  (like InputPassword's eye). The wrapped input should reserve right padding (e.g. pr-10) so its
  content doesn't run under the button.
-->
<div class="relative {className}">
	{#if dirty}
		<div class="pointer-events-none absolute inset-y-0 left-0 z-10 w-1 rounded-l-[var(--radius-field)] bg-red-300"></div>
		<div class="absolute inset-y-0 right-0 z-10 flex items-center pr-3">
			<DirtyMarker {dirty} {onrevert} {title} />
		</div>
	{/if}
	{@render children()}
</div>
