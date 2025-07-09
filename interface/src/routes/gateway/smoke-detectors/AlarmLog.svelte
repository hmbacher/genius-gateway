<script lang="ts">
	import { GeniusAlarmEnding } from '$lib/types/enums';
	import { modals } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import BellRinging from '~icons/tabler/bell-ringing';
	import Start from '~icons/tabler/arrow-bar-right';
	import End from '~icons/tabler/arrow-bar-to-right';
	import Manual from '~icons/tabler/volume-3';
	import Automatic from '~icons/tabler/flame-off';

	// provided by <Modals />

	interface Props {
		isOpen: boolean;
		title: string;
		geniusDevice: any;
	}

	let { isOpen, title, geniusDevice }: Props = $props();

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
				<span class="text-xl font-semibold">Alarms of device <em>{geniusDevice.location}</em></span>
			</span>

			<div class="divider my-2"></div>

			{#if geniusDevice.alarms.length === 0}
				<div class="text-center text-base-content">No alarms</div>
			{:else}
				<div class="max-h-160 overflow-x-auto">
					<table class="table w-full table-pin-rows">
						<thead>
							<tr class="font-bold">
								<th align="left">Start</th>
								<th align="left">End</th>
								<th align="center">Ending Reason</th>
							</tr>
						</thead>
						<tbody>
							{#each geniusDevice.alarms as alarm}
								<tr>
									<td align="left">
										<span class="inline-flex items-baseline">
											<Start
												class="lex-shrink-0 mr-2 h-4 w-4 self-end"
											/>{alarm.startTime.toLocaleString('de-DE', {
												day: '2-digit',
												month: '2-digit',
												year: 'numeric',
												hour: '2-digit',
												minute: '2-digit',
												second: '2-digit'
											})}
										</span>
									</td>
									<td align="left">
										<span class="inline-flex items-baseline">
											{#if alarm.endingReason === 0 || alarm.endingReason === 1}
												<End
													class="lex-shrink-0 mr-2 h-4 w-4 self-end"
												/>{alarm.endTime.toLocaleString('de-DE', {
													day: '2-digit',
													month: '2-digit',
													year: 'numeric',
													hour: '2-digit',
													minute: '2-digit',
													second: '2-digit'
												})}
											{:else}
												<span class="text-base-content/50">No data</span>
											{/if}
										</span>
									</td>
									<td align="center">
										{#if alarm.endingReason === GeniusAlarmEnding.BySmokeDetector}
											<div class="tooltip tooltip-left" data-tip="No more smoke detected">
												<Automatic class="w-6 h-6" />
											</div>
										{:else if alarm.endingReason === GeniusAlarmEnding.ByManual}
											<div class="tooltip tooltip-left" data-tip="Alarming stopped by user">
												<Manual class="w-6 h-6" />
											</div>
										{/if}
									</td>
								</tr>
							{/each}
						</tbody>
					</table>
				</div>
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
