<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { onDestroy, untrack } from 'svelte';
	import ProgressDialog from '$lib/components/ProgressDialog.svelte';
	import { socket } from '$lib/stores/socket';
	import { setImportTracking } from '$lib/stores/haEntityRename.svelte';
	import type { DeviceImportEvent, HaEntityRenameEvent } from '$lib/types/models';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Refresh from '~icons/tabler/refresh';
	import Load from '~icons/tabler/folder-open';

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
		/** Whether the Home Assistant integration is enabled, i.e. whether the import has a
		 *  third step. Only used for the step count until the gateway says so itself. */
		haEnabled?: boolean;
		/** Extra sentence for the final message, e.g. that the file was migrated. */
		successNote?: string;
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
		haEnabled = false,
		successNote,
		task,
		onSuccess,
		onAborted,
		onError,
		...modalProps
	}: Props = $props();

	const IMPORT_EVENT = 'device-import';
	const RENAME_EVENT = 'ha-entity-rename';
	/** How long to wait for the next sign of life from the gateway once the upload is done. */
	const GATEWAY_SILENCE_MS = 90_000;

	let phase = $state<'pending' | 'progress' | 'success' | 'error' | 'aborted'>('pending');
	let progress = $state<number | undefined>(undefined);
	let message = $state('Preparing import...');
	let stepLabel = $state<string | undefined>(undefined);
	let errorDetails = $state<string | undefined>(undefined);
	let successDetails = $state<string | undefined>(undefined);
	let controller: AbortController | undefined;
	let userAborting = $state(false);

	// ── After the upload: what the gateway reports ──────────────────────────────
	// The commit only hands the list over. Writing it to flash and bringing Home Assistant
	// in line happen afterwards on the gateway, which reports both with WebSocket events.
	// They can arrive in any order: the entity-ID rename runs in a task of its own and may
	// finish before the import reports "done".

	/** True once the gateway accepted the list; from then on the import cannot be aborted. */
	let committed = $state(false);
	/** False after a failure that a second attempt cannot fix (the list is already handed over). */
	let canRetry = $state(true);
	let finished = false;
	let hasHaStep = $state(untrack(() => haEnabled));
	let saved = false;
	let done: 'disabled' | 'offline' | 'renaming' | 'sent' | undefined;
	let renameEnd: 'sent' | 'deferred' | 'failed' | undefined;
	let watchdog: ReturnType<typeof setTimeout> | undefined;
	let listening = false;

	const totalSteps = $derived(hasHaStep ? 3 : 2);

	function showStep(step: number, text: string) {
		phase = 'progress';
		message = text;
		stepLabel = `Step ${step} of ${totalSteps}`;
	}

	function importedText(): string {
		return totalDevices === 0
			? 'Device list cleared.'
			: `${totalDevices} ${totalDevices === 1 ? 'device' : 'devices'} imported.`;
	}

	function startListening() {
		if (listening) return;
		listening = true;
		socket.on<DeviceImportEvent>(IMPORT_EVENT, handleImportEvent);
		socket.on<HaEntityRenameEvent>(RENAME_EVENT, handleRenameEvent);
	}

	function stopListening() {
		if (watchdog !== undefined) clearTimeout(watchdog);
		watchdog = undefined;
		// From here on a background rename that cannot be sent is reported by the notice of
		// the rename store again.
		setImportTracking(false);
		if (!listening) return;
		listening = false;
		socket.off(IMPORT_EVENT, handleImportEvent);
		socket.off(RENAME_EVENT, handleRenameEvent);
	}

	function armWatchdog() {
		if (watchdog !== undefined) clearTimeout(watchdog);
		watchdog = setTimeout(handleGatewaySilent, GATEWAY_SILENCE_MS);
	}

	function handleImportEvent(event: DeviceImportEvent) {
		// Recorded even before the commit request has returned - the gateway may be faster.
		if (finished) return;
		if (event.state === 'saved') {
			saved = true;
			hasHaStep = event.ha;
		} else if (event.state === 'done') {
			saved = true; // "done" implies it, should "saved" have been missed
			done = event.ha;
		}
		if (!committed) return;
		armWatchdog();
		evaluate();
	}

	function handleRenameEvent(event: HaEntityRenameEvent) {
		// Only the cycle the import caused. An announced cycle belongs to somebody's edit.
		if (finished || !event.background) return;
		if (event.state === 'sent' || event.state === 'deferred' || event.state === 'failed') {
			renameEnd = event.state;
		}
		if (!committed) return;
		armWatchdog();
		evaluate();
	}

	/** Show where the gateway is, or finish once it has reported everything. */
	function evaluate() {
		if (!committed || finished) return;

		if (done !== undefined && (done !== 'renaming' || renameEnd !== undefined)) {
			const haSent =
				(done === 'sent' && renameEnd !== 'deferred' && renameEnd !== 'failed') ||
				(done === 'renaming' && renameEnd === 'sent');
			finishSuccess(done === 'disabled' ? 'none' : haSent ? 'sent' : 'pending');
			return;
		}

		progress = undefined; // spinner: the gateway reports steps, not percentages
		if (saved && hasHaStep) showStep(3, 'Updating Home Assistant...');
		else showStep(2, 'Saving on the gateway...');
	}

	function finishSuccess(ha: 'none' | 'sent' | 'pending' | 'unknown') {
		finished = true;
		stopListening();
		const haText = {
			none: '',
			// Home Assistant does not acknowledge discovery messages: "sent" is all there is.
			sent: 'The changes were sent to Home Assistant.',
			pending:
				'Home Assistant could not be reached over MQTT - the gateway sends the changes on the next MQTT connect.',
			unknown:
				'The gateway did not report whether Home Assistant was updated - check the entities there.'
		}[ha];
		phase = 'success';
		message = importedText();
		stepLabel = undefined;
		successDetails = [successNote, haText].filter((text) => !!text).join(' ') || undefined;
		// User confirms by clicking Close; onSuccess fires from that handler.
	}

	/** The gateway stopped reporting (connection lost, restart). Say what is known. */
	function handleGatewaySilent() {
		watchdog = undefined;
		if (!committed || finished) return;
		if (saved) {
			finishSuccess('unknown');
			return;
		}
		// Never claim an import the gateway has not confirmed as saved.
		finished = true;
		stopListening();
		canRetry = false;
		phase = 'error';
		message = 'Import not confirmed';
		stepLabel = undefined;
		errorDetails =
			'The gateway accepted the devices but did not report that they were saved. Reload the page and check the device list.';
	}

	function handleProgress(p: ImportProgress) {
		// Suppress progress updates once the user has clicked Abort - otherwise a
		// late "uploading" event arriving while we're already showing "Aborting…"
		// would flip the message back.
		if (userAborting) return;
		switch (p.phase) {
			case 'starting':
				phase = 'pending';
				message = 'Starting import session...';
				stepLabel = `Step 1 of ${totalSteps}`;
				progress = undefined;
				break;
			case 'uploading':
				phase = 'progress';
				progress = Math.round((p.devicesSent / p.devicesTotal) * 100);
				message = `Uploading ${p.devicesSent} of ${p.devicesTotal} devices`;
				stepLabel = `Step 1 of ${totalSteps}`;
				break;
			case 'committing':
				// Whatever the gateway reports from now on belongs to this import.
				saved = false;
				done = undefined;
				renameEnd = undefined;
				setImportTracking(true);
				startListening();
				phase = 'progress';
				progress = 100;
				message = 'Handing over to the gateway...';
				stepLabel = `Step 1 of ${totalSteps}`;
				break;
		}
	}

	async function runTask() {
		controller = new AbortController();
		userAborting = false;
		committed = false;
		finished = false;
		outcomeReported = false;
		canRetry = true;
		phase = 'pending';
		progress = undefined;
		message = totalDevices === 0 ? 'Clearing device list...' : 'Preparing import...';
		stepLabel = undefined;
		errorDetails = undefined;
		successDetails = undefined;

		try {
			const result = await task(handleProgress, controller.signal);
			if (result.ok) {
				// Accepted, not yet saved: follow the gateway through the remaining steps.
				committed = true;
				armWatchdog();
				evaluate();
			} else if (result.aborted) {
				stopListening();
				phase = 'aborted';
				message = 'Import aborted.';
				stepLabel = undefined;
				progress = undefined;
			} else {
				stopListening();
				phase = 'error';
				message = 'Import failed.';
				errorDetails = result.error;
			}
		} catch (e) {
			stopListening();
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

	let outcomeReported = false;

	/** Tell the page once that the list was handed over, however the dialog is left. */
	function reportHandedOver() {
		if (outcomeReported) return;
		outcomeReported = true;
		onSuccess?.();
	}

	/** Stop watching. The gateway finishes the import on its own. */
	function handleHide() {
		finished = true;
		stopListening();
		modals.close();
		reportHandedOver();
	}

	function handleRetry() {
		runTask();
	}

	function handleErrorClose() {
		outcomeReported = true;
		modals.close();
		onError?.(errorDetails);
	}

	function handleSuccessClose() {
		modals.close();
		reportHandedOver();
	}

	function handleAbortedClose() {
		modals.close();
		onAborted?.();
	}

	onDestroy(() => {
		stopListening();
		// Left with Esc or a click beside the dialog while the gateway was still working
		if (committed) reportHandedOver();
	});

	// Kick off as soon as the dialog mounts. untrack() guards against accidental
	// re-runs if runTask's reads of reactive state (e.g. totalDevices) ever change.
	$effect(() => {
		untrack(() => runTask());
	});
</script>

<ProgressDialog
	{isOpen}
	{...modalProps}
	{title}
	titleIcon={Load}
	{phase}
	{progress}
	{message}
	{stepLabel}
	{errorDetails}
	{successDetails}
	blockCloseDuringProgress={!committed}
	cancelButton={committed
		? { label: 'Hide', icon: Cancel, class: 'btn-ghost', handler: handleHide }
		: {
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
	errorButtons={canRetry
		? [
				{ label: 'Close', icon: Cancel, class: 'btn-ghost', handler: handleErrorClose },
				{ label: 'Retry', icon: Refresh, class: 'btn-primary', handler: handleRetry }
			]
		: [{ label: 'Close', icon: Cancel, class: 'btn-primary', handler: handleErrorClose }]}
/>
