<script lang="ts">
	import { modals, onBeforeClose, type ModalProps } from 'svelte-modals';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import {
		AcousticDetectionSession,
		type TunerState,
		type TunerData
	} from '$lib/audio/tuner-pipeline';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import AlertCircle from '~icons/tabler/alert-circle';
	import Loader from '~icons/tabler/loader-2';
	import Microphone from '~icons/tabler/microphone';
	import Refresh from '~icons/tabler/refresh';

	interface Props extends ModalProps {
		title?: string;
		onSuccess?: (data: TunerData) => void;
	}

	let { isOpen, title = 'Acoustic Detection', onSuccess }: Props = $props();

	const titleId = `acoustic-dialog-title-${Math.random().toString(36).slice(2)}`;

	let tunerState: TunerState = $state<TunerState>('idle');
	let errorMessage: string = $state('');
	let tunerData: TunerData | null = $state(null);
	let audioLevel: number = $state(0);

	let session: AcousticDetectionSession | null = $state(null);

	let active = $derived(
		tunerState === 'waiting' || tunerState === 'synced' || tunerState === 'decoding'
	);

	let statusMessage = $derived.by(() => {
		switch (tunerState) {
			case 'idle':
				return 'Initializing…';
			case 'waiting':
				return 'Waiting for audio signal…';
			case 'synced':
				return 'Audio signal detected!';
			case 'decoding':
				return 'Receiving data…';
			case 'success':
				return 'Data received successfully!';
			case 'error':
				return errorMessage || 'Detection failed.';
			default:
				return '';
		}
	});

	$effect(() => {
		if (isOpen && tunerState === 'idle') {
			startDetection();
		}
	});

	async function startDetection() {
		tunerData = null;
		errorMessage = '';

		session = new AcousticDetectionSession(
			{
				onLog: () => {},
				onSync: (_info) => {},
				onData: (data) => {
					tunerData = data;
				},
				onError: (msg) => {
					errorMessage = msg;
				},
				onLevelUpdate: (level) => {
					audioLevel = level;
				}
			},
			(state) => {
				tunerState = state;
			}
		);

		await session.start();
	}

	function stopDetection() {
		if (session) {
			session.stop();
			session = null;
		}
	}

	async function retry() {
		stopDetection();
		// Reset to 'idle' so the $effect above picks it up and reruns
		// startDetection().
		tunerState = 'idle';
	}

	onBeforeClose(() => {
		if (active) {
			stopDetection();
		}
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

			<!-- Content -->
			<div class="flex flex-col items-center justify-center px-6 py-8 space-y-6">
				{#if tunerState === 'waiting'}
					<!-- Waiting for sync: pulsing microphone with spinning ring -->
					<div class="relative flex flex-col items-center justify-center gap-2 w-40 h-40">
						<!-- Spinning outer ring -->
						<div
							class="absolute inset-0 rounded-full border-4 border-primary/20 border-t-primary animate-spin motion-reduce:animate-none"
						></div>
						<!-- Microphone icon -->
						<Microphone class="h-14 w-14 text-primary animate-pulse motion-reduce:animate-none" />
						<!-- Level bar inside spinner -->
						<div class="w-20 h-2 bg-base-300/60 rounded-full overflow-hidden">
							<div
								class="h-full bg-primary transition-all duration-100 rounded-full"
								style="width: {Math.min(audioLevel * 100, 100)}%"
								aria-hidden="true"
							></div>
						</div>
					</div>
				{:else if tunerState === 'synced'}
					<!-- Sync detected: brief burst animation -->
					<div class="relative flex items-center justify-center w-40 h-40">
						<div
							class="absolute inset-0 rounded-full bg-success/20 animate-ping motion-reduce:animate-none"
						></div>
						<div class="absolute inset-4 rounded-full bg-success/10"></div>
						<Check class="h-16 w-16 text-success" />
					</div>
				{:else if tunerState === 'decoding'}
					<!-- Decoding: wave animation -->
					<div class="relative flex items-center justify-center w-40 h-40">
						<!-- Animated concentric rings -->
						<div
							class="absolute inset-0 rounded-full border-2 border-primary/40 animate-ping motion-reduce:animate-none"
							style="animation-duration: 2s"
						></div>
						<div
							class="absolute inset-4 rounded-full border-2 border-primary/30 animate-ping motion-reduce:animate-none"
							style="animation-duration: 2s; animation-delay: 0.5s"
						></div>
						<div
							class="absolute inset-8 rounded-full border-2 border-primary/20 animate-ping motion-reduce:animate-none"
							style="animation-duration: 2s; animation-delay: 1s"
						></div>
						<Loader
							class="h-16 w-16 text-primary animate-spin motion-reduce:animate-none stroke-2"
						/>
					</div>
				{:else if tunerState === 'success'}
					<!-- Success -->
					<div class="flex items-center justify-center w-24 h-24 rounded-full bg-success/10">
						<Check class="h-16 w-16 text-success" />
					</div>
				{:else if tunerState === 'error'}
					<!-- Error -->
					<div class="flex items-center justify-center w-24 h-24 rounded-full bg-error/10">
						<AlertCircle class="h-16 w-16 text-error" />
					</div>
				{/if}

				<!-- Status message — announced via aria-live so screen readers get
				     state changes (waiting → synced → decoding → success/error). -->
				<p
					role="status"
					aria-live="polite"
					aria-atomic="true"
					class="text-center text-lg {tunerState === 'error'
						? 'text-error'
						: tunerState === 'success'
							? 'text-success'
							: 'text-base-content/70'}"
				>
					{statusMessage}
				</p>

				{#if tunerState === 'success' && tunerData}
					<p class="text-sm text-base-content/50 text-center">
						Serial: {tunerData.serialNumber} — {tunerData.product}
					</p>
				{/if}
			</div>

			<!-- Footer -->
			<div class="flex justify-end gap-2 p-6 pt-4 border-t border-base-300">
				{#if tunerState === 'error'}
					<button class="btn btn-sm btn-ghost" onclick={retry} aria-label="Retry detection">
						<Refresh class="h-5 w-5" />
						Retry
					</button>
				{/if}
				<button
					class="btn btn-sm {tunerState === 'success'
						? 'btn-success'
						: tunerState === 'error'
							? 'btn-error'
							: 'btn-primary'}"
					onclick={() => {
						stopDetection();
						modals.close();
						if (tunerState === 'success' && tunerData && onSuccess) {
							onSuccess(tunerData);
						}
					}}
				>
					{#if tunerState === 'success'}
						<Check class="h-5 w-5" />
						{onSuccess ? 'Continue' : 'Done'}
					{:else if tunerState === 'error'}
						<Cancel class="h-5 w-5" />
						Close
					{:else}
						<Cancel class="h-5 w-5" />
						Cancel
					{/if}
				</button>
			</div>
		</div>
	</div>
{/if}
