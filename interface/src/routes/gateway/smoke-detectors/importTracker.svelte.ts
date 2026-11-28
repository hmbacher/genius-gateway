/**
 * What the gateway does after an import was committed.
 *
 * The commit only hands the list over. Writing it to flash and bringing Home Assistant in line
 * happen afterwards on the gateway, which reports both with WebSocket events. This module
 * follows them, independent of the import dialog: the dialog renders from it while it is open,
 * and when it was hidden the outcome is still known - so the dialog can come back with it.
 */
import { socket } from '$lib/stores/socket';
import { setImportTracking } from '$lib/stores/haEntityRename.svelte';
import type { DeviceImportEvent, HaEntityRenameEvent } from '$lib/types/models';

/** What was done for Home Assistant. `sent` is all there is - it acknowledges nothing. */
export type HaOutcome = 'none' | 'sent' | 'pending' | 'unknown';

/**
 * How the import ended. `unconfirmed`: the gateway accepted the list but never reported that
 * it was saved - nothing may be claimed as imported then.
 */
export type ImportOutcome = { kind: 'imported'; ha: HaOutcome } | { kind: 'unconfirmed' };

const IMPORT_EVENT = 'device-import';
const RENAME_EVENT = 'ha-entity-rename';
/** How long to wait for the next sign of life from the gateway once the list is handed over. */
const GATEWAY_SILENCE_MS = 90_000;

/** The gateway's progress with the import that is being followed. */
export const importTracker = $state({
	/** The gateway confirmed that the list is written to flash. */
	saved: false,
	/** Home Assistant is one of the steps (the gateway says so with "saved"). */
	hasHaStep: false,
	/** Set once the gateway is through, or stayed silent for too long. */
	outcome: null as ImportOutcome | null
});

let listening = false;
let committed = false;
// Events can arrive in any order: the entity-ID rename runs in a task of its own and may
// finish before the import reports "done" - and both may beat the reply to the commit request.
let done: 'disabled' | 'offline' | 'renaming' | 'sent' | undefined;
let renameEnd: 'sent' | 'deferred' | 'failed' | undefined;
let watchdog: ReturnType<typeof setTimeout> | undefined;
let whenFinished: ((outcome: ImportOutcome) => void) | undefined;

function stop() {
	if (watchdog !== undefined) clearTimeout(watchdog);
	watchdog = undefined;
	// From here on a background rename that cannot be sent is reported by the rename store again.
	setImportTracking(false);
	if (!listening) return;
	listening = false;
	socket.off(IMPORT_EVENT, handleImportEvent);
	socket.off(RENAME_EVENT, handleRenameEvent);
}

function finish(outcome: ImportOutcome) {
	stop();
	importTracker.outcome = outcome;
	const callback = whenFinished;
	whenFinished = undefined;
	callback?.(outcome);
}

function armWatchdog() {
	if (watchdog !== undefined) clearTimeout(watchdog);
	watchdog = setTimeout(() => {
		watchdog = undefined;
		if (!committed || importTracker.outcome) return;
		// The gateway stopped reporting (connection lost, restart). Say what is known - and
		// never claim an import the gateway has not confirmed as saved.
		finish(importTracker.saved ? { kind: 'imported', ha: 'unknown' } : { kind: 'unconfirmed' });
	}, GATEWAY_SILENCE_MS);
}

function evaluate() {
	if (!committed || importTracker.outcome) return;
	if (done === undefined || (done === 'renaming' && renameEnd === undefined)) return;

	const sent =
		(done === 'sent' && renameEnd !== 'deferred' && renameEnd !== 'failed') ||
		(done === 'renaming' && renameEnd === 'sent');
	finish({ kind: 'imported', ha: done === 'disabled' ? 'none' : sent ? 'sent' : 'pending' });
}

function handleImportEvent(event: DeviceImportEvent) {
	if (importTracker.outcome) return;
	if (event.state === 'saved') {
		importTracker.saved = true;
		importTracker.hasHaStep = event.ha;
	} else if (event.state === 'done') {
		importTracker.saved = true; // "done" implies it, should "saved" have been missed
		done = event.ha;
	}
	if (!committed) return;
	armWatchdog();
	evaluate();
}

function handleRenameEvent(event: HaEntityRenameEvent) {
	// Only the cycle the import caused. An announced cycle belongs to somebody's edit.
	if (importTracker.outcome || !event.background) return;
	if (event.state === 'sent' || event.state === 'deferred' || event.state === 'failed') {
		renameEnd = event.state;
	}
	if (!committed) return;
	armWatchdog();
	evaluate();
}

/**
 * Start following an import, right before its commit request is sent: whatever the gateway
 * reports from now on belongs to it.
 *
 * @param haEnabled whether Home Assistant is expected to be one of the steps
 */
export function beginTracking(haEnabled: boolean) {
	stop();
	committed = false;
	done = undefined;
	renameEnd = undefined;
	whenFinished = undefined;
	importTracker.saved = false;
	importTracker.hasHaStep = haEnabled;
	importTracker.outcome = null;
	setImportTracking(true);
	listening = true;
	socket.on<DeviceImportEvent>(IMPORT_EVENT, handleImportEvent);
	socket.on<HaEntityRenameEvent>(RENAME_EVENT, handleRenameEvent);
}

/** The gateway accepted the list. From here on the import cannot be aborted any more. */
export function commitAccepted() {
	committed = true;
	armWatchdog();
	evaluate();
}

/** The upload failed or was aborted: nothing to follow. */
export function cancelTracking() {
	stop();
	committed = false;
	whenFinished = undefined;
}

/**
 * The dialog was hidden while the gateway is still working. Calls back once with the outcome,
 * so the caller can bring the dialog back or just say that all went well.
 */
export function notifyWhenFinished(callback: (outcome: ImportOutcome) => void) {
	if (importTracker.outcome) callback(importTracker.outcome);
	else whenFinished = callback;
}

/** True if the outcome is something the user has to see, not just a confirmation. */
export function needsAttention(outcome: ImportOutcome): boolean {
	return outcome.kind === 'unconfirmed' || outcome.ha === 'pending' || outcome.ha === 'unknown';
}
