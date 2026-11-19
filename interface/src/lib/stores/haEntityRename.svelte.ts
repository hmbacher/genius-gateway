import { modals } from 'svelte-modals';
import { socket } from '$lib/stores/socket';
import type { HaEntityRenameEvent, HaEntityRenameStatus } from '$lib/types/models';
import HaEntityRenameDialog from '$lib/components/HaEntityRenameDialog.svelte';
import InfoDialog from '$lib/components/InfoDialog.svelte';

const EVENT = 'ha-entity-rename';

/** Latest state reported by the gateway; the progress dialog renders straight from it. */
const status: HaEntityRenameStatus = $state({ state: null, count: 0 });

let watchers = 0;
let progressOpen = false;
let deferredOpen = false;

// Dialogs waiting for the screen to be free. An event usually arrives while the dialog
// that triggered the rename (e.g. the detector editor) is still open and about to close
// itself with modals.close() - opening on top of it would get the wrong dialog closed.
let queue: (() => void)[] = [];
let queueTimer: ReturnType<typeof setInterval> | undefined;

function drainQueue(): boolean {
	while (queue.length > 0 && modals.stack.length === 0) {
		queue.shift()?.();
	}
	return queue.length === 0;
}

function openWhenIdle(open: () => void) {
	queue.push(open);
	if (queueTimer !== undefined || drainQueue()) return;
	queueTimer = setInterval(() => {
		if (drainQueue()) {
			clearInterval(queueTimer);
			queueTimer = undefined;
		}
	}, 100);
}

function handleEvent(event: HaEntityRenameEvent) {
	status.state = event.state;
	status.count = event.count;

	if (event.state === 'started' && !progressOpen) {
		// The gateway decides whether a rename runs; the client never predicts it.
		progressOpen = true;
		openWhenIdle(() =>
			modals.open(HaEntityRenameDialog, {
				status,
				onClosed: () => {
					progressOpen = false;
				}
			})
		);
	} else if (event.state === 'deferred' && !deferredOpen) {
		// No MQTT connection: nothing was sent and there is no progress to show. A dialog
		// instead of a toast, because the user has to know the rename is still outstanding.
		deferredOpen = true;
		const detectors = event.count === 1 ? '1 smoke detector' : `${event.count} smoke detectors`;
		openWhenIdle(() =>
			modals.open(InfoDialog, {
				title: 'Entity IDs not updated yet',
				message: `The MQTT connection is down, so the new Home Assistant entity IDs of <strong>${detectors}</strong> have not been sent.<br><br>The gateway catches up automatically on the next MQTT connect, also after a restart.`,
				variant: 'info',
				onDismiss: () => {
					deferredOpen = false;
					modals.close();
				}
			})
		);
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
