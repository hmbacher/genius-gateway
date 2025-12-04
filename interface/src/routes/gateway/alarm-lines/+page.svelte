<script lang="ts">
	import type { PageData } from './$types';
	import { goto } from '$app/navigation';
	import { modals } from 'svelte-modals';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { AlarmLines, AlarmLine } from '$lib/types/models';
	import { AlarmLineAcquisition } from '$lib/types/enums';
	import { jsonDateReviver, downloadObjectAsJson } from '$lib/utils/misc';
	import { onMount, onDestroy } from 'svelte';
	import { socket } from '$lib/stores/socket';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
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
	import SpinnerSmall from '$lib/components/SpinnerSmall.svelte';

	const BROADCAST_ID = 0xffffffff; // 4294967295

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	let alarmLines: AlarmLines = $state({ lines: [] });

	let activeActions = $state({
		lineTestStart: [] as boolean[],
		lineTestStop: [] as boolean[],
		fireAlarmStart: [] as boolean[],
		fireAlarmStop: [] as boolean[]
	});

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

	type AlarmLineActionFinishedEvent = {
		timedOut: boolean;
	};

	onMount(() => {
		// Event that signals a new alarm line has been detected (by reception of a Genius packet)
		socket.on<NewAlarmLineEvent>('new-alarm-line', (data) => {
			getAlarmLines(); // Reload alarm lines
			notifications.success('New alarm line detected.', 3000);
		});

		// Event that signals an action has been finished
		socket.on('alarm-line-action-finished', (data: AlarmLineActionFinishedEvent) => {
			// Reset the active action flags
			resetActiveActions();
			// Notify the user
			if (data.timedOut) {
				notifications.error(
					`The triggered action timed out.`,
					5000
				);
			} else {
				notifications.success(
					`The triggered action finished successfully.`,
					3000
				);
			}
		});
	});

	onDestroy(() => socket.off('new-alarm-line'));

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
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function postAlarmLines(lines: AlarmLines) {
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
				notifications.success('Alarm lines updated.', 3000);
				alarmLines = JSON.parse(await response.text(), jsonDateReviver);
			} else {
				notifications.error('Updating alarm lines failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
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
				alarmLines = alarmLines;
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
				let success = await postAlarmLineAction(alarmLines.lines[index].id, 'line-test-start');
				if (success) {
					notifications.success('Triggered line test start.', 3000);
					activeActions.lineTestStart[index] = true;
				} else {
					notifications.error('Failed to trigger line test start.', 3000);
				}
			}
		});
	}

	async function handleLineTestStop(index: number) {
		let success = await postAlarmLineAction(alarmLines.lines[index].id, 'line-test-stop');
		if (success) {
			notifications.success('Triggered line test stop.', 3000);
		} else {
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
				let success = await postAlarmLineAction(alarmLines.lines[index].id, 'fire-alarm-start');

				if (success) {
					notifications.success('Triggered fire alarm.', 3000);
				} else {
					notifications.error('Failed to trigger fire alarm.', 3000);
				}
			}
		});
	}

	async function handleFireAlarmStop(index: number) {
		let success = await postAlarmLineAction(alarmLines.lines[index].id, 'fire-alarm-stop');
		if (success) {
			notifications.success('Stopped fire alarm.', 3000);
		} else {
			notifications.error('Failed to stop fire alarm.', 3000);
		}
	}

	let files: any = $state();
	let fileInput = $state<HTMLInputElement>();

	$effect(() => {
		if (files) {
			// Note that `files` is of type `FileList`, not an Array:
			// https://developer.mozilla.org/en-US/docs/Web/API/FileList
			const reader = new FileReader();
			reader.onload = () => {
				const fileContent = reader.result as string;
				try {
					const parsedData = JSON.parse(fileContent, jsonDateReviver) as AlarmLines;
					if (parsedData) {
						alarmLines = parsedData;
						notifications.success('Alarm lines imported.', 3000);
						postAlarmLines(alarmLines);
					} else {
						notifications.error('Invalid alarm lines format.', 3000);
					}
				} catch (error) {
					console.error('Error parsing file:', error);
					notifications.error('Error parsing file.', 3000);
				}

				// Reset files and clear input value to allow re-selection of the same file
				files = null;
				if (fileInput) {
					fileInput.value = '';
				}
			};

			reader.onerror = (ev) => {
				console.log('Error reading the file:', ev);
				notifications.error('Error reading file.', 3000);
				// Reset files and clear input value on error to allow re-selection
				files = null;
				if (fileInput) {
					fileInput.value = '';
				}
			};

			reader.readAsText(files[0]);
		}
	});
</script>

{#if $user.admin}
	<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
		<SettingsCard collapsible={false} maxwidth="max-w-3xl">
			{#snippet icon()}
				<Ring class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
			{/snippet}
			{#snippet title()}
				<span>Alarm Lines</span>
			{/snippet}
			{#await getAlarmLines()}
				<Spinner />
			{:then nothing}
				<div class="relative w-full overflow-visible">
					<div class="flex flex-row absolute right-0 -top-13 gap-2 justify-end">
						<div class="tooltip tooltip-left" data-tip="Add alarm line">
							<button
								class="btn btn-primary text-primary-content btn-md"
								onclick={handleNewAlarmLine}
							>
								<Add class="h-6 w-6" />
							</button>
						</div>
						<div
							class="tooltip tooltip-left"
							data-tip="Load alarm lines from file"
						>
							<label for="upload" class="btn btn-primary text-primary-content btn-md">
								<Load class="h-6 w-6" />
							</label>
							<input bind:files bind:this={fileInput} id="upload" type="file" class="hidden" />
						</div>
						<div class="tooltip tooltip-left" data-tip="Save alarm lines to file">
							<button
								class="btn btn-primary text-primary-content btn-md"
								onclick={() => downloadObjectAsJson(alarmLines, 'genius-alarm-lines')}
							>
								<Save class="h-6 w-6" />
							</button>
						</div>
					</div>

					{#if alarmLines.lines.length === 0}
						<div class="divider my-0"></div>
						<div class="flex flex-col items-center justify-center p-4 text-sm text-gray-500">
							<p class="mb-4 font-semibold">No alarm lines registered yet.</p>
							<p class="mx-20 text-center">
								Click the "+" button to manually add an alarm line or start the comissioning
								procedure of your smoke detectors.
							</p>
						</div>
					{:else}
						<div
							class="overflow-x-auto"
							transition:slide|local={{ duration: 300, easing: cubicOut }}
						>
							<table class="table w-full table-auto">
								<thead>
									<tr class="font-bold">
										<th align="left">ID</th>
										<th align="left">Name</th>
										<th align="left">Registered</th>
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
													class="{line.id === BROADCAST_ID ? 'text-base-content/50' : ''} "
													>{line.id}</td
												>
												<td
													align="left"
													class="{line.id === BROADCAST_ID ? 'text-base-content/50' : ''} "
													>{line.name}</td
												>
												<td align="left"
													>{line.id != BROADCAST_ID
														? line.created.toLocaleString('de-DE', {
																day: '2-digit',
																month: '2-digit',
																year: 'numeric',
																hour: '2-digit',
																minute: '2-digit',
																second: '2-digit'
															})
														: ''}
												</td>
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
														{/if}
													{/if}
												</td>

												<td align="right">
													<span class="my-auto inline-flex flex-row space-x-2">
														<div class="tooltip tooltip-left" data-tip="Edit alarm line">
															<button
																class="btn btn-ghost btn-circle btn-xs"
																onclick={() => handleEdit(index)}
																disabled={line.id === BROADCAST_ID}
															>
																<Edit class="h-6 w-6" />
															</button>
														</div>
														<div class="tooltip tooltip-left" data-tip="Delete alarm line">
															<button
																class="btn btn-ghost btn-circle btn-xs"
																onclick={() => confirmDelete(index)}
																disabled={line.id === BROADCAST_ID}
															>
																<Delete class="h-6 w-6" />
															</button>
														</div>
														{#if !activeActions.lineTestStart[index]}
															<div class="tooltip tooltip-left" data-tip="Start line test">
																<button
																	class="btn btn-ghost btn-circle btn-xs"
																	onclick={() => handleLineTestStart(index)}
																	disabled={isActionActive}
																>
																	<LineTestStart class="h-6 w-6" />
																</button>
															</div>
														{:else}
															<SpinnerSmall />
														{/if}
														{#if !activeActions.lineTestStop[index]}
														<div class="tooltip tooltip-left" data-tip="Stop line test">
															<button
																class="btn btn-ghost btn-circle btn-xs"
																onclick={() => handleLineTestStop(index)}
																disabled={isActionActive}
															>
																<LineTestStop class="h-6 w-6" />
															</button>
														</div>
														{:else}
															<SpinnerSmall />
														{/if}
														{#if !activeActions.fireAlarmStart[index]}
														<div
															class="tooltip tooltip-left tooltip-error"
															data-tip="Trigger fire alarm"
														>
															<button
																class="btn btn-ghost btn-circle btn-xs"
																onclick={() => handleFireAlarmStart(index)}
																disabled={isActionActive}
															>
																<Flame class="h-6 w-6 {!isActionActive ? 'text-error' : ''}" />
															</button>
														</div>
														{:else}
															<SpinnerSmall />
														{/if}
														{#if !activeActions.fireAlarmStop[index]}
														<div class="tooltip tooltip-left" data-tip="Stop fire alarm">
															<button
																class="btn btn-ghost btn-circle btn-xs"
																onclick={() => handleFireAlarmStop(index)}
																disabled={isActionActive}
															>
																<FlameOff class="h-6 w-6" />
															</button>
														</div>
														{:else}
															<SpinnerSmall />
														{/if}
													</span>
												</td>
											</tr>
										{/if}
									{/each}
								</tbody>
							</table>
						</div>
					{/if}
				</div>
			{/await}
		</SettingsCard>
	</div>
{:else}
	{goto('/')}
{/if}
