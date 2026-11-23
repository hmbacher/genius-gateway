import { modals } from 'svelte-modals';
import { socket } from '$lib/stores/socket';
import type { HaEntityRenameEvent, HaEntityRenameStatus } from '$lib/types/models';
import HaEntityRenameDialog from '$lib/components/HaEntityRenameDialog.svelte';
import InfoDialog from '$lib/components/InfoDialog.svelte';
import { escapeHtml, renamedDetectors } from '$lib/genius/entityRename';

const EVENT = 'ha-entity-rename';

/** Latest state reported by the gateway; the progress dialog renders straight from it. */
const status: HaEntityRenameStatus = $state({ state: null, count: 0, detectors: [] });

let watchers = 0;

// While the import dialog follows the gateway, it reports the outcome of the rename the
// import caused as one of its steps - no separate notice then.
let importTracking = false;

/** Tell the store that an import dialog reports the background rename itself (or stopped doing so). */
export function setImportTracking(active: boolean) {
	importTracking = active;
}
let progressOpen = false;
let deferredOpen = false;

// Dialogs waiting for the screen to be free. An event usually arrives while the dialog
// that triggered the rename (e.g. the detector editor) is still open and about to close
// itself with modals.close() - opening on top of it would get the wrong dialog closed.
type QueuedDialog = { kind: 'progress' | 'deferred'; open: () => void };
let queue: QueuedDialog[] = [];
let queueTimer: ReturnType<typeof setInterval> | undefined;

function drainQueue(): boolean {
	while (queue.length > 0 && modals.stack.length === 0) {
		queue.shift()?.open();
	}
	return queue.length === 0;
}

function openWhenIdle(kind: QueuedDialog['kind'], open: () => void) {
	queue.push({ kind, open });
	if (queueTimer !== undefined || drainQueue()) return;
	queueTimer = setInterval(() => {
		if (drainQueue()) {
			clearInterval(queueTimer);
			queueTimer = undefined;
		}
	}, 100);
}

/**
 * Tell the user that new entity IDs have not reached Home Assistant. A dialog instead of a
 * toast, because the user has to know the update is still outstanding.
 */
function openNotSentNotice(event: HaEntityRenameEvent, reason: string) {
	if (deferredOpen) return;
	deferredOpen = true;
	// The message is HTML and locations are entered by the user: escape them
	const detectors = renamedDetectors(
		event.count,
		event.detectors,
		(name) => `<strong>${escapeHtml(name)}</strong>`
	);
	openWhenIdle('deferred', () =>
		modals.open(InfoDialog, {
			title: 'Entity IDs not updated yet',
			message: `Entity IDs in Home Assistant follow the location of a smoke detector. ${reason}, so the new entity IDs of ${detectors} have not been sent.<br><br>The gateway catches up automatically on the next MQTT connect, also after a restart.`,
			variant: 'info',
			onDismiss: () => {
				deferredOpen = false;
				modals.close();
			}
		})
	);
}

function handleEvent(event: HaEntityRenameEvent) {
	// A cycle caused by an import runs in the background: whoever imports expects Home
	// Assistant to be brought in line without further ado. The import dialog shows it as one
	// of its steps; without that dialog (another tab, dialog hidden) only what did not work
	// is reported.
	if (event.background) {
		if (importTracking) return;
		if (event.state === 'deferred') {
			openNotSentNotice(event, 'The MQTT connection is down');
		} else if (event.state === 'failed') {
			openNotSentNotice(event, 'The MQTT connection was lost during the update');
		}
		return;
	}

	status.state = event.state;
	status.count = event.count;
	status.detectors = event.detectors ?? [];

	if (event.state === 'started') {
		// The rename is running now. A notice about a deferred rename that is still waiting
		// for the screen to be free (e.g. behind a dialog left open in a background tab) is
		// out of date and must not pop up afterwards.
		if (queue.some((queued) => queued.kind === 'deferred')) {
			queue = queue.filter((queued) => queued.kind !== 'deferred');
			deferredOpen = false;
		}

		if (progressOpen) return;

		// The gateway decides whether a rename runs; the client never predicts it.
		progressOpen = true;
		// open() resolves once the dialog is closed, however that happens. The dialog must not
		// report it from onDestroy: reading a prop during teardown makes svelte-modals look up
		// the dialog in a stack it has already left.
		openWhenIdle('progress', () => {
			void modals.open(HaEntityRenameDialog, { status }).then(() => {
				progressOpen = false;
			});
		});
	} else if (event.state === 'deferred') {
		// No MQTT connection: nothing was sent and there is no progress to show.
		openNotSentNotice(event, 'The MQTT connection is down');
	}
}

/**
 * Follow Home Assistant entity-ID renames while a page is mounted.
 *
 * Opens the progress dialog when the gateway starts a rename cycle and an info dialog
 * when a rename had to be deferred. Call from `onMount` and return the result, so the
 * subscription ends with the page.
 */
export function watchHaEntityRename(): () => void {
	if (watchers++ === 0) socket.on<HaEntityRenameEvent>(EVENT, handleEvent);

	return () => {
		if (--watchers > 0) return;
		socket.off(EVENT, handleEvent);
		if (queueTimer !== undefined) clearInterval(queueTimer);
		queueTimer = undefined;
		queue = [];
		progressOpen = false;
		deferredOpen = false;
	};
}
