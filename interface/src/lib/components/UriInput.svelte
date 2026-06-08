<script lang="ts" module>
	export type UriProtocol = { scheme: string; defaultPort: number };
</script>

<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import DirtyMarker from './DirtyMarker.svelte';
	import DirtyField from './DirtyField.svelte';

	interface Props {
		value?: string;
		protocols: UriProtocol[];
		defaultProtocol?: string;
		fixedPort?: number;
		portMin?: number;
		portMax?: number;
		id?: string;
		hostPlaceholder?: string;
		labelProtocol?: string;
		labelHost?: string;
		labelPort?: string;
		error?: boolean;
		dirty?: boolean;
	}

	let {
		value = $bindable(''),
		protocols,
		defaultProtocol = protocols[0].scheme,
		fixedPort = undefined,
		portMin = 1,
		portMax = 65535,
		id = 'uri',
		hostPlaceholder = 'Hostname or IP',
		labelProtocol = 'Protocol',
		labelHost = 'Host',
		labelPort = 'Port',
		error = $bindable(false),
		dirty = $bindable(false)
	}: Props = $props();

	const singleProtocol = protocols.length === 1;
	const hostRegex = /^((?:[a-zA-Z0-9-]+\.)*[a-zA-Z0-9-]+|(?:\d{1,3}\.){3}\d{1,3})$/;
	const schemes = protocols.map((p) => p.scheme);
	const defaultPortFor = (s: string): number =>
		protocols.find((p) => p.scheme === s)?.defaultPort ?? portMin;
	// Escape special regex chars in scheme names defensively (e.g. ws+tls).
	const schemesEscaped = schemes.map((s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'));
	const parseRegex = new RegExp(`^(${schemesEscaped.join('|')})://([^:/?#]+)(?::(\\d+))?$`);

	function parseUri(uri: string): { scheme: string; host: string; port: number } | null {
		const m = uri.match(parseRegex);
		if (!m) return null;
		return {
			scheme: m[1],
			host: m[2],
			port: m[3] ? Number(m[3]) : defaultPortFor(m[1])
		};
	}

	const seed = parseUri(value);
	const initialScheme = seed?.scheme ?? defaultProtocol;
	const initialHost = seed?.host ?? '';
	const initialPort = fixedPort ?? seed?.port ?? defaultPortFor(initialScheme);

	let scheme = $state(initialScheme);
	let host = $state(initialHost);
	let port = $state<number>(initialPort);

	// Per-sub-field baseline = the saved/loaded value each field is compared against (for its own
	// dirty marker) and restored to on its own revert. Plus the raw string so a full revert can
	// restore an empty/invalid saved value the write-back can't emit.
	let baseScheme = $state(initialScheme);
	let baseHost = $state(initialHost);
	let basePort = $state<number>(initialPort);
	let baselineValue = $state(value);

	// Track the last externally-observed value so we only re-baseline on genuine external changes,
	// not on our own write-backs.
	let lastExternalValue = $state(value);

	$effect(() => {
		const v = value;
		if (v === lastExternalValue) return;
		lastExternalValue = v;
		baselineValue = v;
		const parsed = parseUri(v);
		if (parsed) {
			scheme = parsed.scheme;
			host = parsed.host;
			if (fixedPort === undefined) port = parsed.port;
			baseScheme = parsed.scheme;
			baseHost = parsed.host;
			if (fixedPort === undefined) basePort = parsed.port;
		} else {
			// Unparseable external value (e.g. ""): keep the displayed fields and adopt them as the
			// baseline so an untouched form is not reported dirty.
			baseScheme = scheme;
			baseHost = host;
			if (fixedPort === undefined) basePort = port;
		}
	});

	const schemeInvalid = $derived(!schemes.includes(scheme));
	const hostInvalid = $derived(!host || !hostRegex.test(host));
	const portInvalid = $derived(
		fixedPort === undefined &&
			(!Number.isInteger(Number(port)) || Number(port) < portMin || Number(port) > portMax)
	);

	const errorMessage = $derived.by(() => {
		if (schemeInvalid) return 'Invalid protocol';
		if (hostInvalid) return 'Host must be a valid hostname or IPv4 address';
		if (portInvalid) return `Port must be between ${portMin} and ${portMax}`;
		return '';
	});

	// Write-back: publish a valid assembled URI to the bound `value`. Never advances the baseline.
	$effect(() => {
		error = !!errorMessage;
		if (errorMessage) return;
		const assembled = `${scheme}://${host}:${fixedPort ?? port}`;
		if (assembled !== value) {
			lastExternalValue = assembled;
			value = assembled;
		}
	});

	// Per-sub-field dirty: each field is its own form control with its own marker + revert.
	const schemeDirty = $derived(scheme !== baseScheme);
	const hostDirty = $derived(host !== baseHost);
	const portDirty = $derived(fixedPort === undefined && Number(port) !== Number(basePort));
	const dirtyNow = $derived(schemeDirty || hostDirty || portDirty);
	$effect(() => {
		dirty = dirtyNow;
	});

	// Inset box-shadow (not a border) so the dirty accent does not shift the field's content.
	const accent = 'shadow-[inset_4px_0_0_0_var(--color-red-300)]';

	function revertScheme() {
		scheme = baseScheme;
	}
	function revertHost() {
		host = baseHost;
	}
	function revertPort() {
		if (fixedPort === undefined) port = basePort;
	}

	/**
	 * Reset every sub-field to the saved baseline. Exposed so the containing card can revert this
	 * field uniformly, like a simple field. Restores `value` explicitly because the write-back only
	 * publishes *valid* assemblies and could not otherwise restore an empty/invalid saved value.
	 */
	export function revert() {
		scheme = baseScheme;
		host = baseHost;
		if (fixedPort === undefined) port = basePort;
		lastExternalValue = baselineValue;
		value = baselineValue;
	}

	/**
	 * Adopt the current field values as the new baseline. The card calls this after a successful
	 * save so the sub-fields clear their dirty state even when the saved value equals what the
	 * component already wrote back (in which case no external `value` change would re-baseline it).
	 */
	export function commit() {
		baseScheme = scheme;
		baseHost = host;
		if (fixedPort === undefined) basePort = port;
		baselineValue = value;
	}

	function onSchemeChange(e: Event) {
		const newScheme = (e.currentTarget as HTMLSelectElement).value;
		if (fixedPort === undefined) {
			const oldDefault = defaultPortFor(scheme);
			if (Number(port) === oldDefault) port = defaultPortFor(newScheme);
		}
		scheme = newScheme;
	}
</script>

<div>
	<div class="flex flex-wrap items-start gap-x-1 gap-y-2">
		<!-- Protocol: native <select> can't host child elements, so we wrap it in a label.input
		     container to get the same visual appearance as other fields with an inner revert button. -->
		{#if !singleProtocol}
			<div class="flex grow flex-col">
				<label class="label py-0" for="{id}-scheme">{labelProtocol}</label>
				<label
					class="input w-full pl-0 {schemeInvalid
						? 'border-error border-2'
						: schemeDirty
							? accent
							: ''}"
					for="{id}-scheme"
				>
					<select
						class="h-full border-none bg-transparent ps-3 pe-2 outline-none"
						id="{id}-scheme"
						value={scheme}
						oninput={onSchemeChange}
					>
						{#each protocols as p}
							<option value={p.scheme}>{p.scheme}://</option>
						{/each}
					</select>
					<DirtyMarker dirty={schemeDirty} onrevert={revertScheme} />
				</label>
			</div>
		{/if}

		<!-- Host: the primary field. With a scheme prefix / fixed-port suffix it needs the daisyUI
		     label-as-input wrapper; otherwise it's a plain input with an overlay revert, identical
		     to a simple text field elsewhere on the page. -->
		<div class="flex min-w-[10rem] grow-[99] flex-col">
			<label class="label py-0" for={id}>{labelHost}</label>
			{#if singleProtocol || fixedPort !== undefined}
				<label
					class="input w-full min-w-0 {hostInvalid
						? 'border-error border-2'
						: hostDirty
							? accent
							: ''}"
					for={id}
				>
					{#if singleProtocol}
						<span class="text-base-content/60 select-none">{protocols[0].scheme}://</span>
					{/if}
					<input
						type="text"
						class="min-w-0 grow"
						{id}
						bind:value={host}
						placeholder={hostPlaceholder}
						required
					/>
					{#if fixedPort !== undefined}
						<span class="text-base-content/60 select-none">:{fixedPort}</span>
					{/if}
					<DirtyMarker dirty={hostDirty} onrevert={revertHost} />
				</label>
			{:else}
				<DirtyField dirty={hostDirty} onrevert={revertHost} class="w-full">
					<input
						type="text"
						class="input w-full pr-9 {hostInvalid
							? 'border-error border-2'
							: hostDirty
								? accent
								: ''}"
						{id}
						bind:value={host}
						placeholder={hostPlaceholder}
						required
					/>
				</DirtyField>
			{/if}
		</div>

		<!-- Port: same pattern as Keep Alive — input fills space, DirtyMarker inside the border. -->
		{#if fixedPort === undefined}
			<div class="flex grow flex-col">
				<label class="label py-0" for="{id}-port">{labelPort}</label>
				<label
					for="{id}-port"
					class="input w-full {portInvalid
						? 'border-error border-2'
						: portDirty
							? accent
							: ''}"
				>
					<span class="text-base-content/60 select-none">:</span>
					<input
						type="number"
						class=""
						min={portMin}
						max={portMax}
						step="1"
						bind:value={port}
						id="{id}-port"
						required
					/>
					<DirtyMarker dirty={portDirty} onrevert={revertPort} />
				</label>
			</div>
		{/if}
	</div>
	{#if errorMessage}
		<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
			<span class="text-error text-sm">{errorMessage}</span>
		</div>
	{/if}
</div>
