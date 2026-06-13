<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import FieldError from '$lib/components/FieldError.svelte';
	import { inRange, hasLength } from '$lib/utils/validators';
	import type {
		GeniusSmokeDetectorInfo,
		GeniusRadioModuleInfo,
		GeniusAlarm,
		GeniusDevice
	} from '$lib/types/models';
	import {
		GeniusDeviceRegistration,
		GeniusSmokeDetector,
		GeniusRadioModule
	} from '$lib/types/enums';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import DateInput from '$lib/components/DateInput.svelte';
	import Cancel from '~icons/tabler/x';
	import Save from '~icons/tabler/device-floppy';
	import IconSmokeDetector from '~icons/custom-icons/smoke-detector-m';
	import IconRadioModule from '~icons/custom-icons/radio-module';
	import IconMapPin from '~icons/tabler/map-pin';
	import IconWarning from '~icons/tabler/alert-triangle';
	import InfoCircle from '~icons/tabler/info-circle';
	import IconExclamationCircle from '~icons/tabler/exclamation-circle';

	// provided by <Modals />

	interface Props extends ModalProps {
		title: string;
		onSaveGeniusDevice: (device: GeniusDevice) => void | Promise<void>;
		geniusDevice?: GeniusDevice;
		saveButtonLabel?: string;
	}

	let {
		isOpen,
		title,
		onSaveGeniusDevice,
		saveButtonLabel = 'Save',
		geniusDevice: _geniusDevice = {
			id: 0,
			smokeDetector: {
				model: GeniusSmokeDetector.GeniusPlusX,
				sn: 0,
				productionDate: new Date()
			} as GeniusSmokeDetectorInfo,
			radioModule: {
				model: GeniusRadioModule.FmBasisX,
				sn: 0
			} as GeniusRadioModuleInfo,
			location: '',
			registration: GeniusDeviceRegistration.Manual,
			isAlarming: false,
			alarms: [] as GeniusAlarm[]
		} as GeniusDevice
	}: Props = $props();

	// Make passed object reactive in EditSmokeDetector modal
	// https://github.com/sveltejs/svelte/issues/12320
	let geniusDevice = $state(_geniusDevice);

	const titleId = `edit-smoke-detector-title-${Math.random().toString(36).slice(2)}`;

	// Generate unique device ID with collision detection
	function generateUniqueDeviceId(): number {
		// Use 32-bit unsigned integer arithmetic to match backend (uint32_t)
		let candidateId = Math.floor(Date.now() / 1000) >>> 0; // >>> 0 converts to uint32

		// Simple linear scan for collision detection - efficient for small device counts
		while (geniusDevices.devices.some((device) => device.id === candidateId)) {
			candidateId = (candidateId + 1) >>> 0; // Ensure 32-bit wraparound
		}

		return candidateId;
	}

	// Generate unique ID for new devices (when id is 0 or undefined)
	if (!geniusDevice.id || geniusDevice.id === 0) {
		geniusDevice.id = generateUniqueDeviceId();
	}

	let smokeDetectorModels = [
		{ id: GeniusSmokeDetector.GeniusH, text: 'Genius H' },
		{ id: GeniusSmokeDetector.GeniusHx, text: 'Genius Hx' },
		{ id: GeniusSmokeDetector.GeniusPlus, text: 'Genius Plus' },
		{ id: GeniusSmokeDetector.GeniusPlusX, text: 'Genius Plus X' }
	];

	let radioModuleModels = [
		{ id: GeniusRadioModule.None, text: 'None' },
		{ id: GeniusRadioModule.FmBasis, text: 'FM.Basis' },
		{ id: GeniusRadioModule.FmPro, text: 'FM.Pro' },
		{ id: GeniusRadioModule.FmMcp, text: 'FM.MCP' },
		{ id: GeniusRadioModule.FmBasisX, text: 'FM.Basis X' },
		{ id: GeniusRadioModule.FmProX, text: 'FM.Pro X' }
	];

	const minSN = 1;
	const maxSN = 4294967294;
	const minLocationLength = 1;
	const maxLocationLength = 40;

	const isAutoDetected =
		geniusDevice.registration === GeniusDeviceRegistration.GeniusPacket ||
		geniusDevice.registration === GeniusDeviceRegistration.Acoustic;
	const isAcoustic = geniusDevice.registration === GeniusDeviceRegistration.Acoustic;
	const registrationLabel = isAcoustic ? 'acoustic readout' : 'Genius radio packet';

	const smokeDetectorSNRangeError = $derived(
		!isAutoDetected && !inRange(geniusDevice.smokeDetector.sn, minSN, maxSN)
	);
	const smokeDetectorSNDuplicateError = $derived(
		geniusDevices.devices.some(
			(d) => d.id !== geniusDevice.id && d.smokeDetector.sn === geniusDevice.smokeDetector.sn
		)
	);
	const hasRadioModule = $derived(geniusDevice.radioModule.model !== GeniusRadioModule.None);
	const radioModuleSNRangeError = $derived(
		!isAutoDetected && hasRadioModule && !inRange(geniusDevice.radioModule.sn, minSN, maxSN)
	);
	const radioModuleSNDuplicateError = $derived(
		hasRadioModule &&
			geniusDevices.devices.some(
				(d) => d.id !== geniusDevice.id && d.radioModule.sn === geniusDevice.radioModule.sn
			)
	);
	const productionDateError = $derived(
		!isAutoDetected &&
			!!geniusDevice.smokeDetector.productionDate &&
			isNaN(geniusDevice.smokeDetector.productionDate.getTime())
	);
	const locationError = $derived(!hasLength(geniusDevice.location, minLocationLength, maxLocationLength));
	const hasErrors = $derived(
		smokeDetectorSNRangeError ||
			smokeDetectorSNDuplicateError ||
			radioModuleSNRangeError ||
			radioModuleSNDuplicateError ||
			productionDateError ||
			locationError
	);

	function handleSave() {
		if (!hasErrors) {
			if (!hasRadioModule) {
				geniusDevice.radioModule.sn = 0;
			}
			onSaveGeniusDevice(geniusDevice);
		}
	}
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
					handleSave();
				}}
				novalidate
			>
				{#if isAutoDetected}
					<div class="alert alert-info alert-soft gap-2 mb-2 p-3">
						<InfoCircle class="h-5 w-5 shrink-0 self-start mt-0.5" />
						<span class="text-sm"
							>Some fields were detected via {registrationLabel} and cannot be changed.</span
						>
					</div>
				{/if}
				<span class="inline-flex items-center">
					<IconSmokeDetector class="mr-2 h-6 w-6" />
					<span class="text-xl font-semibold">Smoke Detector</span>
				</span>

				<div class="flex flex-col lg:flex-row lg:gap-4">
					<div class="flex-1">
						<label class="label" for="smokeDetectorModel">Model</label>
						<select
							class="select select-bordered w-full pl-3"
							id="smokeDetectorModel"
							disabled={isAcoustic}
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
						<label class="label" for="smokeDetectorProductionDate">Production Date</label>
						{#if geniusDevice.smokeDetector.productionDate}
							<DateInput
								bind:date={geniusDevice.smokeDetector.productionDate}
								id="smokeDetectorProductionDate"
								disabled={isAutoDetected}
							/>
							<FieldError show={productionDateError} message="Please set a valid date." />
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
						<label class="label" for="smokeDetectorSN">Serial Number</label>
						<label
							for="smokeDetectorSN"
							class="input input-bordered w-full {smokeDetectorSNRangeError || smokeDetectorSNDuplicateError ? 'border-error border-2' : ''}"
						>
							<input
								type="number"
								min={minSN}
								max={maxSN}
								disabled={isAutoDetected}
								class=""
								bind:value={geniusDevice.smokeDetector.sn}
								id="smokeDetectorSN"
							/>
						</label>
						<FieldError show={smokeDetectorSNRangeError} message="The serial number must be a valid number between {minSN} and {maxSN}." />
						<FieldError show={smokeDetectorSNDuplicateError} message="This smoke detector serial number is already used by another device." />
						{#if !isAutoDetected}
							<div class="alert mt-1 gap-2 p-2">
								<IconExclamationCircle class="h-5 w-5 shrink-0 mt-0.5" />
								<span class="text-sm">Make sure the serial number is correct.</span>
							</div>
						{/if}
					</div>
				</div>

				<div class="divider my-2"></div>

				<span class="inline-flex items-center">
					<IconRadioModule class="mr-2 h-6 w-6" />
					<span class="text-xl font-semibold">Radio Module</span>
				</span>

				<div class="flex flex-col lg:flex-row lg:gap-4">
					<div class="flex-1">
						<label class="label" for="radioModuleModel">Model</label>
						<select
							class="select select-bordered w-full pl-3"
							id="radioModuleModel"
							disabled={isAcoustic}
							bind:value={geniusDevice.radioModule.model}
						>
							{#each radioModuleModels as model}
								<option value={model.id}>
									{model.text}
								</option>
							{/each}
						</select>
					</div>

					{#if geniusDevice.radioModule.model !== GeniusRadioModule.None}
						<div class="flex-1" transition:slide|local={{ duration: 300, easing: cubicOut }}>
							<label class="label" for="radioModuleSN">Serial Number</label>
							<label
								for="radioModuleSN"
								class="input input-bordered w-full {radioModuleSNRangeError || radioModuleSNDuplicateError ? 'border-error border-2' : ''}"
							>
								<input
									type="number"
									min={minSN}
									max={maxSN}
									disabled={isAutoDetected}
									class=""
									bind:value={geniusDevice.radioModule.sn}
									id="radioModuleSN"
								/>
							</label>
							<FieldError show={radioModuleSNRangeError} message="The serial number must be a valid number between {minSN} and {maxSN}." />
							<FieldError show={radioModuleSNDuplicateError} message="This radio module serial number is already used by another device." />
						</div>
					{/if}
				</div>

				<div class="divider my-2"></div>

				<span class="inline-flex items-center">
					<IconMapPin class="mr-2 h-6 w-6" />
					<span class="text-xl font-semibold">Location</span>
				</span>

				<div>
					<label class="label" for="location">Mounting location (e.g. Living room)</label>
					<input
						type="text"
						minlength={minLocationLength}
						maxlength={maxLocationLength}
						class="input input-bordered invalid:border-error w-full invalid:border-2"
						bind:value={geniusDevice.location}
						id="location"
					/>
					<FieldError show={locationError} message="Please set a location of length between {minLocationLength} and {maxLocationLength} characters." />
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
						<Cancel class="h-5 w-5" />
						<span>Cancel</span>
					</button>
					<button
						class="btn btn-primary text-primary-content inline-flex items-center"
						type="submit"
						disabled={hasErrors}
					>
						<Save class="h-5 w-5" />
						<span>{saveButtonLabel}</span>
					</button>
				</div>
			</form>
		</div>
	</div>
{/if}
