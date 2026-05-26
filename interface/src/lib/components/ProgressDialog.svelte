<script lang="ts">
	import { modals, type ModalProps, onBeforeClose } from 'svelte-modals';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import type { Component } from 'svelte';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import AlertCircle from '~icons/tabler/alert-circle';
	import Loader from '~icons/tabler/loader-2';

	type IconComponent = Component<{ class?: string }>;

	type Phase = 'pending' | 'progress' | 'success' | 'error';

	type ActionButton = {
		label: string;
		icon?: IconComponent;
		/** DaisyUI button class, e.g. 'btn-primary', 'btn-error', 'btn-ghost'. */
		class?: string;
		handler: () => void;
		/** Disable the button (greys it out, ignores clicks). */
		disabled?: boolean;
	};

	interface Props extends ModalProps {
		title: string;
		phase: Phase;
		/** 0-100. Omit for an indeterminate spinner. */
		progress?: number;
		/** Primary status line shown below the progress widget. */
		message: string;
		/** Optional secondary line, e.g. "Chunk 8 of 17". */
		stepLabel?: string;
		/** Detail shown beneath the error message in the 'error' phase. */
		errorDetails?: string;
		/** Visible while phase === 'progress' or 'pending'. */
		cancelButton?: ActionButton & { disabledDuringProgress?: boolean };
		/** Visible while phase === 'success'. Optionally auto-fires after N seconds. */
		successButton?: ActionButton & { autoTriggerAfterSeconds?: number };
		/** Visible while phase === 'error'. Order is preserved left-to-right. */
		errorButtons?: ActionButton[];
		/** Block backdrop-click / Esc dismissal while task runs. Default true. */
		blockCloseDuringProgress?: boolean;
	}

	let {
		isOpen,
		title,
		phase,
		progress,
		message,
		stepLabel,
		errorDetails,
		cancelButton,
		successButton,
		errorButtons,
		blockCloseDuringProgress = true
	}: Props = $props();

	const titleId = `progress-dialog-title-${Math.random().toString(36).slice(2)}`;

	let running = $derived(phase === 'pending' || phase === 'progress');

	// Optional auto-trigger for the success button (firmware-update style countdown).
	let countdown: number = $state(0);
	let countdownTimer: number | undefined = $state();

	$effect(() => {
		if (phase === 'success' && successButton?.autoTriggerAfterSeconds) {
			countdown = successButton.autoTriggerAfterSeconds;
			countdownTimer = setInterval(() => {
				countdown--;
				if (countdown <= 0) {
					clearInterval(countdownTimer);
					successButton?.handler();
				}
			}, 1000) as unknown as number;
			return () => {
				if (countdownTimer) clearInterval(countdownTimer);
			};
		}
	});

	onBeforeClose(() => {
		if (running && blockCloseDuringProgress) return false;
		return true;
	});
</script>

{#if isOpen}
	<div
		role="dialog"
		aria-modal="true"
		aria-labelledby={titleId}
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center backdrop-blur-sm"
		transition:fly={{ y: 50, duration: 300 }}
		use:focusTrap
	>
		<div
			class="bg-base-100 shadow-2xl rounded-2xl pointer-events-auto flex max-h-full w-full max-w-md flex-col border border-base-300"
		>
			<!-- Header -->
			<div class="flex items-center justify-between p-6 pb-4">
				<h2 id={titleId} class="text-base-content text-xl font-bold">{title}</h2>
			</div>

			<!-- Progress Content -->
			<div class="flex flex-col items-center justify-center px-6 py-8 space-y-6">
				{#if phase === 'progress' || phase === 'pending'}
					<div class="relative">
						{#if phase === 'progress' && progress !== undefined && progress > 0}
							<div
								class="radial-progress text-primary"
								style="--value:{progress}; --size:10rem; --thickness: 0.5rem;"
								role="progressbar"
								aria-valuenow={progress}
								aria-valuemin="0"
								aria-valuemax="100"
							>
								<span class="text-3xl font-bold">{progress}%</span>
							</div>
						{:else}
							<!-- Indeterminate spinner matching radial size -->
							<div class="flex items-center justify-center w-40 h-40">
								<Loader class="text-primary h-16 w-16 animate-spin stroke-2" />
							</div>
						{/if}
					</div>
				{:else if phase === 'success'}
					<div class="flex items-center justify-center w-24 h-24 rounded-full bg-success/10">
						<Check class="h-16 w-16 text-success" />
					</div>
				{:else if phase === 'error'}
					<div class="flex items-center justify-center w-24 h-24 rounded-full bg-error/10">
						<AlertCircle class="h-16 w-16 text-error" />
					</div>
				{/if}

				<!-- Status messages -->
				<div class="space-y-1 text-center">
					<p class="text-lg {phase === 'error' ? 'text-error' : 'text-base-content/70'}">
						{message}
					</p>
					{#if stepLabel && running}
						<p class="text-sm text-base-content/50">{stepLabel}</p>
					{/if}
					{#if phase === 'error' && errorDetails}
						<p class="text-sm text-error/70 mt-2">{errorDetails}</p>
					{/if}
					{#if phase === 'success' && successButton?.autoTriggerAfterSeconds && countdown > 0}
						<p class="text-sm text-base-content/50 mt-2">
							{successButton.label} in {countdown}
							{countdown === 1 ? 'second' : 'seconds'}...
						</p>
					{/if}
				</div>
			</div>

			<!-- Footer -->
			<div class="flex justify-end gap-2 p-6 pt-4 border-t border-base-300">
				{#if running && cancelButton}
					{@const CancelIcon = cancelButton.icon ?? Cancel}
					<button
						class="btn btn-sm {cancelButton.class ?? 'btn-ghost'}"
						disabled={cancelButton.disabled ||
							(phase === 'progress' && cancelButton.disabledDuringProgress)}
						onclick={cancelButton.handler}
					>
						<CancelIcon class="h-4 w-4" />
						{cancelButton.label}
					</button>
				{/if}
				{#if phase === 'success' && successButton}
					{@const SuccessIcon = successButton.icon ?? Check}
					<button
						class="btn btn-sm {successButton.class ?? 'btn-primary'}"
						onclick={() => {
							if (countdownTimer) clearInterval(countdownTimer);
							successButton.handler();
						}}
					>
						<SuccessIcon class="h-4 w-4" />
						{successButton.label}
					</button>
				{/if}
				{#if phase === 'error' && errorButtons}
					{#each errorButtons as btn}
						{@const Icon = btn.icon}
						<button class="btn btn-sm {btn.class ?? 'btn-ghost'}" onclick={btn.handler}>
							{#if Icon}<Icon class="h-4 w-4" />{/if}
							{btn.label}
						</button>
					{/each}
				{/if}
				{#if phase === 'success' && !successButton}
					<button class="btn btn-sm btn-primary" onclick={() => modals.close()}>
						<Check class="h-4 w-4" />
						Close
					</button>
				{/if}
			</div>
		</div>
	</div>
{/if}
