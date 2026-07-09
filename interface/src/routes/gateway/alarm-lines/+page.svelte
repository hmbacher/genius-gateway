<script lang="ts">
	import type { PageData } from './$types';
	import { goto } from '$app/navigation';
	import { modals } from 'svelte-modals';
	import { slide, fade } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { AlarmLines, AlarmLine } from '$lib/types/models';
	import { AlarmLineAcquisition, GeniusDeviceRegistration } from '$lib/types/enums';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import { hasAutomaticLineDetection } from '$lib/genius/line';
	import { jsonDateReviver, downloadObjectAsJson } from '$lib/utils/misc';
	import { onMount, onDestroy } from 'svelte';
	import { socket } from '$lib/stores/socket';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import InfoDialog from '$lib/components/InfoDialog.svelte';
	import InfoPopover from '$lib/components/InfoPopover.svelte';
	import EditAlarmLine from './EditAlarmLine.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import Delete from '~icons/tabler/trash';
	import Add from '~icons/tabler/circle-plus';
	import Edit from '~icons/tabler/pencil';
	import Ring from '~icons/tabler/topology-ring-2';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Save from '~icons/tabler/device-floppy';
	import Load from '~icons/tabler/folder-open';
	import LineTestStart from '~icons/tabler/location';
	import LineTestStop from '~icons/tabler/location-off';
	import Flame from '~icons/tabler/flame-filled';
	import FlameOff from '~icons/tabler/flame-off';
	import Manual from '~icons/tabler/forms';
	import Automatic from '~icons/tabler/access-point';
	import Microphone from '~icons/tabler/microphone';
	import Radar from '~icons/tabler/radar';
	import SpinnerSmall from '$lib/components/SpinnerSmall.svelte';

	const BROADCAST_ID = 0xffffffff; // 4294967295

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	$effect(() => {
		if (!$user.admin) goto('/');
	});

	let alarmLines: AlarmLines = $state({ lines: [] });
	let alarmLinesLoaded = $state(false);

	let activeActions = $state({
		lineTestStart: [] as boolean[],
		lineTestStop: [] as boolean[],
		fireAlarmStart: [] as boolean[],
		fireAlarmStop: [] as boolean[]
	});

	// Avoid hiding the column before devices have loaded; default to showing it.
	let hasXSeriesHardware = $derived(
		!geniusDevices.isLoaded ||
			geniusDevices.devices.some((d) => hasAutomaticLineDetection(d.radioModule.model))
	);

	let isActionActive = $derived(
		activeActions.lineTestStart.some((active) => active) ||
			activeActions.lineTestStop.some((active) => active) ||
			activeActions.fireAlarmStart.some((active) => active) ||
			activeActions.fireAlarmStop.some((active) => active)
	);

	function resetActiveActions() {
		activeActions.lineTestStart.fill(false);
		activeActions.lineTestStop.fill(false);
		activeActions.fireAlarmStart.fill(false);
		activeActions.fireAlarmStop.fill(false);
	}

	type NewAlarmLineEvent = {
		newAlarmLineId: number;
	};

	type AlarmLineActionStartedEvent = {
		lineId: number;
		action: string;
	};

	type AlarmLineActionFinishedEvent = {
		timedOut: boolean;
	};

	function setActiveAction(lineId: number, action: string) {
		const index = alarmLines.lines.findIndex((line) => line.id === lineId);
		if (index === -1) return;
		switch (action) {
			case 'line-test-start':
				activeActions.lineTestStart[index] = true;
				break;
			case 'line-test-stop':
				activeActions.lineTestStop[index] = true;
				break;
			case 'fire-alarm-start':
				activeActions.fireAlarmStart[index] = true;
				break;
			case 'fire-alarm-stop':
				activeActions.fireAlarmStop[index] = true;
				break;
		}
	}

	onMount(() => {
		// Event that signals a new alarm line has been detected (by reception of a Genius packet)
		socket.on<NewAlarmLineEvent>('new-alarm-line', (data) => {
			getAlarmLines(); // Reload alarm lines
			notifications.success('New alarm line detected.', 3000);
		});

		// Event that signals an action has been started (from any trigger source:
		// Web UI click, HA button, or MQTT subscription). Idempotent with the
		// eager flag set in the click handlers.
		socket.on('alarm-line-action-started', (data: AlarmLineActionStartedEvent) => {
			setActiveAction(data.lineId, data.action);
		});

		// Event that signals an action has been finished
		socket.on('alarm-line-action-finished', (data: AlarmLineActionFinishedEvent) => {
			// Reset the active action flags
			resetActiveActions();
			// Notify the user
			if (data.timedOut) {
				notifications.error(`The triggered action timed out.`, 5000);
			} else {
				notifications.success(`The triggered action finished successfully.`, 3000);
			}
		});
	});

	onDestroy(() => {
		socket.off('new-alarm-line');
		socket.off('alarm-line-action-started');
		socket.off('alarm-line-action-finished');
	});

	async function getAlarmLines() {
		try {
			const response = await fetch('/rest/alarm-lines', {
				method: 'GET',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			alarmLines = JSON.parse(await response.text(), jsonDateReviver);
			alarmLinesLoaded = true;
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function postAlarmLines(
		lines: AlarmLines,
		opts: { suppressNotifications?: boolean } = {}
	): Promise<boolean> {
		try {
			const response = await fetch('/rest/alarm-lines', {
				method: 'POST',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(lines)
			});

			if (response.status == 200) {
				if (!opts.suppressNotifications)
					notifications.success('Alarm lines updated.', 3000);
				alarmLines = JSON.parse(await response.text(), jsonDateReviver);
				return true;
			} else {
				if (!opts.suppressNotifications)
					notifications.error('Updating alarm lines failed.', 3000);
				return false;
			}
		} catch (error) {
			console.error('Error:', error);
			if (!opts.suppressNotifications)
				notifications.error('Updating alarm lines failed.', 3000);
			return false;
		}
	}

	async function postAlarmLineAction(lineId: number, action: string): Promise<boolean> {
		let failed: boolean = true;

		try {
			const response = await fetch('/rest/alarm-lines/do', {
				method: 'POST',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify({
					lineId: lineId,
					action: action
				})
			});

			if (response.status == 200) {
				let data = await response.json();
				if ('success' in data && data.success === true) {
					failed = false;
				}
			}
		} catch (error) {
			console.error('Error:', error);
		}

		return !failed;
	}

	function confirmDelete(index: number) {
		modals.open(ConfirmDialog, {
			title: 'Confirm deletion',
			message:
				'Are you sure you want to delete the alarm line "' +
				alarmLines.lines[index].name +
				' (' +
				alarmLines.lines[index].id +
				')"?',
			labels: {
				cancel: { label: 'Abort', icon: Cancel },
				confirm: { label: 'Yes', icon: Check }
			},
			onConfirm: () => {
				alarmLines.lines.splice(index, 1);
				modals.close();
				postAlarmLines(alarmLines);
			}
		});
	}

	function handleEdit(index: number) {
		modals.open(EditAlarmLine, {
			title: 'Edit alarm line',
			alarmLine: $state.snapshot(alarmLines.lines[index]), // Deep copy
			existingAlarmLines: alarmLines.lines,
			onSaveAlarmLine: (editedAlarmLine: AlarmLine) => {
				alarmLines.lines[index] = editedAlarmLine;
				modals.close();
				postAlarmLines(alarmLines);
			}
		});
	}

	function handleNewAlarmLine() {
		modals.open(EditAlarmLine, {
			title: 'Add alarm line',
			existingAlarmLines: alarmLines.lines,
			onSaveAlarmLine: (newAlarmLine: AlarmLine) => {
				alarmLines.lines = [...alarmLines.lines, newAlarmLine];
				modals.close();
				postAlarmLines(alarmLines);
			}
		});
		//
	}

	function handleLineTestStart(index: number) {
		modals.open(ConfirmDialog, {
			title: 'Confirm line test',
			message:
				'Are you sure you want to trigger a line test for the alarm line "' +
				alarmLines.lines[index].name +
				' (' +
				alarmLines.lines[index].id +
				')"?',
			labels: {
				cancel: { label: 'Abort', icon: Cancel },
				confirm: { label: 'Yes', icon: Check }
			},
			onConfirm: async () => {
				modals.close();
				activeActions.lineTestStart[index] = true;
				const success = await postAlarmLineAction(alarmLines.lines[index].id, 'line-test-start');
				if (success) {
					notifications.success('Triggered line test start.', 3000);
				} else {
					activeActions.lineTestStart[index] = false;
					notifications.error('Failed to trigger line test start.', 3000);
				}
			}
		});
	}

	async function handleLineTestStop(index: number) {
		activeActions.lineTestStop[index] = true;
		const success = await postAlarmLineAction(alarmLines.lines[index].id, 'line-test-stop');
		if (success) {
			notifications.success('Triggered line test stop.', 3000);
		} else {
			activeActions.lineTestStop[index] = false;
			notifications.error('Failed to trigger line test stop.', 3000);
		}
	}

	function handleFireAlarmStart(index: number) {
		modals.open(ConfirmDialog, {
			title: 'Confirm fire alarm',
			message:
				'Are you sure you want to trigger a fire alarm for alarm line "' +
				alarmLines.lines[index].name +
				'" with ID ' +
				alarmLines.lines[index].id +
				'?<br /><span class="text-error">Note: Genius Gateway will ignore any self triggered fire alarm.</span>',
			labels: {
				cancel: { label: 'Abort', icon: Cancel },
				confirm: { label: 'Yes', icon: Check }
			},
			onConfirm: async () => {
				modals.close();
				activeActions.fireAlarmStart[index] = true;
				const success = await postAlarmLineAction(alarmLines.lines[index].id, 'fire-alarm-start');
				if (success) {
					notifications.success('Triggered fire alarm.', 3000);
				} else {
					activeActions.fireAlarmStart[index] = false;
					notifications.error('Failed to trigger fire alarm.', 3000);
				}
			}
		});
	}

	async function handleFireAlarmStop(index: number) {
		activeActions.fireAlarmStop[index] = true;
		const success = await postAlarmLineAction(alarmLines.lines[index].id, 'fire-alarm-stop');
		if (success) {
			notifications.success('Stopped fire alarm.', 3000);
		} else {
			activeActions.fireAlarmStop[index] = false;
			notifications.error('Failed to stop fire alarm.', 3000);
		}
	}

	function isValidAlarmLines(data: unknown): data is AlarmLines {
		if (!data || !Array.isArray((data as AlarmLines).lines)) return false;
		return (data as AlarmLines).lines.every(
			(l) =>
				typeof l.id === 'number' &&
				typeof l.name === 'string' &&
				typeof l.acquisition === 'number' &&
				(l.created instanceof Date || typeof l.created === 'string')
		);
	}

	let files = $state<FileList | undefined>(undefined);
	let fileInput = $state<HTMLInputElement | undefined>(undefined);

	$effect(() => {
		if (!files) return;
		// Note that `files` is of type `FileList`, not an Array:
		// https://developer.mozilla.org/en-US/docs/Web/API/FileList
		const reader = new FileReader();
		let cancelled = false;

		reader.onload = async () => {
			if (cancelled) return;
			const fileContent = reader.result as string;
			try {
				const parsedData = JSON.parse(fileContent, jsonDateReviver);
				if (!isValidAlarmLines(parsedData)) {
					notifications.error('Invalid alarm lines format.', 3000);
				} else {
					const ok = await postAlarmLines(parsedData, { suppressNotifications: true });
					if (ok) {
						notifications.success('Alarm lines imported.', 3000);
					} else {
						await getAlarmLines();
						modals.open(InfoDialog, {
							title: 'Import failed',
							message: 'The file could not be imported. Please try exporting a fresh backup.',
							variant: 'error',
							onDismiss: () => modals.close()
						});
					}
				}
			} catch (error) {
				console.error('Error parsing file:', error);
				notifications.error('Error parsing file.', 3000);
			}

			// Reset files and clear input value to allow re-selection of the same file
			files = undefined;
			if (fileInput) fileInput.value = '';
		};

		reader.onerror = (ev) => {
			if (cancelled) return;
			console.log('Error reading the file:', ev);
			notifications.error('Error reading file.', 3000);
			files = undefined;
			if (fileInput) fileInput.value = '';
		};

		reader.readAsText(files[0]);

		// Cleanup: if the component unmounts (or `files` changes) before the
		// read finishes, abort the FileReader and ignore any late callbacks.
		// Without this, onload/onerror would touch destroyed component state.
		return () => {
			cancelled = true;
			reader.onload = null;
			reader.onerror = null;
			if (reader.readyState === FileReader.LOADING) reader.abort();
		};
	});
</script>

{#if $user.admin}
	<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
		<SettingsCard collapsible={false} maxwidth="max-w-3xl">
			{#snippet icon()}
				<Ring class="h-6 w-6" />
			{/snippet}
			{#snippet title()}
				<span>Alarm Lines</span>
			{/snippet}
			{#snippet actions()}
				<div class="tooltip tooltip-bottom" data-tip="Add alarm line">
					<button
						class="btn btn-primary btn-md"
						aria-label="Add alarm line"
						disabled={!alarmLinesLoaded}
						onclick={handleNewAlarmLine}
					>
						<Add class="h-6 w-6" />
					</button>
				</div>
				<div class="tooltip tooltip-bottom" data-tip="Load alarm lines from file">
					<label
						for="upload"
						class="btn btn-primary btn-md"
						class:btn-disabled={!alarmLinesLoaded}
						aria-label="Load alarm lines from file"
					>
						<Load class="h-6 w-6" />
					</label>
					<input
						bind:files
						bind:this={fileInput}
						id="upload"
						type="file"
						class="hidden"
						disabled={!alarmLinesLoaded}
					/>
				</div>
				<div class="tooltip tooltip-left" data-tip="Save alarm lines to file">
					<button
						class="btn btn-primary btn-md"
						aria-label="Save alarm lines to file"
						disabled={!alarmLinesLoaded}
						onclick={() => downloadObjectAsJson(alarmLines, 'genius-alarm-lines')}
					>
						<Save class="h-6 w-6" />
					</button>
				</div>
			{/snippet}
			{#await getAlarmLines()}
				<Spinner />
			{:then nothing}
				{#if alarmLines.lines.length === 0}
					<div class="divider my-0"></div>
					<div class="flex flex-col items-center justify-center p-4 text-sm text-gray-500">
						<p class="mb-4 font-semibold">No alarm lines registered yet.</p>
						<p class="mx-20 text-center">
							Click the "+" button to manually add an alarm line or start the comissioning procedure
							of your smoke detectors.
						</p>
					</div>
				{:else}
					<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
						<!-- Mobile cards (< md) -->
						<div class="md:hidden space-y-2">
							{#each alarmLines.lines as line, index}
								{#if line.id !== BROADCAST_ID || (line.id === BROADCAST_ID && page.data.features.allow_broadcast)}
									<div
										class="rounded-box bg-base-100 p-3 flex flex-col gap-1.5 {line.id ===
										BROADCAST_ID
											? 'opacity-60'
											: ''}"
									>
										<!-- Row 1: name · id · acquisition icon -->
										<div class="flex items-center gap-2 overflow-hidden">
											<span class="text-lg font-bold truncate min-w-0">{line.name}</span>
											<span class="text-base text-base-content/40 shrink-0">#{line.id}</span>
											{#if line.id !== BROADCAST_ID}
												{#if line.acquisition === AlarmLineAcquisition.Manual}
													<div class="tooltip tooltip-top" data-tip="Manually added alarm line">
														<Manual class="flex-shrink-0 h-6 w-6 text-base-content/50" />
													</div>
												{:else if line.acquisition === AlarmLineAcquisition.GeniusPacket}
													<div
														class="tooltip tooltip-top"
														data-tip="Alarm line extracted from Genius radio packet"
													>
														<Automatic class="flex-shrink-0 h-6 w-6 text-base-content/50" />
													</div>
												{:else if line.acquisition === AlarmLineAcquisition.Acoustic}
													<div
														class="tooltip tooltip-top"
														data-tip="Alarm line discovered via acoustic device readout"
													>
														<Microphone class="flex-shrink-0 h-6 w-6 text-base-content/50" />
													</div>
												{:else if line.acquisition === AlarmLineAcquisition.SignalProbe}
													<div
														class="tooltip tooltip-top"
														data-tip="Alarm line discovered via signal probe"
													>
														<Radar class="flex-shrink-0 h-6 w-6 text-base-content/50" />
													</div>
												{/if}
											{/if}
										</div>
										{#if hasXSeriesHardware}
											<!-- Row 2: smoke detector locations -->
											<div class="text-base-content/70" transition:slide|local={{ duration: 200, easing: cubicOut }}>
												{#if !geniusDevices.isLoaded}
													<SpinnerSmall />
												{:else}
													{@const locations = geniusDevices.devices
														.filter(
															(d) =>
																d.registration === GeniusDeviceRegistration.Acoustic &&
																d.radioModule.lineId === line.id
														)
														.map((d) => d.location)
														.join(', ')}
													{#if locations}
														{locations}
													{:else}
														<span class="inline-flex items-center gap-1">
															<span class="italic text-base-content/40">No devices</span>
															<InfoPopover label="About smoke detector assignment">
																<p>
																	Only smoke detectors with <em>FM Basis X</em> /
																	<em>FM Pro X</em> radio module can be referenced here.
																</p>
															</InfoPopover>
														</span>
													{/if}
												{/if}
											</div>
										{/if}
										<!-- Row 3: action buttons -->
										<div class="flex items-center gap-0.5 border-t border-base-200 pt-1.5">
											<button
												class="btn btn-ghost btn-sm"
												aria-label="Edit alarm line"
												onclick={() => {
													(document.activeElement as HTMLElement)?.blur();
													handleEdit(index);
												}}
												disabled={line.id === BROADCAST_ID}
											>
												<Edit class="h-6 w-6" />
											</button>
											<button
												class="btn btn-ghost btn-sm"
												aria-label="Delete alarm line"
												onclick={() => {
													(document.activeElement as HTMLElement)?.blur();
													confirmDelete(index);
												}}
												disabled={line.id === BROADCAST_ID}
											>
												<Delete class="h-6 w-6" />
											</button>
											{#if !activeActions.lineTestStart[index]}
												<button
													class="btn btn-ghost btn-sm"
													aria-label="Start line test"
													onclick={() => {
														(document.activeElement as HTMLElement)?.blur();
														handleLineTestStart(index);
													}}
													disabled={isActionActive}
												>
													<LineTestStart class="h-6 w-6" />
												</button>
											{:else}
												<span class="btn btn-ghost btn-sm pointer-events-none">
													<SpinnerSmall />
												</span>
											{/if}
											{#if !activeActions.lineTestStop[index]}
												<button
													class="btn btn-ghost btn-sm"
													aria-label="Stop line test"
													onclick={() => handleLineTestStop(index)}
													disabled={isActionActive}
												>
													<LineTestStop class="h-6 w-6" />
												</button>
											{:else}
												<span class="btn btn-ghost btn-sm pointer-events-none">
													<SpinnerSmall />
												</span>
											{/if}
											{#if !activeActions.fireAlarmStart[index]}
												<button
													class="btn btn-ghost btn-sm"
													aria-label="Trigger fire alarm"
													onclick={() => {
														(document.activeElement as HTMLElement)?.blur();
														handleFireAlarmStart(index);
													}}
													disabled={isActionActive}
												>
													<Flame class="h-5 w-5 {!isActionActive ? 'text-error' : ''}" />
												</button>
											{:else}
												<span class="btn btn-ghost btn-sm pointer-events-none">
													<SpinnerSmall />
												</span>
											{/if}
											{#if !activeActions.fireAlarmStop[index]}
												<button
													class="btn btn-ghost btn-sm"
													aria-label="Stop fire alarm"
													onclick={() => handleFireAlarmStop(index)}
													disabled={isActionActive}
												>
													<FlameOff class="h-6 w-6" />
												</button>
											{:else}
												<span class="btn btn-ghost btn-sm pointer-events-none">
													<SpinnerSmall />
												</span>
											{/if}
										</div>
									</div>
								{/if}
							{/each}
						</div>
						<!-- Desktop table (≥ md) -->
						<div class="hidden md:block overflow-x-auto">
							<table class="table w-full table-auto">
								<thead>
									<tr class="font-bold">
										<th align="left">ID</th>
										<th align="left">Name</th>
										{#if hasXSeriesHardware}
											<th align="left" transition:fade={{ duration: 150 }}>
												<span class="inline-flex items-center gap-1">
													Smoke Detectors
													<InfoPopover label="About smoke detector assignment">
														<p>
															Only smoke detectors with <em>FM Basis X</em> / <em>FM Pro X</em>
															radio module can be referenced here.
														</p>
													</InfoPopover>
												</span>
											</th>
										{/if}
										<th align="center">Acquisition</th>
										<th align="right" class="pr-8">Manage</th>
									</tr>
								</thead>
								<tbody>
									{#each alarmLines.lines as line, index}
										{#if line.id !== BROADCAST_ID || (line.id === BROADCAST_ID && page.data.features.allow_broadcast)}
											<tr>
												<td
													align="left"
													class={line.id === BROADCAST_ID ? 'text-base-content/50' : ''}
													>{line.id}</td
												>
												<td
													align="left"
													class={line.id === BROADCAST_ID ? 'text-base-content/50' : ''}
													>{line.name}</td
												>
												{#if hasXSeriesHardware}
													<td align="left" class="text-sm" transition:fade={{ duration: 150 }}>
														{#if !geniusDevices.isLoaded}
															<div class="flex justify-center"><SpinnerSmall /></div>
														{:else}
															{@const locations = geniusDevices.devices
																.filter(
																	(d) =>
																		d.registration === GeniusDeviceRegistration.Acoustic &&
																		d.radioModule.lineId === line.id
																)
																.map((d) => d.location)
																.join(', ')}
															<span class={locations ? '' : 'flex justify-center'}
																>{locations || '-'}</span
															>
														{/if}
													</td>
												{/if}
												<td align="center">
													{#if line.id != BROADCAST_ID}
														{#if line.acquisition === AlarmLineAcquisition.Manual}
															<div class="tooltip tooltip-top" data-tip="Manually added alarm line">
																<Manual class="h-6 w-6" />
															</div>
														{:else if line.acquisition === AlarmLineAcquisition.GeniusPacket}
															<div
																class="tooltip tooltip-top"
																data-tip="Alarm line extracted from Genius radio packet"
															>
																<Automatic class="h-6 w-6" />
															</div>
														{:else if line.acquisition === AlarmLineAcquisition.Acoustic}
															<div
																class="tooltip tooltip-top"
																data-tip="Alarm line discovered via acoustic device readout"
															>
																<Microphone class="h-6 w-6" />
															</div>
														{:else if line.acquisition === AlarmLineAcquisition.SignalProbe}
															<div
																class="tooltip tooltip-top"
																data-tip="Alarm line discovered via signal probe"
															>
																<Radar class="h-6 w-6" />
															</div>
														{/if}
													{/if}
												</td>
												<td align="right">
													<span class="my-auto inline-flex flex-row">
														<div class="tooltip tooltip-left" data-tip="Edit alarm line">
															<button
																class="btn btn-ghost btn-circle btn-sm"
																aria-label="Edit alarm line"
																onclick={() => handleEdit(index)}
																disabled={line.id === BROADCAST_ID}
															>
																<Edit class="h-6 w-6" />
															</button>
														</div>
														<div class="tooltip tooltip-left" data-tip="Delete alarm line">
															<button
																class="btn btn-ghost btn-circle btn-sm"
																aria-label="Delete alarm line"
																onclick={() => confirmDelete(index)}
																disabled={line.id === BROADCAST_ID}
															>
																<Delete class="h-6 w-6" />
															</button>
														</div>
														{#if !activeActions.lineTestStart[index]}
															<div class="tooltip tooltip-left" data-tip="Start line test">
																<button
																	class="btn btn-ghost btn-circle btn-sm"
																	aria-label="Start line test"
																	onclick={() => handleLineTestStart(index)}
																	disabled={isActionActive}
																>
																	<LineTestStart class="h-6 w-6" />
																</button>
															</div>
														{:else}
															<span class="btn btn-ghost btn-circle btn-sm pointer-events-none">
																<SpinnerSmall />
															</span>
														{/if}
														{#if !activeActions.lineTestStop[index]}
															<div class="tooltip tooltip-left" data-tip="Stop line test">
																<button
																	class="btn btn-ghost btn-circle btn-sm"
																	aria-label="Stop line test"
																	onclick={() => handleLineTestStop(index)}
																	disabled={isActionActive}
																>
																	<LineTestStop class="h-6 w-6" />
																</button>
															</div>
														{:else}
															<span class="btn btn-ghost btn-circle btn-sm pointer-events-none">
																<SpinnerSmall />
															</span>
														{/if}
														{#if !activeActions.fireAlarmStart[index]}
															<div
																class="tooltip tooltip-left tooltip-error"
																data-tip="Trigger fire alarm"
															>
																<button
																	class="btn btn-ghost btn-circle btn-sm"
																	aria-label="Trigger fire alarm"
																	onclick={() => handleFireAlarmStart(index)}
																	disabled={isActionActive}
																>
																	<Flame class="h-6 w-6 {!isActionActive ? 'text-error' : ''}" />
																</button>
															</div>
														{:else}
															<span class="btn btn-ghost btn-circle btn-sm pointer-events-none">
																<SpinnerSmall />
															</span>
														{/if}
														{#if !activeActions.fireAlarmStop[index]}
															<div class="tooltip tooltip-left" data-tip="Stop fire alarm">
																<button
																	class="btn btn-ghost btn-circle btn-sm"
																	aria-label="Stop fire alarm"
																	onclick={() => handleFireAlarmStop(index)}
																	disabled={isActionActive}
																>
																	<FlameOff class="h-6 w-6" />
																</button>
															</div>
														{:else}
															<span class="btn btn-ghost btn-circle btn-sm pointer-events-none">
																<SpinnerSmall />
															</span>
														{/if}
													</span>
												</td>
											</tr>
										{/if}
									{/each}
								</tbody>
							</table>
						</div>
					</div>
				{/if}
			{/await}
		</SettingsCard>
	</div>
{/if}
