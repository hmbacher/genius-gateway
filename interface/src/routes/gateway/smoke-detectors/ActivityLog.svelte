<script lang="ts">
	import type { Component } from 'svelte';
	import type { GeniusDevice } from '$lib/types/models';
	import { modals, type ModalProps } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import { formatDateTimeSeconds } from '$lib/utils/formatDate';
	import { alarmCount, describeEvent } from '$lib/genius/activity';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import {
		EVENT_ALARM,
		EVENT_CREATED,
		EVENT_MODULE_MISSING,
		EVENT_READOUT,
		EVENT_STATUS_PACKET,
		EVENT_STATUS_RESET
	} from '$lib/genius/status';
	import { GeniusAlarmEnding } from '$lib/types/enums';
	import Cancel from '~icons/tabler/x';
	import Logs from '~icons/tabler/logs';
	import Details from '~icons/tabler/list-details';
	import IconAlarm from '~icons/tabler/bell-ringing';
	import IconCreated from '~icons/tabler/circle-plus';
	import IconReadout from '~icons/tabler/microphone';
	import IconStatus from '~icons/tabler/access-point';
	import IconReset from '~icons/tabler/restore';
	import IconMissing from '~icons/tabler/access-point-off';
	import IconOther from '~icons/tabler/point';

	// provided by <Modals />

	interface Props extends ModalProps {
		title: string;
		geniusDevice: GeniusDevice;
		/** Open with the filter "alarms only" switched on. */
		alarmsOnly?: boolean;
		/** When set, the footer offers a "Details" button that closes the log and calls this. */
		onDetails?: () => void;
	}

	let {
		isOpen,
		title,
		geniusDevice: openedDevice,
		alarmsOnly = false,
		onDetails
	}: Props = $props();

	// Follow the device list: an entry added while the dialog is open shows up at once
	let geniusDevice = $derived(
		geniusDevices.devices.find((d) => d.id === openedDevice.id) ?? openedDevice
	);

	const titleId = `activity-log-title-${Math.random().toString(36).slice(2)}`;

	const ICONS: Record<number, Component<{ class?: string }>> = {
		[EVENT_CREATED]: IconCreated,
		[EVENT_READOUT]: IconReadout,
		[EVENT_STATUS_PACKET]: IconStatus,
		[EVENT_STATUS_RESET]: IconReset,
		[EVENT_MODULE_MISSING]: IconMissing,
		[EVENT_ALARM]: IconAlarm
	};

	// The prop only gives the initial position of the switch.
	// svelte-ignore state_referenced_locally
	let onlyAlarms = $state(alarmsOnly);

	// Show newest first - most users want the recent entries at the top.
	const allEntries = $derived([...(geniusDevice.events ?? [])].reverse());
	const entries = $derived(allEntries.filter((e) => !onlyAlarms || e.type === EVENT_ALARM));

	// The gateway counts the alarms separately; the log only keeps the newest entries.
	const alarmsInLog = $derived(allEntries.filter((e) => e.type === EVENT_ALARM).length);
	const alarmsTotal = $derived(alarmCount(geniusDevice));
</script>

{#if isOpen}
	<div
		role="dialog"
		aria-modal="true"
		aria-labelledby={titleId}
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center p-4"
		transition:fly={{ y: 50 }}
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex max-h-full w-full max-w-xl flex-col justify-between p-4 shadow-lg"
		>
			<h2
				id={titleId}
				class="text-base-content flex items-center gap-2 text-start text-2xl font-bold"
			>
				<Logs class="text-primary h-7 w-7 flex-shrink-0" />{title}
			</h2>

			<div class="divider my-2 shrink-0"></div>

			<div class="flex flex-wrap items-center justify-between gap-x-4 gap-y-2">
				<span class="text-xl font-semibold">
					Activity of device <em>{geniusDevice.location}</em>
				</span>
				<label class="label cursor-pointer gap-2 py-0">
					<span>Alarms only</span>
					<input type="checkbox" class="toggle toggle-primary" bind:checked={onlyAlarms} />
				</label>
			</div>

			<div class="divider my-2 shrink-0"></div>

			{#if entries.length === 0}
				<div class="text-base-content text-center">
					{onlyAlarms ? 'No alarms' : 'No entries'}
				</div>
			{:else}
				<div class="max-h-160 min-h-0 overflow-x-auto overscroll-contain">
					<table class="table-pin-rows table w-full">
						<thead>
							<tr class="font-bold">
								<th align="left">Time</th>
								<th align="left">Event</th>
							</tr>
						</thead>
						<tbody>
							{#each entries as entry}
								{@const info = describeEvent(entry)}
								{@const Icon = ICONS[entry.type] ?? IconOther}
								{@const running =
									entry.type === EVENT_ALARM &&
									(entry.endingReason ?? GeniusAlarmEnding.AlarmActive) ===
										GeniusAlarmEnding.AlarmActive}
								<tr>
									<td align="left" class="align-top whitespace-nowrap">
										{#if entry.time}
											{formatDateTimeSeconds(entry.time)}
										{:else}
											<span class="text-base-content/50">Time unknown</span>
										{/if}
									</td>
									<td align="left">
										<span class="inline-flex items-start gap-2">
											<Icon
												class="mt-0.5 h-5 w-5 flex-shrink-0 {entry.type === EVENT_ALARM
													? 'text-error'
													: 'text-base-content/60'}"
											/>
											<span>
												<span class="font-medium {running ? 'text-error' : ''}">{info.title}</span>
												{#if info.detail}
													<br /><span class="text-base-content/60 text-sm">{info.detail}</span>
												{/if}
											</span>
										</span>
									</td>
								</tr>
							{/each}
						</tbody>
					</table>
				</div>
			{/if}

			{#if alarmsTotal > alarmsInLog}
				<div class="text-base-content/60 mt-2 text-sm">
					{alarmsTotal} alarms in total - the log keeps the newest entries only.
				</div>
			{/if}

			<div class="divider my-2 shrink-0"></div>

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
				{#if onDetails}
					<button
						class="btn btn-primary text-primary-content inline-flex items-center"
						onclick={() => {
							modals.close();
							onDetails?.();
						}}
						type="button"
					>
						<Details class="h-5 w-5" />
						<span>Details</span>
					</button>
				{/if}
			</div>
		</div>
	</div>
{/if}
