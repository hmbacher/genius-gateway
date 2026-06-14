<script lang="ts">
	import IconRevert from '~icons/tabler/arrow-back-up';

	let show = $state(false);
	let type = $derived(show ? 'text' : 'password');

	interface Props {
		value?: string;
		id?: string;
		/**
		 * Committed baseline for dirty tracking. When provided, dirty state and the left-accent
		 * shadow are computed internally from `value !== baseline`, so the revert button appears
		 * immediately during typing without depending on the parent re-rendering first.
		 */
		baseline?: string;
		/** Reset the field back to its saved value. */
		onrevert?: () => void;
		/** Extra classes applied to the input (non-dirty styling). */
		class?: string;
	}

	let {
		value = $bindable('') as string,
		id = '' as string,
		baseline,
		onrevert,
		class: className = ''
	}: Props = $props();

	const dirty = $derived(baseline !== undefined && value !== baseline);
</script>

<div class="relative">
	{#if dirty}
		<div class="pointer-events-none absolute inset-y-0 left-0 z-10 w-1 rounded-l-[var(--radius-field)] bg-red-300"></div>
	{/if}
	<input {type} class="input input-bordered w-full pr-16 {className}" bind:value {id} />
	<div class="absolute inset-y-0 right-0 z-10 flex items-center gap-2 pr-3">
		<!-- svelte-ignore a11y_click_events_have_key_events -->
		<svg
			xmlns="http://www.w3.org/2000/svg"
			class="text-base-content/50 h-6 {show ? 'block' : 'hidden'}"
			onclick={() => (show = false)}
			role="button"
			aria-label="Hide password"
			tabindex="0"
			width="40"
			height="40"
			viewBox="0 0 24 24"
			stroke-width="2"
			stroke="currentColor"
			fill="none"
			stroke-linecap="round"
			stroke-linejoin="round"
		>
			<path stroke="none" d="M0 0h24v24H0z" fill="none" />
			<path d="M10.585 10.587a2 2 0 0 0 2.829 2.828" />
			<path
				d="M16.681 16.673a8.717 8.717 0 0 1 -4.681 1.327c-3.6 0 -6.6 -2 -9 -6c1.272 -2.12 2.712 -3.678 4.32 -4.674m2.86 -1.146a9.055 9.055 0 0 1 1.82 -.18c3.6 0 6.6 2 9 6c-.666 1.11 -1.379 2.067 -2.138 2.87"
			/>
			<path d="M3 3l18 18" />
		</svg>

		<!-- svelte-ignore a11y_click_events_have_key_events -->
		<svg
			xmlns="http://www.w3.org/2000/svg"
			class="text-base-content/50 h-6 {show ? 'hidden' : 'block'}"
			onclick={() => (show = true)}
			role="button"
			aria-label="Show password"
			tabindex="0"
			width="40"
			height="40"
			viewBox="0 0 24 24"
			stroke-width="2"
			stroke="currentColor"
			fill="none"
			stroke-linecap="round"
			stroke-linejoin="round"
		>
			<path stroke="none" d="M0 0h24v24H0z" fill="none" />
			<path d="M10 12a2 2 0 1 0 4 0a2 2 0 0 0 -4 0" />
			<path d="M21 12c-2.4 4 -5.4 6 -9 6c-3.6 0 -6.6 -2 -9 -6c2.4 -4 5.4 -6 9 -6c3.6 0 6.6 2 9 6" />
		</svg>
		{#if dirty}
			<!-- svelte-ignore a11y_consider_explicit_label -->
			<button
				type="button"
				class="text-error tooltip tooltip-left flex cursor-pointer items-center"
				data-tip="Revert change"
				aria-label="Revert change"
				onclick={() => onrevert?.()}
			>
				<IconRevert class="h-5 w-5" />
			</button>
		{/if}
	</div>
</div>
