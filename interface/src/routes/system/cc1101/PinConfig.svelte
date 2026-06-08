<script lang="ts">
	import { onMount } from 'svelte';
	import { page } from '$app/state';
	import { modals } from 'svelte-modals';
	import { user } from '$lib/stores/user';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';
	import IconSelect from '$lib/components/IconSelect.svelte';
	import IconBan from '~icons/tabler/ban';
	import PinTestDialog from './PinTestDialog.svelte';
	import IconRoute from '~icons/tabler/route-square';
	import IconTest from '~icons/tabler/plug-connected';
	import IconAlert from '~icons/tabler/alert-triangle';
	import type {
		CC1101PinProfile,
		CC1101Pins,
		CC1101Gpio,
		CC1101ProbeResult
	} from '$lib/types/models';

	const authHeader = () => ({
		Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
		'Content-Type': 'application/json'
	});

	// SPI host isn't user-selectable; ESP32-S3 exposes SPI2_HOST (1) and SPI3_HOST (2). Preset-less
	// (generic) boards seed spi_host = -1 to mark "unconfigured", so we coerce that to a valid
	// default before sending it to probe/save (which reject any host other than SPI2/SPI3).
	const DEFAULT_SPI_HOST = 1; // SPI2_HOST
	const validSpiHost = (h: number) => (h === 1 || h === 2 ? h : DEFAULT_SPI_HOST);

	type PinKey = 'csn' | 'sck' | 'mosi' | 'miso' | 'gdo0';
	const PIN_FIELDS: { key: PinKey; label: string; output: boolean }[] = [
		{ key: 'csn', label: 'CSN - Chip Select', output: true },
		{ key: 'sck', label: 'SCK - Clock', output: true },
		{ key: 'mosi', label: 'MOSI - Data to Radio', output: true },
		{ key: 'miso', label: 'MISO - Data from Radio', output: false },
		{ key: 'gdo0', label: 'GDO0 - Packet Signal', output: false }
	];

	const f = createDirtyState<CC1101Pins>({ csn: -1, miso: -1, mosi: -1, sck: -1, gdo0: -1, spi_host: 1 });

	let profile = $state<CC1101PinProfile | null>(null);
	let validPins = $state<CC1101Gpio[]>([]);
	// Selected wiring: a preset index, or 'custom'
	let mode = $state<number | 'custom'>('custom');
	let loading = $state(true);
	let loadError = $state(false);

	let isCustom = $derived(mode === 'custom');

	let usedCounts = $derived.by(() => {
		const counts = new Map<number, number>();
		for (const field of PIN_FIELDS) {
			const v = f.current[field.key];
			if (v >= 0) counts.set(v, (counts.get(v) ?? 0) + 1);
		}
		return counts;
	});
	let hasDuplicates = $derived([...usedCounts.values()].some((c) => c > 1));
	let allSet = $derived(PIN_FIELDS.every((field) => f.current[field.key] >= 0));
	let canApply = $derived(allSet && !hasDuplicates && f.anyDirty);

	function pinsEqual(a: CC1101Pins, b: CC1101Pins): boolean {
		return PIN_FIELDS.every((field) => a[field.key] === b[field.key]) && a.spi_host === b.spi_host;
	}

	function gpioFor(key: PinKey): CC1101Gpio | undefined {
		return validPins.find((g) => g.num === f.current[key]);
	}

	function isStrapping(key: PinKey): boolean {
		return gpioFor(key)?.strapping === true;
	}

	function optionDisabled(field: { output: boolean }, gpio: CC1101Gpio): boolean {
		return gpio.reserved || (field.output && !gpio.output);
	}

	function pinOptions(field: { key: PinKey; output: boolean }) {
		return validPins.map((gpio) => ({
			value: gpio.num,
			label: gpio.label,
			prefixIcon: gpio.reserved ? IconBan : gpio.strapping ? IconAlert : undefined,
			prefixClass: gpio.reserved ? 'text-error-content' : gpio.strapping ? 'text-warning-content' : undefined,
			suffix: gpio.reserved ? '- reserved' : gpio.strapping ? '- strapping' : undefined,
			itemClass: gpio.reserved
				? 'bg-error text-error-content hover:bg-error/80'
				: gpio.strapping
					? 'bg-warning text-warning-content hover:bg-warning/80'
					: undefined,
			disabled: optionDisabled(field, gpio)
		}));
	}

	function selectClass(field: { key: PinKey }): string {
		if ((usedCounts.get(f.current[field.key]) ?? 0) > 1) return 'border-error border-2';
		if (isStrapping(field.key)) return 'border-warning';
		return '';
	}

	function selectPreset(i: number) {
		if (!profile) return;
		mode = i;
		Object.assign(f.current, profile.presets[i].pins);
	}

	function selectCustom() {
		mode = 'custom';
	}

	function onWiring(value: string) {
		if (value === 'custom') selectCustom();
		else selectPreset(parseInt(value));
	}

	async function load() {
		loading = true;
		loadError = false;
		try {
			const [pRes, sRes, vRes] = await Promise.all([
				fetch('/rest/cc1101/pin-profile', { headers: authHeader() }),
				fetch('/rest/cc1101-pins', { headers: authHeader() }),
				fetch('/rest/cc1101/valid-pins', { headers: authHeader() })
			]);
			if (!pRes.ok || !sRes.ok || !vRes.ok) {
				loadError = true;
				return;
			}
			profile = await pRes.json();
			validPins = (await vRes.json()).gpios;
			const pins: CC1101Pins = await sRes.json();
			f.reset({
				csn: pins.csn,
				miso: pins.miso,
				mosi: pins.mosi,
				sck: pins.sck,
				gdo0: pins.gdo0,
				spi_host: validSpiHost(pins.spi_host)
			});

			const presetIdx = profile?.presets.findIndex((p) => pinsEqual(p.pins, f.current)) ?? -1;
			mode = presetIdx >= 0 ? presetIdx : 'custom';
		} catch (e) {
			console.error('Failed to load CC1101 pin configuration:', e);
			loadError = true;
		} finally {
			loading = false;
		}
	}

	function pinsBody() {
		return JSON.stringify({
			csn: f.current.csn,
			miso: f.current.miso,
			mosi: f.current.mosi,
			sck: f.current.sck,
			gdo0: f.current.gdo0,
			spi_host: f.current.spi_host
		});
	}

	async function probe(): Promise<CC1101ProbeResult> {
		const res = await fetch('/rest/cc1101/probe', {
			method: 'POST',
			headers: authHeader(),
			body: pinsBody()
		});
		const json = await res.json();
		if (!res.ok) throw new Error(json.reason ?? 'Self-test failed.');
		return json;
	}

	async function save(): Promise<void> {
		const res = await fetch('/rest/cc1101-pins', {
			method: 'POST',
			headers: authHeader(),
			body: pinsBody()
		});
		if (!res.ok) throw new Error('Failed to save pin configuration.');
		f.commit();
	}

	function openTest() {
		modals.open(PinTestDialog, { probe, save });
	}

	onMount(load);
</script>

<SettingsCard collapsible={false} isDirty={f.anyDirty} onRevert={() => f.revertAll()}>
	{#snippet icon()}
		<IconRoute class="h-6 w-6" />
	{/snippet}
	{#snippet title()}
		<span>SPI Configuration</span>
	{/snippet}

	{#if loading}
		<Spinner />
	{:else if loadError || !profile}
		<div class="flex items-center text-error">
			<IconAlert class="mr-2 h-5 w-5" />
			<span>Could not load the pin configuration.</span>
		</div>
	{:else}
		<div class="flex w-full flex-col space-y-4">
			{#if profile.presets.length > 0}
				<div class="form-control">
					<label class="label py-1" for="wiring-select">
						<span class="label-text">Wiring</span>
					</label>
					<select
						id="wiring-select"
						class="select select-bordered w-full sm:w-64"
						value={isCustom ? 'custom' : String(mode)}
						onchange={(e) => onWiring(e.currentTarget.value)}
					>
						{#each profile.presets as preset, i}
							<option value={String(i)}>{preset.name}</option>
						{/each}
						<option value="custom">Custom</option>
					</select>
				</div>
			{/if}

			<!-- Unified pin list: static values for a preset, dropdowns in the right column for Custom -->
			<div class="rounded-box bg-base-100 divide-y divide-base-300">
				{#each PIN_FIELDS as field}
					<div class="flex items-center justify-between gap-3 px-4 py-2">
						<span class="flex-1">{field.label}</span>
						{#if isCustom}
							<div class="flex items-center gap-2">
								{#if isStrapping(field.key)}
									<span class="tooltip tooltip-left" data-tip="Strapping pin - use with care">
										<IconAlert class="text-warning h-5 w-5" />
									</span>
								{/if}
								<IconSelect
									bind:value={f.current[field.key]}
									options={pinOptions(field)}
									placeholder="— select GPIO —"
									class="w-30 {selectClass(field)}"
									dirty={f.isDirty(field.key)}
									onrevert={() => f.revert(field.key)}
								/>
							</div>
						{:else}
							<span class="font-mono">GPIO {f.current[field.key]}</span>
						{/if}
					</div>
				{/each}
			</div>

			{#if hasDuplicates}
				<div class="flex items-center text-error text-xs">
					<IconAlert class="mr-2 h-4 w-4 shrink-0" />
					<span>Each pin must be assigned to a single function.</span>
				</div>
			{/if}

			<div class="flex flex-wrap justify-end gap-2">
				<button class="btn btn-primary" onclick={openTest} disabled={!canApply}>
					<IconTest class="h-5 w-5" />
					Test &amp; Save
				</button>
			</div>
		</div>
	{/if}
</SettingsCard>
