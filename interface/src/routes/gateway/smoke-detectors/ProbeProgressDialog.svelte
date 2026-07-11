<script lang="ts">
	import { modals, onBeforeClose, type ModalProps } from 'svelte-modals';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly, slide } from 'svelte/transition';
	import type { ConfigCheckResponder, ConfigCheckProbeEvent } from '$lib/types/models';
	import { socket } from '$lib/stores/socket';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import SignalIndicator from './SignalIndicator.svelte';
	import Radar from '~icons/tabler/radar';
	import RadarSweep from '~icons/tabler/radar-2';
	import Add from '~icons/tabler/circle-plus';
	import Close from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Stop from '~icons/tabler/player-stop';
	import IconHash from '~icons/tabler/hash';
	import IconNumber from '~icons/tabler/number';
	import ChevronDown from '~icons/tabler/chevron-down';

	interface Props extends ModalProps {
		title: string;
		/** Total survey window in ms (drives the progress bar / countdown). */
		windowMs: number;
		/** Survey id - filters the WS stream to this survey. */
		sweepId: number;
		/** Add a discovered (unknown) module as a new device; onAdded fires once it is saved. */
		onAdd: (r: ConfigCheckResponder, onAdded: () => void) => void;
		/** Alarm-line IDs already configured on the gateway (snapshot taken when the probe started). */
		registeredLineIds: number[];
		/** Open the alarm-line editor for a not-yet-configured line; onRegistered fires once saved. */
		onAddLine: (lineId: number, onRegistered: () => void) => void;
		/** Finalize the survey now, keeping the responders heard so far (user pressed "Stop probing"). */
		onStop: () => void;
		/** Abort and discard the survey (user pressed "Cancel" while it was still running). */
		onCancel: () => void;
	}

	const {
		isOpen,
		title,
		windowMs,
		sweepId,
		onAdd,
		registeredLineIds,
		onAddLine,
		onStop,
		onCancel
	}: Props = $props();

	// This dialog is deliberately non-dismissable: a stray backdrop click or Escape must not throw
	// away a running probe or the addable results. onBeforeClose blocks every close route (backdrop,
	// Escape, programmatic) unless one of our own buttons set `allowClose` first.
	let allowClose = false;
	onBeforeClose(() => allowClose);

	/** Authorized dismiss - the only way this dialog actually closes. */
	function dismiss() {
		allowClose = true;
		modals.close();
	}

	// True from the moment "Stop probing" is pressed until the survey's 'done' event lands - keeps
	// the footer buttons disabled and shows "Stopping…" so the action can't be double-fired.
	let stopping = $state(false);

	/** Stop probing early but keep the dialog open on the results (so the user can add what showed up).
	 *  The backend finalizes and emits 'done', which flips us into the results view below. */
	function handleStop() {
		if (stopping || done) return;
		stopping = true;
		onStop();
		// Safety net: if the 'done' event is missed, still surface the results we already have so the
		// dialog can't get stuck on "Stopping…".
		setTimeout(() => {
			if (!done) {
				responderCount = heard.size;
				stopped = true;
				done = true;
			}
		}, 4000);
	}

	/** Abort the whole survey and close the dialog. */
	function handleCancel() {
		onCancel();
		dismiss();
	}

	// Alarm lines already configured on the gateway before this probe (frozen snapshot). These never
	// offer an "Add alarm line" button - there is nothing to add.
	const registered = new Set<number>(registeredLineIds.map((id) => id >>> 0));
	// Lines registered from this dialog this session; grown on save so the button flips to "Added"
	// in place (rather than just disappearing) - matching the device-add behaviour below.
	let addedLines = $state(new Set<number>());

	// 0x00000000 = unassigned (ALARMLINES_ID_NONE), 0xFFFFFFFF = broadcast - neither is a user line.
	function isRealLine(lineId: number): boolean {
		const u = lineId >>> 0;
		return u !== 0 && u !== 0xffffffff;
	}

	/** Hand off to the parent's alarm-line editor (mirrors the device-add flow); on a successful save
	 *  mark the line added so its button flips to "Added". */
	function addLine(lineId: number) {
		const key = lineId >>> 0;
		if (registered.has(key) || addedLines.has(key)) return;
		onAddLine(lineId, () => (addedLines = new Set(addedLines).add(key)));
	}

	// Serials added as devices from this dialog; grown on save so the row's "Add" button flips to
	// an "Added" state in place (rather than the row still looking un-added after the editor closes).
	let added = $state(new Set<number>());

	/** Hand off to the parent's device editor; on a successful save mark this serial added. */
	function addDevice(r: ConfigCheckResponder) {
		const key = r.sn >>> 0;
		if (added.has(key)) return;
		onAdd(r, () => (added = new Set(added).add(key)));
	}

	const titleId = `probe-progress-title-${Math.random().toString(36).slice(2)}`;

	type Heard = { known: boolean; r: ConfigCheckResponder; at: Date };
	// Keyed by serial so repeated sightings of the same module update in place.
	let heard = $state<Map<number, Heard>>(new Map());
	let done = $state(false);
	let stopped = $state(false); // survey ended via an early user stop rather than running to completion
	let responderCount = $state(0);

	const knownList = $derived([...heard.values()].filter((h) => h.known));
	const unknownList = $derived([...heard.values()].filter((h) => !h.known));

	// Section collapse state - both start expanded; collapsing is opt-in. Kept as its own state
	// (independent of the derived lists) so a toggle survives the live WS updates below.
	let knownOpen = $state(true);
	let unknownOpen = $state(true);

	type LineGroup = { lineId: number; items: Heard[] };
	/** Bucket responders by their alarm Line-ID (writing the line once per group instead of per row),
	 *  sorted ascending so the grouping stays stable as responders trickle in. */
	function groupByLine(list: Heard[]): LineGroup[] {
		const map = new Map<number, Heard[]>();
		for (const h of list) {
			const key = h.r.lineId >>> 0;
			const arr = map.get(key);
			if (arr) arr.push(h);
			else map.set(key, [h]);
		}
		return [...map.entries()]
			.map(([lineId, items]) => ({ lineId, items }))
			.sort((a, b) => a.lineId - b.lineId);
	}
	const knownGroups = $derived(groupByLine(knownList));
	const unknownGroups = $derived(groupByLine(unknownList));

	// ── Timer / progress ──────────────────────────────────────────────────────
	const startedAt = Date.now();
	let now = $state(Date.now());
	const remainingMs = $derived(done ? 0 : Math.max(0, windowMs - (now - startedAt)));
	const percent = $derived(done ? 100 : Math.min(100, ((now - startedAt) / windowMs) * 100));
	const remainingS = $derived(Math.ceil(remainingMs / 1000));

	$effect(() => {
		if (done) return;
		const id = setInterval(() => (now = Date.now()), 250);
		return () => clearInterval(id);
	});

	// ── Live WS stream ────────────────────────────────────────────────────────
	$effect(() => {
		const handler = (evt: ConfigCheckProbeEvent) => {
			if (evt.sweepId !== sweepId) return;
			if (evt.phase === 'responder' && evt.responder) {
				const next = new Map(heard);
				next.set(evt.responder.sn, { known: evt.known ?? false, r: evt.responder, at: new Date() });
				heard = next;
			} else if (evt.phase === 'done') {
				done = true;
				stopped = evt.stopped ?? false;
				responderCount = evt.responderCount ?? heard.size;
				// Reconcile against the authoritative 'done' payload - covers any 'responder'
				// events that were missed while the dialog was still opening.
				if (evt.responders) {
					const unknownSns = new Set((evt.discovered ?? []).map((d) => d.sn));
					const next = new Map(heard);
					for (const r of evt.responders) {
						next.set(r.sn, { known: !unknownSns.has(r.sn), r, at: next.get(r.sn)?.at ?? new Date() });
					}
					heard = next;
				}
			}
		};
		socket.on<ConfigCheckProbeEvent>('config-check-probe', handler);
		return () => socket.off('config-check-probe', handler);
	});

	/** Format a 32-bit alarm Line-ID (decimal) - the authoritative line identity. (Group/line is
	 *  display-only metadata and deliberately not used for labeling; see docs/reverse-engineering/protocol-analysis.md.) */
	function lineLabel(id: number): string {
		const u = id >>> 0;
		if (u === 0) return 'unassigned';
		if (u === 0xffffffff) return 'broadcast';
		return u.toString(10);
	}
	/** Format a 32-bit serial as an unsigned decimal number. */
	function dec(sn: number): string {
		return (sn >>> 0).toString(10);
	}
	/** Location of the registered device whose radio module matches this serial, if any. */
	function locationFor(sn: number): string | undefined {
		const key = sn >>> 0;
		const loc = geniusDevices.devices.find((d) => (d.radioModule.sn >>> 0) === key)?.location?.trim();
		return loc ? loc : undefined;
	}
</script>

{#if isOpen}
	<div
		role="dialog"
		aria-modal="true"
		aria-labelledby={titleId}
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center p-4"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex max-h-[85vh] w-full max-w-lg flex-col p-4 shadow-lg"
		>
			<h2 id={titleId} class="text-base-content flex items-center gap-2 text-start text-2xl font-bold">
				<Radar class="h-7 w-7 flex-shrink-0 text-primary {done ? '' : 'animate-pulse'}" />{title}
			</h2>

			{#if done}
				<p class="text-base-content/70 mt-1 flex items-center gap-1 text-start text-sm">
					<Check class="h-4 w-4 text-success" />
					{stopped ? 'Stopped' : 'Complete'} - {responderCount} module{responderCount === 1
						? ''
						: 's'} responded.
				</p>
			{:else}
				<p class="text-base-content/70 mt-1 text-start text-sm">
					Probing directly-reachable modules… ~{remainingS}s remaining
				</p>
				<div class="mt-2 flex items-center gap-3">
					<progress class="progress progress-primary flex-1" value={percent} max="100"></progress>
					<button
						class="btn btn-primary inline-flex flex-shrink-0 items-center gap-1"
						onclick={handleStop}
						disabled={stopping}
					>
						<Stop class="h-5 w-5" /> {stopping ? 'Stopping…' : 'Stop'}
					</button>
				</div>
			{/if}

			<div class="divider my-2"></div>

			<div class="min-h-0 flex-1 space-y-4 overflow-y-auto">
				<!-- Per-line group header: "Alarm Line #<id>" plus a one-click register button for lines the
				     gateway doesn't yet know about. Shared by the known and unknown sections. -->
				{#snippet lineHeader(lineId: number)}
					<div class="mb-1 flex items-center gap-2">
						<div class="text-base-content/50 flex items-center gap-1 text-xs font-medium">
							<span>Alarm Line</span>
							<span class="flex items-center gap-0.5 font-mono">
								<IconHash class="h-3.5 w-3.5" />{lineLabel(lineId)}
							</span>
						</div>
						{#if isRealLine(lineId) && !registered.has(lineId >>> 0)}
							{#if addedLines.has(lineId >>> 0)}
								<button class="btn btn-ghost btn-xs gap-1 text-success" disabled>
									<Check class="h-3.5 w-3.5" /> Added
								</button>
							{:else}
								<button
									class="btn btn-outline btn-secondary btn-xs gap-1"
									onclick={() => addLine(lineId)}
									disabled={!done}
									title={done ? undefined : 'Stop probing to add this alarm line'}
								>
									<Add class="h-3.5 w-3.5" /> Add alarm line
								</button>
							{/if}
						{/if}
					</div>
				{/snippet}

				<!-- Registered devices (known) - grouped at the top; only shown when at least one responded -->
				{#if knownList.length > 0}
					<div>
						<button
							type="button"
							class="text-base-content/60 hover:text-base-content flex w-full cursor-pointer items-center gap-1.5 text-start text-xs font-semibold tracking-wide uppercase"
							class:mb-2={knownOpen}
							onclick={() => (knownOpen = !knownOpen)}
							aria-expanded={knownOpen}
						>
							<ChevronDown
								class="h-3.5 w-3.5 transition-transform {knownOpen ? '' : '-rotate-90'}"
							/>
							Already registered devices ({knownList.length})
						</button>
						{#if knownOpen}
							<div class="flex flex-col gap-3" transition:slide={{ duration: 200 }}>
								{#each knownGroups as g (g.lineId)}
									<div>
										{@render lineHeader(g.lineId)}
										<ul class="flex flex-col gap-2">
											{#each g.items as h (h.r.sn)}
												{@const loc = locationFor(h.r.sn)}
												<li class="rounded-box bg-base-200 flex items-center gap-3 p-2">
													<div class="flex min-w-0 flex-1 items-center gap-2">
														<span
															class="truncate text-sm font-bold {loc ? '' : 'text-base-content/70 italic'}"
														>
															{loc ?? 'Unknown location'}
														</span>
														<span
															class="text-base-content/60 flex flex-shrink-0 items-center gap-0.5 font-mono text-sm"
														>
															<IconNumber class="h-3.5 w-3.5" />{dec(h.r.sn)}
														</span>
													</div>
													<SignalIndicator rssi={h.r.rssi} lastRangeTest={h.at} />
												</li>
											{/each}
										</ul>
									</div>
								{/each}
							</div>
						{/if}
					</div>
				{/if}

				<!-- New nearby modules (unknown) - grouped below, with one-click add; only when non-empty -->
				{#if unknownList.length > 0}
					<div>
						<button
							type="button"
							class="text-base-content/60 hover:text-base-content flex w-full cursor-pointer items-center gap-1.5 text-start text-xs font-semibold tracking-wide uppercase"
							class:mb-2={unknownOpen}
							onclick={() => (unknownOpen = !unknownOpen)}
							aria-expanded={unknownOpen}
						>
							<ChevronDown
								class="h-3.5 w-3.5 transition-transform {unknownOpen ? '' : '-rotate-90'}"
							/>
							New nearby devices ({unknownList.length})
						</button>
						{#if unknownOpen}
							<div class="flex flex-col gap-3" transition:slide={{ duration: 200 }}>
								{#each unknownGroups as g (g.lineId)}
									<div>
										{@render lineHeader(g.lineId)}
										<ul class="flex flex-col gap-2">
											{#each g.items as h (h.r.sn)}
												<li class="rounded-box bg-base-200 flex items-center gap-3 p-2">
													<span
														class="text-base-content flex min-w-0 flex-1 items-center gap-0.5 font-mono text-sm font-medium"
													>
														<IconNumber class="h-4 w-4 flex-shrink-0" />{dec(h.r.sn)}
													</span>
													<SignalIndicator rssi={h.r.rssi} lastRangeTest={h.at} />
													{#if added.has(h.r.sn >>> 0)}
														<button class="btn btn-ghost gap-1 text-success" disabled>
															<Check class="h-5 w-5" /> Added
														</button>
													{:else}
														<button
															class="btn btn-secondary gap-1"
															onclick={() => addDevice(h.r)}
															disabled={!done}
															title={done ? undefined : 'Stop probing to add this detector'}
														>
															<Add class="h-5 w-5" /> Add
														</button>
													{/if}
												</li>
											{/each}
										</ul>
									</div>
								{/each}
							</div>
						{/if}
					</div>
				{/if}

				<!-- Nothing to show yet: while probing, a rotating radar sweep stands in as the
				     central "listening" element (mirrors the acoustic dialog's live mic); once done
				     with no answers, fall back to a plain message. -->
				{#if knownList.length === 0 && unknownList.length === 0}
					{#if done}
						<div class="text-base-content/50 py-6 text-center text-sm italic">
							No modules responded.
						</div>
					{:else}
						<div
							class="flex flex-col items-center justify-center gap-4 py-6"
							role="status"
							aria-live="polite"
						>
							<div class="relative flex h-32 w-32 items-center justify-center">
								<!-- Radar pulses out from a static, muted dish. inset-8 keeps the fully
								     expanded ring (animate-ping scales to 2×) within this box, so it is
								     never clipped by the surrounding scroll area while still visible. -->
								<div
									class="border-primary/40 absolute inset-8 animate-ping rounded-full border motion-reduce:hidden"
									style="animation-duration: 2s"
								></div>
								<div
									class="border-primary/40 absolute inset-8 animate-ping rounded-full border motion-reduce:hidden"
									style="animation-duration: 2s; animation-delay: -1s"
								></div>
								<RadarSweep class="h-16 w-16 text-slate-400 dark:text-slate-500" />
							</div>
							<p class="text-base-content/60 text-sm">Listening for modules…</p>
						</div>
					{/if}
				{/if}
			</div>

			<div class="divider my-2"></div>
			<div class="flex justify-end">
				{#if done}
					<button class="btn btn-primary inline-flex items-center gap-1" onclick={dismiss}>
						<Close class="h-5 w-5" /> Close
					</button>
				{:else}
					<button
						class="btn btn-ghost inline-flex items-center gap-1"
						onclick={handleCancel}
						disabled={stopping}
					>
						<Close class="h-5 w-5" /> Cancel
					</button>
				{/if}
			</div>
		</div>
	</div>
{/if}
