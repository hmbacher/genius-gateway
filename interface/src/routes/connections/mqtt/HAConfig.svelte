<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import Collapsible from '$lib/components/Collapsible.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import DirtyMarker from '$lib/components/DirtyMarker.svelte';
	import DirtyField from '$lib/components/DirtyField.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';
	import { isDiscoveryPrefix } from '$lib/utils/validators';
	import FieldError from '$lib/components/FieldError.svelte';
	import HomeAssistant from '~icons/tabler/smart-home';
	import Info from '~icons/tabler/info-circle';
	import type { HASettings } from '$lib/types/models';

	const f = createDirtyState<HASettings>({
		enabled: false,
		discovery_prefix: 'homeassistant/',
		device_name: '',
		manufacturer: '',
		model: ''
	});

	async function getHASettings() {
		try {
			const response = await fetch('/rest/haSettings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			f.reset(await response.json());
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	const discoveryPrefixError = $derived(!isDiscoveryPrefix(f.current.discovery_prefix ?? ''));
	const deviceNameError = $derived((f.current.device_name ?? '').length > 64);
	const manufacturerError = $derived((f.current.manufacturer ?? '').length > 64);
	const modelError = $derived((f.current.model ?? '').length > 64);
	const hasError = $derived(discoveryPrefixError || deviceNameError || manufacturerError || modelError);

	async function postHASettings() {
		try {
			const response = await fetch('/rest/haSettings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(f.current)
			});
			if (response.status == 200) {
				notifications.success('Home Assistant settings updated.', 3000);
				f.reset(await response.json());
			} else {
				notifications.error('Updating Home Assistant settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	function handleSubmit() {
		if (!hasError) {
			postHASettings();
		}
	}
</script>

{#await getHASettings()}
	<Spinner />
{:then _}
	<Collapsible open={false} class="shadow-lg" isDirty={f.anyDirty} onRevert={() => f.revertAll()}>
		{#snippet icon()}
			<HomeAssistant class="h-6 w-6" />
		{/snippet}
		{#snippet title()}
			<span>Home Assistant Integration</span>
		{/snippet}
		<form
			class="fieldset"
			onsubmit={(e) => {
				e.preventDefault();
				handleSubmit();
			}}
			novalidate
			transition:slide|local={{ duration: 300, easing: cubicOut }}
		>
			<div class="alert alert-info my-2 shadow-lg">
				<Info class="h-6 w-6 shrink-0 stroke-current" />
				<span
					>Publishes diagnostic, firmware-update and app entities to Home Assistant via MQTT
					discovery. Requires an active MQTT connection.</span
				>
			</div>
			<div class="grid w-full grid-cols-1 content-center gap-x-4 gap-y-2 px-4">
				<div class="flex items-center w-full mt-2">
					<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('enabled') ? 'w-1 mr-2' : 'w-0'}"></div>
					<label class="label cursor-pointer justify-start gap-2 items-center">
						<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.enabled} />
						<div class="flex items-center gap-2">
							<span>Enable Home Assistant Integration</span>
							<DirtyMarker dirty={f.isDirty('enabled')} onrevert={() => f.revert('enabled')} />
						</div>
					</label>
				</div>

				<div>
					<label class="label" for="discovery_prefix">Discovery Prefix</label>
					<DirtyField dirty={f.isDirty('discovery_prefix')} onrevert={() => f.revert('discovery_prefix')}>
						<input
							type="text"
							class="input w-full pr-10 {discoveryPrefixError ? 'border-error border-2' : ''}"
							bind:value={f.current.discovery_prefix}
							id="discovery_prefix"
							minlength="1"
							maxlength="64"
							required
						/>
					</DirtyField>
					<FieldError show={discoveryPrefixError} message="Must be 1–64 characters (a–z, A–Z, 0–9, -, _, ., /). No leading slash or double slashes." />
				</div>

				<div>
					<label class="label" for="device_name">Device Name</label>
					<DirtyField dirty={f.isDirty('device_name')} onrevert={() => f.revert('device_name')}>
						<input
							type="text"
							class="input w-full pr-10 {deviceNameError ? 'border-error border-2' : ''}"
							bind:value={f.current.device_name}
							id="device_name"
							maxlength="64"
							placeholder="(empty falls back to firmware name: {page.data.appName})"
						/>
					</DirtyField>
					<FieldError show={deviceNameError} message="Device name is limited to 64 characters." />
				</div>

				<div>
					<label class="label" for="manufacturer">Manufacturer</label>
					<DirtyField dirty={f.isDirty('manufacturer')} onrevert={() => f.revert('manufacturer')}>
						<input
							type="text"
							class="input w-full pr-10 {manufacturerError ? 'border-error border-2' : ''}"
							bind:value={f.current.manufacturer}
							id="manufacturer"
							maxlength="64"
						/>
					</DirtyField>
					<FieldError show={manufacturerError} message="Manufacturer is limited to 64 characters." />
				</div>

				<div>
					<label class="label" for="model">Model</label>
					<DirtyField dirty={f.isDirty('model')} onrevert={() => f.revert('model')}>
						<input
							type="text"
							class="input w-full pr-10 {modelError ? 'border-error border-2' : ''}"
							bind:value={f.current.model}
							id="model"
							maxlength="64"
						/>
					</DirtyField>
					<FieldError show={modelError} message="Model is limited to 64 characters." />
				</div>
			</div>
			<div class="divider mb-2 mt-0"></div>
			<div class="mx-4 flex flex-wrap justify-end gap-2">
				<button class="btn btn-primary" disabled={!f.anyDirty || hasError} type="submit"
					>Apply Settings</button
				>
			</div>
		</form>
	</Collapsible>
{/await}
