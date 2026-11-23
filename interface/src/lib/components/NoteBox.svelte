<script lang="ts">
	import type { Component, Snippet } from 'svelte';
	import IconInfo from '~icons/tabler/info-circle';
	import IconWarning from '~icons/tabler/alert-triangle';

	/**
	 * A boxed note with an icon, for the outcome of an operation.
	 *
	 * - `neutral`: what was done, nothing to act on
	 * - `info`: something the user should know about
	 * - `warning`: something is outstanding or did not work as expected
	 */
	type Tone = 'neutral' | 'info' | 'warning';

	interface Props {
		tone?: Tone;
		/** Icon left of the text. Defaults to an info circle, or a triangle for a warning. */
		icon?: Component<{ class?: string }>;
		children: Snippet;
	}

	let { tone = 'neutral', icon, children }: Props = $props();

	const toneClass: Record<Tone, string> = {
		neutral: '',
		info: 'alert-info alert-soft',
		warning: 'alert-warning'
	};

	const Icon = $derived(icon ?? (tone === 'warning' ? IconWarning : IconInfo));
</script>

<div class="alert {toneClass[tone]} items-start gap-2 p-3 text-left" role="note">
	<Icon class="mt-0.5 h-5 w-5 shrink-0" />
	<span class="text-sm">{@render children()}</span>
</div>
