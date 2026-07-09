<script lang="ts">
	import { GeniusAlarmEnding } from '$lib/types/enums';
	import type { GeniusDevice } from '$lib/types/models';
	import { modals, type ModalProps } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import { formatDateTimeSeconds } from '$lib/utils/formatDate';
	import Cancel from '~icons/tabler/x';
	import BellRinging from '~icons/tabler/bell-ringing';
	import Start from '~icons/tabler/arrow-bar-right';
	import End from '~icons/tabler/arrow-bar-to-right';
	import Manual from '~icons/tabler/volume-3';
	import Automatic from '~icons/tabler/flame-off';
	import Imported from '~icons/tabler/file-import';

	// provided by <Modals />

	interface Props extends ModalProps {
		title: string;
		geniusDevice: GeniusDevice;
	}

	let { isOpen, title, geniusDevice }: Props = $props();

	const titleId = `alarm-log-title-${Math.random().toString(36).slice(2)}`;

	// Show newest first - most users want the recent alarms at the top.
	const orderedAlarms = $derived([...geniusDevice.alarms].reverse());
</script>

{#if isOpen}
	<div
		role="dialog"
		aria-modal="true"
		aria-labelledby={titleId}
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center overflow-y-auto"
		transition:fly={{ y: 50 }}
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-md flex-col justify-between p-4 shadow-lg md:w-[28rem]"
		>
			<h2 id={titleId} class="text-base-content text-start text-2xl font-bold">{title}</h2>

			<div class="divider my-2"></div>

			<span class="inline-flex items-center">
				<BellRinging class="mr-2 h-6 w-6" />
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
							{#each orderedAlarms as alarm}
								<tr>
									<td align="left">
										<span class="inline-flex items-baseline">
											<Start class="flex-shrink-0 mr-2 h-4 w-4 self-end" />{formatDateTimeSeconds(
												alarm.startTime
											)}
										</span>
									</td>
									<td align="left">
										<span class="inline-flex items-baseline">
											{#if alarm.endingReason !== GeniusAlarmEnding.AlarmActive}
												<End class="flex-shrink-0 mr-2 h-4 w-4 self-end" />{formatDateTimeSeconds(
													alarm.endTime
												)}
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
										{:else if alarm.endingReason === GeniusAlarmEnding.ByImport}
											<div class="tooltip tooltip-left" data-tip="Cleared on device config import">
												<Imported class="w-6 h-6" />
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
					<Cancel class="h-5 w-5" />
					<span>Close</span>
				</button>
			</div>
		</div>
	</div>
{/if}
