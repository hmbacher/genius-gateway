<script lang="ts">
	import { modals } from 'svelte-modals';
	import type { Action } from 'svelte/action';
	import { fly } from 'svelte/transition';
	import type { AlarmLine } from '$lib/types/models';
	import { AlarmLineAcquisition } from '$lib/types/enums';
	import FieldError from '$lib/components/FieldError.svelte';
	import { inRange, hasLength } from '$lib/utils/validators';
	import Cancel from '~icons/tabler/x';
	import Save from '~icons/tabler/device-floppy';

	// provided by <Modals />

	interface Props {
		isOpen: boolean;
		title: string;
		existingAlarmLines: AlarmLine[];
		onSaveAlarmLine: any;
		alarmLine?: AlarmLine;
	}

	let {
		isOpen,
		title,
		onSaveAlarmLine,
		existingAlarmLines,
		alarmLine: _alarmLine = {
			id: 0,
			name: '',
			created: new Date(),
			acquisition: AlarmLineAcquisition.Manual // Default to manually added
		}
	}: Props = $props();

	// Make passed object reactive in EditAlarmLine modal
	// https://github.com/sveltejs/svelte/issues/12320
	let alarmLine = $state(_alarmLine);

	const _orgID = $state.snapshot(alarmLine.id);

	const minID = 0x00000001;
	const maxID = 0xfffffffe;
	const minNameLength = 1;
	const maxNameLength = 100;

	const idRangeError = $derived(!inRange(alarmLine.id, minID, maxID));
	const idExistsError = $derived(
		existingAlarmLines.some((line) => line.id === alarmLine.id) && alarmLine.id !== _orgID
	);
	const nameError = $derived(!hasLength(alarmLine.name, minNameLength, maxNameLength));
	const hasErrors = $derived(idRangeError || idExistsError || nameError);

	const focus: Action = (node) => {
		// the node has been mounted in the DOM
		node.focus();
	};

	const titleId = `edit-alarm-line-title-${Math.random().toString(36).slice(2)}`;
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
			<form
				class="fieldset"
				onsubmit={(e) => {
					e.preventDefault();
					onSaveAlarmLine(alarmLine);
				}}
				novalidate
			>
				<div class="flex flex-col gap-2">
					<div class="flex-1">
						<label class="label" for="AlarmLineID">ID</label>
						<label
							for="AlarmLineID"
							class="input input-bordered w-full {idRangeError || idExistsError ? 'border-error border-2' : ''}"
						>
							<input
								type="number"
								placeholder="Provide a unique ID for the alarm line"
								min={minID}
								max={maxID}
								required
								disabled={alarmLine.acquisition === AlarmLineAcquisition.Acoustic ||
									alarmLine.acquisition === AlarmLineAcquisition.GeniusPacket}
								class=""
								bind:value={alarmLine.id}
								id="AlarmLineID"
								use:focus
							/>
						</label>
						{#if alarmLine.acquisition === AlarmLineAcquisition.Acoustic || alarmLine.acquisition === AlarmLineAcquisition.GeniusPacket}
							<label for="AlarmLineID" class="label">
								<span class="text-wrap pl-1">
									IDs of alarm lines discovered via {alarmLine.acquisition === AlarmLineAcquisition.Acoustic ? 'acoustic readout' : 'Genius radio packet'} cannot be changed.
								</span>
							</label>
						{:else}
							<FieldError show={idRangeError} message="The alarm line ID must be a valid number between {minID} and {maxID}." />
							<FieldError show={idExistsError} message="This alarm line ID is already registered." />
						{/if}
					</div>
					<div class="flex-1">
						<label class="label" for="AlarmLineName">Name</label>
						<input
							type="text"
							placeholder="Provide a name for the alarm line"
							min="1"
							max="30"
							required
							class="input input-bordered invalid:border-error w-full invalid:border-2"
							bind:value={alarmLine.name}
							id="AlarmLineName"
						/>
						<FieldError show={nameError} message="Please set a name of length between {minNameLength} and {maxNameLength} characters." />
					</div>
				</div>

				<div class="divider my-2"></div>
				<div class="flex justify-end gap-2">
					<button
						class="btn btn-neutral text-neutral-content inline-flex items-center"
						onclick={() => {
							modals.close(1);
						}}
						type="button"
					>
						<Cancel class="mr-2 h-5 w-5" />
						<span>Cancel</span>
					</button>
					<button
						class="btn btn-primary text-primary-content inline-flex items-center"
						type="submit"
						disabled={hasErrors}
					>
						<Save class="mr-2 h-5 w-5" />
						<span>Save</span>
					</button>
				</div>
			</form>
		</div>
	</div>
{/if}
