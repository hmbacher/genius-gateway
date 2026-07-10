<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import type { GeniusDevice } from '$lib/types/models';
	import { GeniusRadioModule } from '$lib/types/enums';
	import {
		LINE_MAJORS,
		LINE_MINORS,
		validMinorsFor,
		validateLine,
		isHMajorAllowed,
		isOldFmModule,
		DEFAULT_LINE_MAJOR,
		DEFAULT_LINE_MINOR
	} from '$lib/genius/line';
	import FieldError from '$lib/components/FieldError.svelte';
	import Cancel from '~icons/tabler/x';
	import Save from '~icons/tabler/device-floppy';
	import IconRadioModule from '~icons/custom-icons/radio-module';
	import IconForms from '~icons/tabler/forms';
	import IconWarning from '~icons/tabler/alert-triangle';

	interface Props extends ModalProps {
		title: string;
		geniusDevice: GeniusDevice;
		onSave: (device: GeniusDevice) => void | Promise<void>;
		saveButtonLabel?: string;
	}

	let { isOpen, title, geniusDevice: _geniusDevice, onSave, saveButtonLabel = 'Save' }: Props =
		$props();

	// Make the passed object reactive inside the modal (svelte #12320 workaround).
	let geniusDevice = $state(_geniusDevice);

	const titleId = `manual-line-entry-title-${Math.random().toString(36).slice(2)}`;
	const model = geniusDevice.radioModule.model;
	const hAllowed = isHMajorAllowed(model);

	// Seed from any existing line, else the FM.Basis factory default A.0. If the
	// seeded major is not selectable for this module (H on FM.Basis), use the default.
	const seededMajor = geniusDevice.radioModule.lineCharacter || DEFAULT_LINE_MAJOR;
	let major = $state(seededMajor === 'H' && !hAllowed ? DEFAULT_LINE_MAJOR : seededMajor);
	let minor = $state(geniusDevice.radioModule.lineNumber ?? DEFAULT_LINE_MINOR);

	const availableMinors = $derived(validMinorsFor(major, model) ?? [...LINE_MINORS]);

	// Keep the minor valid whenever the major changes (e.g. switching to H.x).
	$effect(() => {
		if (!availableMinors.includes(minor)) minor = availableMinors[0] ?? DEFAULT_LINE_MINOR;
	});

	const lineError = $derived(validateLine(major, minor, model));

	function handleSave() {
		if (lineError) return;
		geniusDevice.radioModule.lineCharacter = major;
		geniusDevice.radioModule.lineNumber = minor;
		geniusDevice.radioModule.lineManual = true;
		// Old modules expose no 32-bit Line-ID; keep it cleared for manual entry.
		if (isOldFmModule(model)) geniusDevice.radioModule.lineId = 0;
		onSave(geniusDevice);
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
			<h2
				id={titleId}
				class="text-base-content flex items-center gap-2 text-start text-2xl font-bold"
			>
				<IconForms class="text-primary h-7 w-7 flex-shrink-0" />{title}
			</h2>
			<div class="divider my-2"></div>
			<form
				class="fieldset"
				onsubmit={(e) => {
					e.preventDefault();
					handleSave();
				}}
				novalidate
			>
				<div class="alert alert-warning mb-2 gap-2 p-3">
					<IconWarning class="mt-0.5 h-5 w-5 shrink-0 self-start" />
					<span class="text-sm">
						This radio module does not report its alarm line. Set it to match the rotary switch
						(letter + digit) on the module.
					</span>
				</div>

				<span class="inline-flex items-center">
					<IconRadioModule class="mr-2 h-6 w-6" />
					<span class="text-xl font-semibold">Alarm Line</span>
				</span>

				<div class="flex flex-col lg:flex-row lg:gap-4">
					<div class="flex-1">
						<label class="label" for="lineMajor">Major (letter)</label>
						<select
							class="select select-bordered w-full pl-3"
							id="lineMajor"
							bind:value={major}
						>
							{#each LINE_MAJORS as m}
								<option value={m} disabled={m === 'H' && !hAllowed}>{m}</option>
							{/each}
						</select>
					</div>

					<div class="flex-1">
						<label class="label" for="lineMinor">Minor (digit)</label>
						<select
							class="select select-bordered w-full pl-3"
							id="lineMinor"
							bind:value={minor}
						>
							{#each availableMinors as n}
								<option value={n}>{n}</option>
							{/each}
						</select>
					</div>
				</div>

				<FieldError show={!!lineError} message={lineError ?? ''} />

				{#if model === GeniusRadioModule.FmBasis}
					<div class="mt-1 text-sm text-base-content/60">
						FM.Basis cannot use Sammelalarm (H) lines.
					</div>
				{/if}

				<div class="divider my-2"></div>
				<div class="flex justify-end gap-2">
					<button
						class="btn btn-neutral text-neutral-content inline-flex items-center"
						onclick={() => modals.close(1)}
						type="button"
					>
						<Cancel class="h-5 w-5" />
						<span>Cancel</span>
					</button>
					<button
						class="btn btn-primary text-primary-content inline-flex items-center"
						type="submit"
						disabled={!!lineError}
					>
						<Save class="h-5 w-5" />
						<span>{saveButtonLabel}</span>
					</button>
				</div>
			</form>
		</div>
	</div>
{/if}
