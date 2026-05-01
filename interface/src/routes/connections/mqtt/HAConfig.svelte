<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import Collapsible from '$lib/components/Collapsible.svelte';
	import HomeAssistant from '~icons/tabler/smart-home';
	import Info from '~icons/tabler/info-circle';
	import type { HASettings } from '$lib/types/models';

	let haSettings: HASettings = $state();

	let formField: any = $state();

	async function getHASettings() {
		try {
			const response = await fetch('/rest/haSettings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			haSettings = await response.json();
			strSettings = JSON.stringify(haSettings);
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	let strSettings: string = $state('{}');
	let isSettingsDirty: boolean = $derived(JSON.stringify(haSettings) !== strSettings);

	let formErrors = $state({
		discovery_prefix: false,
		device_name: false,
		manufacturer: false,
		model: false
	});

	let hasError = $derived(
		formErrors.discovery_prefix ||
			formErrors.device_name ||
			formErrors.manufacturer ||
			formErrors.model
	);

	async function postHASettings() {
		try {
			const response = await fetch('/rest/haSettings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(haSettings)
			});
			if (response.status == 200) {
				notifications.success('Home Assistant settings updated.', 3000);
				haSettings = await response.json();
				strSettings = JSON.stringify(haSettings);
			} else {
				notifications.error('Updating Home Assistant settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	function handleSubmit() {
		let valid = true;

		formErrors.discovery_prefix =
			haSettings.discovery_prefix.length < 1 || haSettings.discovery_prefix.length > 64;
		formErrors.device_name = haSettings.device_name.length > 64;
		formErrors.manufacturer = haSettings.manufacturer.length > 64;
		formErrors.model = haSettings.model.length > 64;

		if (
			formErrors.discovery_prefix ||
			formErrors.device_name ||
			formErrors.manufacturer ||
			formErrors.model
		)
			valid = false;

		if (valid) {
			postHASettings();
		}
	}

	function preventDefault(fn) {
		return function (event) {
			event.preventDefault();
			fn.call(this, event);
		};
	}
</script>

{#await getHASettings() then _}
	<Collapsible open={false} class="shadow-lg" isDirty={isSettingsDirty}>
		{#snippet icon()}
			<HomeAssistant class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
		{/snippet}
		{#snippet title()}
			<span>Home Assistant Integration</span>
		{/snippet}
		<form
			class="fieldset"
			onsubmit={preventDefault(handleSubmit)}
			novalidate
			bind:this={formField}
			transition:slide|local={{ duration: 300, easing: cubicOut }}
		>
			<div class="alert bg-base-300 my-2 shadow-lg">
				<Info class="h-6 w-6 shrink-0" />
				<span
					>Publishes diagnostic, firmware-update and app entities to Home Assistant via MQTT
					discovery. Requires an active MQTT connection.</span
				>
			</div>
			<div class="grid w-full grid-cols-1 content-center gap-x-4 gap-y-2 px-4">
				<label class="label inline-flex cursor-pointer content-end justify-start gap-4 text-base">
					<input
						type="checkbox"
						bind:checked={haSettings.enabled}
						class="toggle toggle-primary"
					/>
					Enable Home Assistant Integration
				</label>

				<div>
					<label class="label" for="discovery_prefix">Discovery Prefix</label>
					<input
						type="text"
						class="input input-bordered w-full invalid:border-error invalid:border-2 {formErrors.discovery_prefix
							? 'border-error border-2'
							: ''}"
						bind:value={haSettings.discovery_prefix}
						id="discovery_prefix"
						min="1"
						max="64"
						required
					/>
					{#if formErrors.discovery_prefix}
						<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
							<label class="label" for="discovery_prefix">
								<span class="label-text-alt text-error"
									>Discovery prefix is required and limited to 64 characters</span
								>
							</label>
						</div>
					{/if}
				</div>

				<div>
					<label class="label" for="device_name">Device Name</label>
					<input
						type="text"
						class="input input-bordered w-full {formErrors.device_name ? 'border-error border-2' : ''}"
						bind:value={haSettings.device_name}
						id="device_name"
						max="64"
						placeholder="(empty uses firmware name)"
					/>
					{#if formErrors.device_name}
						<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
							<label class="label" for="device_name">
								<span class="label-text-alt text-error">Device name is limited to 64 characters</span>
							</label>
						</div>
					{/if}
				</div>

				<div>
					<label class="label" for="manufacturer">Manufacturer</label>
					<input
						type="text"
						class="input input-bordered w-full {formErrors.manufacturer
							? 'border-error border-2'
							: ''}"
						bind:value={haSettings.manufacturer}
						id="manufacturer"
						max="64"
					/>
					{#if formErrors.manufacturer}
						<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
							<label class="label" for="manufacturer">
								<span class="label-text-alt text-error">Manufacturer is limited to 64 characters</span>
							</label>
						</div>
					{/if}
				</div>

				<div>
					<label class="label" for="model">Model</label>
					<input
						type="text"
						class="input input-bordered w-full {formErrors.model ? 'border-error border-2' : ''}"
						bind:value={haSettings.model}
						id="model"
						max="64"
					/>
					{#if formErrors.model}
						<div transition:slide|local={{ duration: 300, easing: cubicOut }}>
							<label class="label" for="model">
								<span class="label-text-alt text-error">Model is limited to 64 characters</span>
							</label>
						</div>
					{/if}
				</div>
			</div>
			<div class="divider mb-2 mt-0"></div>
			<div class="mx-4 flex flex-wrap justify-end gap-2">
				<button class="btn btn-primary" disabled={hasError || !isSettingsDirty} type="submit"
					>Apply Settings</button
				>
			</div>
		</form>
	</Collapsible>
{/await}
