<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { onDestroy, untrack } from 'svelte';
	import ProgressDialog from '$lib/components/ProgressDialog.svelte';
	import NoteBox from '$lib/components/NoteBox.svelte';
	import { notifications } from '$lib/components/toasts/notifications';
	import {
		beginTracking,
		cancelTracking,
		commitAccepted,
		importTracker,
		needsAttention,
		notifyWhenFinished,
		type HaOutcome
	} from './importTracker.svelte';
	import Self from './DeviceImportDialog.svelte';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Refresh from '~icons/tabler/refresh';
	import Load from '~icons/tabler/folder-open';
	import Send from '~icons/tabler/send';

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
		/** Note for the final result that the file was migrated from an older version. */
		migrationNote?: string;
		/** Runs the chunked-import task. Resolves with ok=true on success, ok=false with
		 *  optional error text on failure, ok=false + aborted=true if the signal fires.
		 *  The dialog calls onProgress to drive its UI, and passes its AbortSignal so the
		 *  task can cancel in-flight fetches and clean up the server-side session slot.
		 *  Not needed with `resume`. */
		task?: (
			onProgress: (p: ImportProgress) => void,
			signal: AbortSignal
		) => Promise<{ ok: boolean; error?: string; aborted?: boolean }>;
		/** Come back with the outcome of an import whose dialog was hidden: nothing is uploaded,
		 *  the dialog shows what the gateway reported in the meantime. */
		resume?: boolean;
		onSuccess?: () => void;
		onAborted?: () => void;
		onError?: (errorDetail?: string) => void;
	}

	let {
		isOpen,
		title = 'Importing Devices',
		totalDevices,
		haEnabled = false,
		migrationNote,
		task,
		resume = false,
		onSuccess,
		onAborted,
		onError,
		...modalProps
	}: Props = $props();

	let phase = $state<'pending' | 'progress' | 'success' | 'error' | 'aborted'>('pending');
	let progress = $state<number | undefined>(undefined);
	let message = $state('Preparing import...');
	let stepLabel = $state<string | undefined>(undefined);
	let errorDetails = $state<string | undefined>(undefined);
	let controller: AbortController | undefined;
	let userAborting = $state(false);

	/** True once the gateway accepted the list; from then on the import cannot be aborted and
	 *  the dialog follows what the gateway reports (see importTracker). */
	let committed = $state(untrack(() => resume));
	/** False after a failure that a second attempt cannot fix (the list is already handed over). */
	let canRetry = $state(true);
	/** What was done for Home Assistant, once the import is through. */
	let haOutcome = $state<HaOutcome>('none');
	let outcomeReported = false;

	const totalSteps = $derived((committed ? importTracker.hasHaStep : haEnabled) ? 3 : 2);

	function importedText(): string {
		return totalDevices === 0
			? 'Device list cleared.'
			: `${totalDevices} ${totalDevices === 1 ? 'device' : 'devices'} imported.`;
	}

	// After the commit: show where the gateway is, or its result once it is through.
	$effect(() => {
		if (!committed) return;
		const outcome = importTracker.outcome;
		if (!outcome) {
			phase = 'progress';
			progress = undefined; // spinner: the gateway reports steps, not percentages
			if (importTracker.saved && importTracker.hasHaStep) {
				message = 'Updating Home Assistant...';
				stepLabel = `Step 3 of ${totalSteps}`;
			} else {
				message = 'Saving on the gateway...';
				stepLabel = `Step 2 of ${totalSteps}`;
			}
		} else if (outcome.kind === 'imported') {
			haOutcome = outcome.ha;
			phase = 'success';
			message = importedText();
			stepLabel = undefined;
			// User confirms by clicking Close; onSuccess fires from that handler.
		} else {
			// Never claim an import the gateway has not confirmed as saved.
			canRetry = false;
			phase = 'error';
			message = 'Import not confirmed';
			stepLabel = undefined;
			errorDetails =
				'The gateway accepted the devices but did not report that they were saved. Reload the page and check the device list.';
		}
	});

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
				beginTracking(haEnabled);
				phase = 'progress';
				progress = 100;
				message = 'Handing over to the gateway...';
				stepLabel = `Step 1 of ${totalSteps}`;
				break;
		}
	}

	async function runTask() {
		if (!task) return;
		controller = new AbortController();
		userAborting = false;
		committed = false;
		outcomeReported = false;
		canRetry = true;
		phase = 'pending';
		progress = undefined;
		message = totalDevices === 0 ? 'Clearing device list...' : 'Preparing import...';
		stepLabel = undefined;
		errorDetails = undefined;

		try {
			const result = await task(handleProgress, controller.signal);
			if (result.ok) {
				// Accepted, not yet saved: follow the gateway through the remaining steps.
				commitAccepted();
				committed = true;
			} else if (result.aborted) {
				cancelTracking();
				phase = 'aborted';
				message = 'Import aborted.';
				stepLabel = undefined;
				progress = undefined;
			} else {
				cancelTracking();
				phase = 'error';
				message = 'Import failed.';
				errorDetails = result.error;
			}
		} catch (e) {
			cancelTracking();
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

	/** Tell the page once that the list was handed over, however the dialog is left. */
	function reportHandedOver() {
		if (outcomeReported) return;
		outcomeReported = true;
		onSuccess?.();
	}

	/** Open the dialog again once nothing else is on the screen. */
	function reopenWhenIdle(open: () => void) {
		if (modals.stack.length === 0) {
			open();
			return;
		}
		const timer = setInterval(() => {
			if (modals.stack.length > 0) return;
			clearInterval(timer);
			open();
		}, 200);
	}

	/**
	 * The dialog is left while the gateway is still working. The gateway goes on; when it is
	 * through, this same dialog comes back with the result if there is something the user has
	 * to see - otherwise a toast confirms the import.
	 */
	function followInBackground() {
		const text = importedText();
		const props = { totalDevices, migrationNote, title };
		notifyWhenFinished((outcome) => {
			if (needsAttention(outcome)) {
				reopenWhenIdle(() => void modals.open(Self, { ...props, resume: true }));
			} else {
				notifications.success(text, 3000);
			}
		});
	}

	function handleHide() {
		followInBackground();
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
		if (!committed) {
			if (!resume) cancelTracking();
			return;
		}
		// Left with Esc or a click beside the dialog while the gateway was still working
		if (!importTracker.outcome && !outcomeReported) followInBackground();
		reportHandedOver();
	});

	// Kick off as soon as the dialog mounts. untrack() guards against accidental
	// re-runs if runTask's reads of reactive state (e.g. totalDevices) ever change.
	$effect(() => {
		untrack(() => {
			if (!resume) runTask();
		});
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
>
	{#snippet successContent()}
		{#if migrationNote}
			<NoteBox tone="info">{migrationNote}</NoteBox>
		{/if}
		{#if haOutcome === 'sent'}
			<!-- Home Assistant does not acknowledge discovery messages: "sent" is all there is. -->
			<NoteBox tone="neutral" icon={Send}>The changes were sent to Home Assistant.</NoteBox>
		{:else if haOutcome === 'pending'}
			<NoteBox tone="warning">
				Home Assistant could not be reached over MQTT. The gateway sends the changes on the next
				MQTT connect.
			</NoteBox>
		{:else if haOutcome === 'unknown'}
			<NoteBox tone="warning">
				The gateway did not report whether Home Assistant was updated. Check the entities there.
			</NoteBox>
		{/if}
	{/snippet}
</ProgressDialog>
