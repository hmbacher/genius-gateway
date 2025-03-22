<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { modals } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import BellRinging from '~icons/tabler/bell-ringing';
	import Start from '~icons/tabler/arrow-bar-right';
	import End from '~icons/tabler/arrow-bar-to-right';
	import Manual from '~icons/tabler/hand-click';
	import Automatic from '~icons/tabler/flame-off';

	// provided by <Modals />

	interface Props {
		isOpen: boolean;
		title: string;
		hekatronDevice: any;
	}

	let { isOpen, title, hekatronDevice }: Props = $props();

	function preventDefault(fn) {
		return function (event) {
			event.preventDefault();
			fn.call(this, event);
		};
	}
</script>

{#if isOpen}
	<div
		role="dialog"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center overflow-y-auto"
		transition:fly={{ y: 50 }}
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-md flex-col justify-between p-4 shadow-lg md:w-[28rem]"
		>
			<h2 class="text-base-content text-start text-2xl font-bold">{title}</h2>

			<div class="divider my-2"></div>

			<span class="inline-flex items-baseline">
				<BellRinging class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
				<span class="text-xl font-semibold"
					>Alarms of device <em>{hekatronDevice.location}</em></span
				>
			</span>

			<div class="divider my-2"></div>

			{#if hekatronDevice.alarms.length === 0}
				<div class="text-center text-base-content">No alarms</div>
			{:else}
				<table class="table w-full table-auto">
					<thead>
						<tr class="font-bold">
							<th align="left">Start</th>
							<th align="left">End</th>
							<th align="center">Ending Reason</th>
						</tr>
					</thead>
					<tbody>
						{#each hekatronDevice.alarms as alarm}
							<tr>
								<td align="left">
									<span class="inline-flex items-baseline">
										<Start
											class="lex-shrink-0 mr-2 h-4 w-4 self-end"
										/>{alarm.startTime.toLocaleString()}
									</span>
								</td>
								<td align="left">
									<span class="inline-flex items-baseline">
										<End
											class="lex-shrink-0 mr-2 h-4 w-4 self-end"
										/>{alarm.endTime.toLocaleString()}
									</span>
								</td>
								<td align="center">
									{#if alarm.endingReason === 0}
										<div><Automatic class="w-6 h-6" /></div>
									{:else}
										<div><Manual class="text-accent w-6 h-6" /></div>
									{/if}
								</td>
							</tr>
						{/each}
					</tbody>
				</table>
			{/if}

			<div class="divider my-2"></div>

			<div class="flex justify-end gap-2">
				<button
					class="btn btn-neutral text-neutral-content inline-flex items-center"
					onclick={() => {
						modals.close();
					}}
					type="button"
				>
					<Cancel class="mr-2 h-5 w-5" />
					<span>Close</span>
				</button>
			</div>
		</div>
	</div>
{/if}
