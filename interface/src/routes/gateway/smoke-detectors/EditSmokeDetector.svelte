<script lang="ts">
	import { onMount } from 'svelte';
	import { modals } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import type { GeniusComponent, GeniusAlarm, GeniusDevice } from '$lib/types/models';
	import {
		GeniusDeviceRegistration,
		GeniusSmokeDetector,
		GeniusRadioModule
	} from '$lib/types/enums';
	import DateInput from '$lib/components/DateInput.svelte';
	import Cancel from '~icons/tabler/x';
	import Save from '~icons/tabler/device-floppy';
	import IconSmokeDetector from '~icons/custom-icons/smoke-detector-m';
	import IconRadioModule from '~icons/custom-icons/radio-module';
	import IconMapPin from '~icons/tabler/map-pin';
	import Warning from '~icons/tabler/alert-triangle';

	// provided by <Modals />

	interface Props {
		isOpen: boolean;
		title: string;
		onSaveGeniusDevice: any;
		geniusDevice?: any;
	}

	let {
		isOpen,
		title,
		onSaveGeniusDevice,
		geniusDevice = $bindable({
			smokeDetector: {
				model: GeniusSmokeDetector.GeniusPlusX,
				sn: 0,
				productionDate: new Date()
			} as GeniusComponent,
			radioModule: {
				model: GeniusRadioModule.FmBasisX,
				sn: 0,
				productionDate: new Date()
			} as GeniusComponent,
			location: '',
			registration: GeniusDeviceRegistration.Manual, // Default to manual registration
			alarms: [] as GeniusAlarm[] // No alarms by default
		} as GeniusDevice)
	}: Props = $props();

	let smokeDetectorModels = [
		{
			id: 0,
			text: `Genius Genius Plus X`
		}
	];

	let radioModuleModels = [
		{
			id: 0,
			text: `Genius FM Basis X`
		}
	];

	let minSN = 1;
	let maxSN = 4294967294;

	let minLocationLength = 1;
	let maxLocationLength = 40;

	let formErrors = $state({
		smokeDetector: {
			sn: false,
			productionDate: false
		},
		radioModule: {
			sn: false,
			productionDate: false
		},
		location: false
	});

	function handleSave() {
		let valid = true;

		// --- Validate Smoke Detector
		// Validate if smoke detector SN is within range
		if (geniusDevice.smokeDetector.sn < minSN || geniusDevice.smokeDetector.sn.length > maxSN) {
			formErrors.smokeDetector.sn = true;
			valid = false;
		} else {
			formErrors.smokeDetector.sn = false;
		}

		// --- Validate Radio Module
		// Validate if smoke detector SN is within range
		if (geniusDevice.radioModule.sn < minSN || geniusDevice.radioModule.sn.length > maxSN) {
			formErrors.radioModule.sn = true;
			valid = false;
		} else {
			formErrors.radioModule.sn = false;
		}

		// --- Validate Production Date
		// Check production date (if applicable)
		formErrors.smokeDetector.productionDate = false;
		if (geniusDevice.smokeDetector.productionDate) {
			if (isNaN(geniusDevice.smokeDetector.productionDate)) {
				formErrors.smokeDetector.productionDate = true;
				valid = false;
			}
		}

		// --- Validate Production Date
		// Check production date (if applicable)
		formErrors.radioModule.productionDate = false;
		if (geniusDevice.radioModule.productionDate) {
			if (isNaN(geniusDevice.radioModule.productionDate)) {
				formErrors.radioModule.productionDate = true;
				valid = false;
			}
		}

		// Validate location
		if (
			geniusDevice.location.length < minLocationLength ||
			geniusDevice.location.length > maxLocationLength
		) {
			formErrors.location = true;
			valid = false;
		} else {
			formErrors.location = false;
		}

		// Callback on saving
		if (valid) {
			onSaveGeniusDevice(geniusDevice);
		}
	}

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
			<form
				class="form-control text-base-content mb-1 w-full"
				onsubmit={preventDefault(handleSave)}
				novalidate
			>
				<span class="inline-flex items-baseline">
					<IconSmokeDetector class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
					<span class="text-xl font-semibold">Smoke Detector</span>
				</span>

				<div class="flex flex-col lg:flex-row lg:gap-4">
					<div class="flex-1">
						<label class="label" for="smokeDetectorModel">
							<span class="label-text">Model</span>
						</label>
						<select
							class="select"
							id="smokeDetectorModel"
							bind:value={geniusDevice.smokeDetector.model}
						>
							{#each smokeDetectorModels as model}
								<option value={model.id}>
									{model.text}
								</option>
							{/each}
						</select>
					</div>

					<div class="flex-1">
						<label class="label" for="smokeDetectorProductionDate">
							<span class="label-text text-md">Production Date</span>
						</label>
						{#if geniusDevice.smokeDetector.productionDate}
							<DateInput
								bind:date={geniusDevice.smokeDetector.productionDate}
								id="smokeDetectorProductionDate"
							/>
							{#if formErrors.smokeDetector.productionDate}
								<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
									<label for="smokeDetectorProductionDate" class="label">
										<span
											class="label-text-alt text-error {formErrors.smokeDetector.productionDate
												? ''
												: 'hidden'}"
										>
											Please set a valid date.
										</span>
									</label>
								</div>
							{/if}
						{:else}
							<input
								id="smokeDetectorProductionDate"
								class="input input-bordered"
								value="Unknown"
								disabled
							/>
						{/if}
					</div>

					<div class="flex-1">
						<label class="label" for="smokeDetectorSN">
							<span class="label-text text-md">Serial Number</span>
						</label>
						<input
							type="number"
							min={minSN}
							max={maxSN}
							class="input input-bordered invalid:border-error w-full invalid:border-2"
							bind:value={geniusDevice.smokeDetector.sn}
							id="smokeDetectorSN"
						/>
						{#if formErrors.smokeDetector.sn}
							<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
								<label for="smokeDetectorSN" class="label">
									<span
										class="label-text-alt text-error {formErrors.smokeDetector.sn ? '' : 'hidden'}"
									>
										The serial number must be a valid number between <em>{minSN}</em> and
										<em>{maxSN}</em>.
									</span>
								</label>
							</div>
						{/if}
					</div>
				</div>

				<div class="alert alert-warning shadow-lg mt-1">
					<Warning class="h-6 w-6 shrink-0" />
					<span>
						The serial number is crucial for correctly assigning an alarm signal to the emitting
						smoke detector.
					</span>
				</div>

				<div class="divider my-2"></div>

				<span class="inline-flex items-baseline">
					<IconRadioModule class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
					<span class="text-xl font-semibold">Radio Module</span>
				</span>

				<div class="flex flex-col lg:flex-row lg:gap-4">
					<div class="flex-1">
						<label class="label" for="radioModuleModel">
							<span class="label-text">Model</span>
						</label>
						<select
							class="select"
							id="radioModuleModel"
							bind:value={geniusDevice.radioModule.model}
						>
							{#each radioModuleModels as model}
								<option value={model.id}>
									{model.text}
								</option>
							{/each}
						</select>
					</div>

					<div class="flex-1">
						<label class="label" for="radioModuleProductionDate">
							<span class="label-text text-md">Production Date</span>
						</label>
						{#if geniusDevice.radioModule.productionDate}
							<DateInput
								bind:date={geniusDevice.radioModule.productionDate}
								id="radioModuleProductionDate"
							/>
							{#if formErrors.radioModule.productionDate}
								<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
									<label for="radioModuleProductionDate" class="label">
										<span
											class="label-text-alt text-error {formErrors.radioModule.productionDate
												? ''
												: 'hidden'}"
										>
											Please set a valid date.
										</span>
									</label>
								</div>
							{/if}
						{:else}
							<input
								id="radioModuleProductionDate"
								class="input input-bordered"
								value="Unknown"
								disabled
							/>
						{/if}
					</div>

					<div class="flex-1">
						<label class="label" for="radioModuleSN">
							<span class="label-text text-md">Serial Number</span>
						</label>
						<input
							type="number"
							min={minSN}
							max={maxSN}
							class="input input-bordered invalid:border-error w-full invalid:border-2"
							bind:value={geniusDevice.radioModule.sn}
							id="radioModuleSN"
						/>
						{#if formErrors.radioModule.sn}
							<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
								<label for="radioModuleSN" class="label">
									<span
										class="label-text-alt text-error {formErrors.radioModule.sn ? '' : 'hidden'}"
									>
										The radio module serial number must be a valid number between <em>{minSN}</em>
										and
										<em>{maxSN}</em>.
									</span>
								</label>
							</div>
						{/if}
					</div>
				</div>

				<div class="divider my-2"></div>

				<span class="inline-flex items-baseline">
					<IconMapPin class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
					<span class="text-xl font-semibold">Location</span>
				</span>

				<div>
					<label class="label" for="location">
						<span class="label-text text-md">Mounting location (e.g. Living room)</span>
					</label>
					<input
						type="text"
						min="1"
						max="30"
						class="input input-bordered invalid:border-error w-full invalid:border-2"
						bind:value={geniusDevice.location}
						id="location"
					/>
					{#if formErrors.location}
						<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
							<label for="location" class="label">
								<span class="label-text-alt text-error {formErrors.location ? '' : 'hidden'}">
									Please set a location of length between <em>{minLocationLength}</em> and
									<em>{maxLocationLength}</em> characters.
								</span>
							</label>
						</div>
					{/if}
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
					>
						<Save class="mr-2 h-5 w-5" />
						<span>Save</span>
					</button>
				</div>
			</form>
		</div>
	</div>
{/if}
