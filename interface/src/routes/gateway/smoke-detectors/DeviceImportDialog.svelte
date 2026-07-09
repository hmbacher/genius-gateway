<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { untrack } from 'svelte';
	import ProgressDialog from '$lib/components/ProgressDialog.svelte';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Refresh from '~icons/tabler/refresh';

	export type ImportProgress =
		| { phase: 'starting' }
		| {
				phase: 'uploading';
				chunkIndex: number;
				totalChunks: number;
				devicesSent: number;
				devicesTotal: number;
		  }
		| { phase: 'committing' };

	interface Props extends ModalProps {
		title?: string;
		totalDevices: number;
		/** Runs the chunked-import task. Resolves with ok=true on success, ok=false with
		 *  optional error text on failure, ok=false + aborted=true if the signal fires.
		 *  The dialog calls onProgress to drive its UI, and passes its AbortSignal so the
		 *  task can cancel in-flight fetches and clean up the server-side session slot. */
		task: (
			onProgress: (p: ImportProgress) => void,
			signal: AbortSignal
		) => Promise<{ ok: boolean; error?: string; aborted?: boolean }>;
		onSuccess?: () => void;
		onAborted?: () => void;
		onError?: (errorDetail?: string) => void;
	}

	let {
		isOpen,
		title = 'Importing Devices',
		totalDevices,
		task,
		onSuccess,
		onAborted,
		onError
	}: Props = $props();

	let phase = $state<'pending' | 'progress' | 'success' | 'error' | 'aborted'>('pending');
	let progress = $state<number | undefined>(undefined);
	let message = $state('Preparing import...');
	let stepLabel = $state<string | undefined>(undefined);
	let errorDetails = $state<string | undefined>(undefined);
	let controller: AbortController | undefined;
	let userAborting = $state(false);

	function handleProgress(p: ImportProgress) {
		// Suppress progress updates once the user has clicked Abort - otherwise a
		// late "uploading" event arriving while we're already showing "Aborting…"
		// would flip the message back.
		if (userAborting) return;
		switch (p.phase) {
			case 'starting':
				phase = 'pending';
				message = 'Starting import session...';
				stepLabel = undefined;
				progress = undefined;
				break;
			case 'uploading':
				phase = 'progress';
				progress = Math.round((p.devicesSent / p.devicesTotal) * 100);
				message = `Uploading ${p.devicesSent} of ${p.devicesTotal} devices`;
				stepLabel = undefined;
				break;
			case 'committing':
				phase = 'progress';
				progress = 100;
				message = 'Finalizing on device...';
				stepLabel = undefined;
				break;
		}
	}

	async function runTask() {
		controller = new AbortController();
		userAborting = false;
		phase = 'pending';
		progress = undefined;
		message = totalDevices === 0 ? 'Clearing device list...' : 'Preparing import...';
		stepLabel = undefined;
		errorDetails = undefined;

		try {
			const result = await task(handleProgress, controller.signal);
			if (result.ok) {
				phase = 'success';
				message =
					totalDevices === 0
						? 'Device list cleared.'
						: `${totalDevices} ${totalDevices === 1 ? 'device' : 'devices'} imported.`;
				stepLabel = undefined;
				// User confirms by clicking Close; onSuccess fires from that handler.
			} else if (result.aborted) {
				phase = 'aborted';
				message = 'Import aborted.';
				stepLabel = undefined;
				progress = undefined;
			} else {
				phase = 'error';
				message = 'Import failed.';
				errorDetails = result.error;
			}
		} catch (e) {
			phase = 'error';
			message = 'Import failed.';
			errorDetails = e instanceof Error ? e.message : String(e);
		}
	}

	function handleCancel() {
		if (userAborting || !controller) return; // already aborting / completed
		userAborting = true;
		// Show indeterminate spinner while the task unwinds and cleanupSession runs.
		// runTask's await then resolves with { ok: false, aborted: true } and the
		// resolution branch above closes the dialog.
		phase = 'progress';
		progress = undefined;
		message = 'Aborting…';
		stepLabel = undefined;
		controller.abort();
	}

	function handleRetry() {
		runTask();
	}

	function handleErrorClose() {
		modals.close();
		onError?.(errorDetails);
	}

	function handleSuccessClose() {
		modals.close();
		onSuccess?.();
	}

	function handleAbortedClose() {
		modals.close();
		onAborted?.();
	}

	// Kick off as soon as the dialog mounts. untrack() guards against accidental
	// re-runs if runTask's reads of reactive state (e.g. totalDevices) ever change.
	$effect(() => {
		untrack(() => runTask());
	});
</script>

<ProgressDialog
	{isOpen}
	{title}
	{phase}
	{progress}
	{message}
	{stepLabel}
	{errorDetails}
	cancelButton={{
		label: 'Abort',
		icon: Cancel,
		class: 'btn-primary',
		handler: handleCancel,
		disabled: userAborting
	}}
	successButton={{
		label: 'Close',
		icon: Check,
		class: 'btn-primary',
		handler: handleSuccessClose
	}}
	abortedButton={{
		label: 'Close',
		icon: Check,
		class: 'btn-primary',
		handler: handleAbortedClose
	}}
	errorButtons={[
		{ label: 'Close', icon: Cancel, class: 'btn-ghost', handler: handleErrorClose },
		{ label: 'Retry', icon: Refresh, class: 'btn-primary', handler: handleRetry }
	]}
/>
