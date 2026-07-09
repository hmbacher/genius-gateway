<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import type { ConfigCheckResponder } from '$lib/types/models';
	import SignalIndicator from './SignalIndicator.svelte';
	import Radar from '~icons/tabler/radar-2';
	import Add from '~icons/tabler/circle-plus';
	import Close from '~icons/tabler/x';

	interface Props extends ModalProps {
		title: string;
		responders: ConfigCheckResponder[];
		onAdd: (r: ConfigCheckResponder) => void;
	}

	const { isOpen, title, responders, onAdd }: Props = $props();

	const titleId = `probe-result-title-${Math.random().toString(36).slice(2)}`;

	/** Format a 32-bit alarm Line-ID - the authoritative line identity. (Group/line is display-only
	 *  metadata and deliberately not used for labeling; see docs/reverse-engineering/protocol-analysis.md.) */
	function lineLabel(id: number): string {
		const u = id >>> 0;
		if (u === 0) return 'unassigned';
		if (u === 0xffffffff) return 'broadcast';
		return '0x' + u.toString(16).toUpperCase().padStart(8, '0');
	}

	function hex(sn: number): string {
		return '0x' + (sn >>> 0).toString(16).toUpperCase().padStart(8, '0');
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
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex max-h-[80vh] w-full max-w-lg flex-col p-4 shadow-lg"
		>
			<h2 id={titleId} class="text-base-content flex items-center gap-2 text-start text-2xl font-bold">
				<Radar class="h-7 w-7 flex-shrink-0 text-primary" />{title}
			</h2>
			<p class="text-base-content/70 mt-1 text-start text-sm">
				Radio modules heard directly during the last signal probe that are not yet registered as
				devices. Add one to start tracking it.
			</p>
			<div class="divider my-2"></div>

			<div class="min-h-0 flex-1 overflow-y-auto">
				<ul class="flex flex-col gap-2">
					{#each responders as r (r.sn)}
						<li
							class="rounded-box bg-base-200 flex items-center gap-3 p-2"
						>
							<div class="min-w-0 flex-1">
								<div class="font-mono text-base font-medium">{hex(r.sn)}</div>
								<div class="text-base-content/60 text-sm">
									Line {lineLabel(r.lineId)}
								</div>
							</div>
							<SignalIndicator rssi={r.rssi} lastRangeTest={new Date()} />
							<button class="btn btn-primary gap-1" onclick={() => onAdd(r)}>
								<Add class="h-5 w-5" /> Add
							</button>
						</li>
					{:else}
						<li class="text-base-content/50 py-6 text-center italic">No new modules discovered.</li>
					{/each}
				</ul>
			</div>

			<div class="divider my-2"></div>
			<div class="flex justify-end">
				<button class="btn inline-flex items-center gap-1" onclick={() => modals.close()}>
					<Close class="h-5 w-5" /> Close
				</button>
			</div>
		</div>
	</div>
{/if}
