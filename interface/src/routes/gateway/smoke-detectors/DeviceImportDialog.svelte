<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { untrack } from 'svelte';
	import ProgressDialog from '$lib/components/ProgressDialog.svelte';
	import Cancel from '~icons/tabler/x';
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
		 *  optional error text on failure. The dialog calls onProgress to drive its UI. */
		task: (onProgress: (p: ImportProgress) => void) => Promise<{ ok: boolean; error?: string }>;
		onSuccess?: () => void;
		onError?: (errorDetail?: string) => void;
	}

	let { isOpen, title = 'Importing Devices', totalDevices, task, onSuccess, onError }: Props = $props();

	let phase = $state<'pending' | 'progress' | 'success' | 'error'>('pending');
	let progress = $state<number | undefined>(undefined);
	let message = $state('Preparing import...');
	let stepLabel = $state<string | undefined>(undefined);
	let errorDetails = $state<string | undefined>(undefined);
	let aborted = false;

	function handleProgress(p: ImportProgress) {
		if (aborted) return;
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
				stepLabel = `Chunk ${p.chunkIndex} of ${p.totalChunks}`;
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
		aborted = false;
		phase = 'pending';
		progress = undefined;
		message = totalDevices === 0 ? 'Clearing device list...' : 'Preparing import...';
		stepLabel = undefined;
		errorDetails = undefined;

		try {
			const result = await task(handleProgress);
			if (aborted) return;
			if (result.ok) {
				phase = 'success';
				message =
					totalDevices === 0
						? 'Device list cleared.'
						: `${totalDevices} ${totalDevices === 1 ? 'device' : 'devices'} imported.`;
				stepLabel = undefined;
				// Brief delay so the user sees the success state, then close + notify caller.
				setTimeout(() => {
					if (aborted) return;
					modals.close();
					onSuccess?.();
				}, 1200);
			} else {
				phase = 'error';
				message = 'Import failed.';
				errorDetails = result.error;
			}
		} catch (e) {
			if (aborted) return;
			phase = 'error';
			message = 'Import failed.';
			errorDetails = e instanceof Error ? e.message : String(e);
		}
	}

	function handleCancel() {
		aborted = true;
		modals.close();
		onError?.('Aborted by user');
	}

	function handleRetry() {
		runTask();
	}

	function handleErrorClose() {
		modals.close();
		onError?.(errorDetails);
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
		class: 'btn-ghost',
		handler: handleCancel
	}}
	errorButtons={[
		{ label: 'Close', icon: Cancel, class: 'btn-ghost', handler: handleErrorClose },
		{ label: 'Retry', icon: Refresh, class: 'btn-primary', handler: handleRetry }
	]}
/>
