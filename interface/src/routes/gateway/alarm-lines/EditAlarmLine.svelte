<script lang="ts">
	import { modals } from 'svelte-modals';
	import type { Action } from 'svelte/action';
	import { fly } from 'svelte/transition';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import type { AlarmLine } from '$lib/types/models';
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
		alarmLine = $bindable({
				id: 0,
				name: '',
				created: new Date(),
				acquisition: 0
			})
	}: Props = $props();

	let _orgID = $state.snapshot(alarmLine.id);	// Store the original ID to check for duplicates

	let minID = 0x00000001;
	let maxID = 0xfffffffe;

	let minNameLength = 1;
	let maxNameLength = 100;

	let formErrors = $state({
		id: {
			range: alarmLine.id < minID || alarmLine.id > maxID,
			exists: idExists(alarmLine.id)
		},
		name: alarmLine.name.length < minNameLength || alarmLine.name.length > maxNameLength
	});

	let hasformErrors = $derived(formErrors.id.range || formErrors.id.exists || formErrors.name);

	function idExists(id: number) {
		return existingAlarmLines.some((line) => line.id === id) && id !== _orgID;
	}

	function preventDefault(fn) {
		return function (event) {
			event.preventDefault();
			fn.call(this, event);
		};
	}

	const focus: Action = (node) => {
		// the node has been mounted in the DOM
		node.focus();
	};
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
			<form
				class="form-control text-base-content mb-1 w-full"
				onsubmit={preventDefault(onSaveAlarmLine(alarmLine))}
				novalidate
			>
				<div class="flex flex-col gap-2">
					<div class="flex-1">
						<label class="label" for="radioModuleSN">
							<span class="label-text text-md">ID</span>
						</label>
						<input
							type="number"
							placeholder="Provide a unique ID for the alarm line"
							min={minID}
							max={maxID}
							required
							class="input input-bordered invalid:border-error w-full invalid:border-2 {formErrors
								.id.range || formErrors.id.exists
								? 'border-error border-2'
								: ''}"
							bind:value={alarmLine.id}
							id="AlramLineID"
							oninput={() => {
								formErrors.id.range = alarmLine.id < minID || alarmLine.id > maxID;
								formErrors.id.exists = idExists(alarmLine.id);
							}}
							use:focus
						/>
						{#if formErrors.id.range}
							<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
								<label for="AlramLineID" class="label">
									<span class="label-text-alt text-error text-xs text-wrap">
										The alarm line ID must be a valid number between {minID} and
										{maxID}.
									</span>
								</label>
							</div>
						{:else if formErrors.id.exists}
							<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
								<label for="AlramLineID" class="label">
									<span class="label-text-alt text-error text-xs text-wrap">
										This alarm line ID is already registered.
									</span>
								</label>
							</div>
						{/if}
					</div>
					<div class="flex-1">
						<label class="label" for="location">
							<span class="label-text text-md">Name</span>
						</label>
						<input
							type="text"
							placeholder="Provide a name for the alarm line"
							min="1"
							max="30"
							required
							class="input input-bordered invalid:border-error w-full invalid:border-2"
							bind:value={alarmLine.name}
							id="AlarmLineName"
							oninput={() => {
								formErrors.name =
									alarmLine.name.length < minNameLength || alarmLine.name.length > maxNameLength;
							}}
						/>
						{#if formErrors.name}
							<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
								<label for="AlarmLineName" class="label">
									<span class="label-text-alt text-error text-xs text-wrap">
										Please set a name of length between {minNameLength} and
										{maxNameLength} characters.
									</span>
								</label>
							</div>
						{/if}
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
						disabled={hasformErrors}
					>
						<Save class="mr-2 h-5 w-5" />
						<span>Save</span>
					</button>
				</div>
			</form>
		</div>
	</div>
{/if}
