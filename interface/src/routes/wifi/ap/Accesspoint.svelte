<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import InputPassword from '$lib/components/InputPassword.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import DirtyField from '$lib/components/DirtyField.svelte';
	import DirtyMarker from '$lib/components/DirtyMarker.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';
	import { isIPv4, inRange, hasLength } from '$lib/utils/validators';
	import FieldError from '$lib/components/FieldError.svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import Spinner from '$lib/components/Spinner.svelte';
	import AP from '~icons/tabler/access-point';
	import MAC from '~icons/tabler/dna-2';
	import Home from '~icons/tabler/home';
	import Devices from '~icons/tabler/devices';
	import type { ApSettings, ApStatus } from '$lib/types/models';

	const f = createDirtyState<ApSettings>({
		provision_mode: 0,
		ssid: '',
		password: '',
		channel: 1,
		ssid_hidden: false,
		max_clients: 4,
		local_ip: '',
		gateway_ip: '',
		subnet_mask: ''
	});
	let apStatus: ApStatus = $state({
		status: 0,
		ip_address: '',
		mac_address: '',
		station_num: 0
	});

	async function getAPStatus() {
		try {
			const response = await fetch('/rest/apStatus', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			apStatus = await response.json();
		} catch (error) {
			console.error('Error:', error);
		}
		return apStatus;
	}

	async function getAPSettings() {
		try {
			const response = await fetch('/rest/apSettings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			if (response.ok) f.reset(await response.json());
		} catch (error) {
			console.error('Error:', error);
		}
	}

	const interval = setInterval(async () => {
		getAPStatus();
	}, 5000);

	onDestroy(() => clearInterval(interval));

	onMount(() => {
		if (!page.data.features.security || $user.admin) {
			getAPSettings();
		}
	});

	let provisionMode = [
		{ id: 0, text: `Always` },
		{ id: 1, text: `When WiFi Disconnected` },
		{ id: 2, text: `Never` }
	];

	let apStatusDescription = [
		{ bg_color: 'bg-success', text_color: 'text-success-content', description: 'Active' },
		{ bg_color: 'bg-error', text_color: 'text-error-content', description: 'Inactive' },
		{ bg_color: 'bg-warning', text_color: 'text-warning-content', description: 'Lingering' }
	];

	const ssidError = $derived(!hasLength(f.current.ssid, 3, 32));
	const channelError = $derived(!inRange(Number(f.current.channel), 1, 13));
	const maxClientsError = $derived(!inRange(Number(f.current.max_clients), 1, 8));
	const localIPError = $derived(!isIPv4(f.current.local_ip));
	const gatewayIPError = $derived(!isIPv4(f.current.gateway_ip));
	const subnetMaskError = $derived(!isIPv4(f.current.subnet_mask));
	const hasErrors = $derived(
		ssidError || channelError || maxClientsError || localIPError || gatewayIPError || subnetMaskError
	);

	async function postAPSettings() {
		try {
			const response = await fetch('/rest/apSettings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(f.current)
			});
			if (response.status == 200) {
				notifications.success('Access Point settings updated.', 3000);
				f.reset(await response.json());
			} else {
				notifications.error('Updating Access Point settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
	}

	function handleSubmitAP() {
		if (!hasErrors) {
			postAPSettings();
		}
	}
</script>

<SettingsCard collapsible={false} isDirty={f.anyDirty} onRevert={() => f.revertAll()}>
	{#snippet icon()}
		<AP class="h-6 w-6" />
	{/snippet}
	{#snippet title()}
		<span>Access Point</span>
	{/snippet}
	<div class="w-full">
		{#await getAPStatus()}
			<Spinner />
		{:then nothing}
			<div
				class="flex w-full flex-col space-y-1"
				transition:slide|local={{ duration: 300, easing: cubicOut }}
			>
				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div
						class="mask mask-hexagon h-auto w-10 shrink-0 {apStatusDescription[apStatus.status]
							.bg_color}"
					>
						<AP class="h-auto w-full scale-75 {apStatusDescription[apStatus.status].text_color}" />
					</div>
					<div>
						<div class="font-bold">Status</div>
						<div class="text-sm opacity-75">
							{apStatusDescription[apStatus.status].description}
						</div>
					</div>
				</div>

				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
						<Home class="text-primary-content h-auto w-full scale-75" />
					</div>
					<div>
						<div class="font-bold">IP Address</div>
						<div class="text-sm opacity-75">
							{apStatus.ip_address}
						</div>
					</div>
				</div>

				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
						<MAC class="text-primary-content h-auto w-full scale-75" />
					</div>
					<div>
						<div class="font-bold">MAC Address</div>
						<div class="text-sm opacity-75">
							{apStatus.mac_address}
						</div>
					</div>
				</div>

				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
						<Devices class="text-primary-content h-auto w-full scale-75" />
					</div>
					<div>
						<div class="font-bold">AP Clients</div>
						<div class="text-sm opacity-75">
							{apStatus.station_num}
						</div>
					</div>
				</div>
			</div>
		{/await}
	</div>

	{#if !page.data.features.security || $user.admin}
		<div class="bg-base-200 shadow-lg relative grid w-full max-w-2xl self-center overflow-hidden">
			<div
				class="min-h-16 flex w-full items-center justify-between space-x-3 p-4 text-xl font-medium"
			>
				Change AP Settings
			</div>
			{#await getAPSettings()}
				<Spinner />
			{:then nothing}
				<div
					class="flex flex-col gap-2 p-0"
					transition:slide|local={{ duration: 300, easing: cubicOut }}
				>
					<form
						class="fieldset grid w-full grid-cols-1 content-center gap-x-4 gap-y-2 p-4 mb-4 sm:grid-cols-2"
						onsubmit={(e) => {
							e.preventDefault();
							handleSubmitAP();
						}}
						novalidate
					>
						<!-- Provision mode -->
						<div>
							<label class="label" for="apmode">Provide Access Point ...</label>
							<label
								class="input w-full pl-0 {f.isDirty('provision_mode') ? 'shadow-[inset_4px_0_0_0_var(--color-red-300)]' : ''}"
								for="apmode"
							>
								<select
									class="h-full flex-1 border-none bg-transparent ps-3 pe-2 outline-none"
									id="apmode"
									bind:value={f.current.provision_mode}
								>
									{#each provisionMode as mode}
										<option value={mode.id}>{mode.text}</option>
									{/each}
								</select>
								<DirtyMarker dirty={f.isDirty('provision_mode')} onrevert={() => f.revert('provision_mode')} />
							</label>
						</div>

						<!-- SSID -->
						<div>
							<label class="label" for="ssid">SSID</label>
							<DirtyField dirty={f.isDirty('ssid')} onrevert={() => f.revert('ssid')}>
								<input
									type="text"
									class="input w-full pr-10 {ssidError ? 'border-error border-2' : ''}"
									bind:value={f.current.ssid}
									id="ssid"
									min="2"
									max="32"
									required
								/>
							</DirtyField>
							<FieldError show={ssidError} message="SSID must be between 3 and 32 characters long." />
						</div>

						<!-- Password -->
						<div>
							<label class="label" for="pwd">Password</label>
							<InputPassword
								bind:value={f.current.password}
								id="pwd"
								baseline={f.baseline.password}
								onrevert={() => f.revert('password')}
							/>
						</div>

						<!-- Channel -->
						<div>
							<label class="label" for="channel">Preferred Channel</label>
							<label
								for="channel"
								class="input w-full {channelError ? 'border-error border-2' : f.isDirty('channel') ? 'shadow-[inset_4px_0_0_0_var(--color-red-300)]' : ''}"
							>
								<input
									type="number"
									min="1"
									max="13"
									class=""
									bind:value={f.current.channel}
									id="channel"
									required
								/>
								<DirtyMarker dirty={f.isDirty('channel')} onrevert={() => f.revert('channel')} />
							</label>
							<FieldError show={channelError} message="Must be channel 1 to 13." />
						</div>

						<!-- Max clients -->
						<div>
							<label class="label" for="clients">Max Clients</label>
							<label
								for="clients"
								class="input w-full {maxClientsError ? 'border-error border-2' : f.isDirty('max_clients') ? 'shadow-[inset_4px_0_0_0_var(--color-red-300)]' : ''}"
							>
								<input
									type="number"
									min="1"
									max="8"
									class=""
									bind:value={f.current.max_clients}
									id="clients"
									required
								/>
								<DirtyMarker dirty={f.isDirty('max_clients')} onrevert={() => f.revert('max_clients')} />
							</label>
							<FieldError show={maxClientsError} message="Maximum 8 clients allowed." />
						</div>

						<!-- Local IP -->
						<div>
							<label class="label" for="localIP">Local IP</label>
							<DirtyField dirty={f.isDirty('local_ip')} onrevert={() => f.revert('local_ip')}>
								<input
									type="text"
									class="input w-full pr-10 {localIPError ? 'border-error border-2' : ''}"
									minlength="7"
									maxlength="15"
									size="15"
									bind:value={f.current.local_ip}
									id="localIP"
									required
								/>
							</DirtyField>
							<FieldError show={localIPError} message="Must be a valid IPv4 address." />
						</div>

						<!-- Gateway IP -->
						<div>
							<label class="label" for="gateway">Gateway IP</label>
							<DirtyField
								dirty={f.isDirty('gateway_ip')}
								onrevert={() => f.revert('gateway_ip')}
							>
								<input
									type="text"
									class="input w-full pr-10 {gatewayIPError ? 'border-error border-2' : ''}"
									minlength="7"
									maxlength="15"
									size="15"
									bind:value={f.current.gateway_ip}
									id="gateway"
									required
								/>
							</DirtyField>
							<FieldError show={gatewayIPError} message="Must be a valid IPv4 address." />
						</div>

						<!-- Subnet mask -->
						<div>
							<label class="label" for="subnet">Subnet Mask</label>
							<DirtyField
								dirty={f.isDirty('subnet_mask')}
								onrevert={() => f.revert('subnet_mask')}
							>
								<input
									type="text"
									class="input w-full pr-10 {subnetMaskError ? 'border-error border-2' : ''}"
									minlength="7"
									maxlength="15"
									size="15"
									bind:value={f.current.subnet_mask}
									id="subnet"
									required
								/>
							</DirtyField>
							<FieldError show={subnetMaskError} message="Must be a valid IPv4 address." />
						</div>

						<!-- Hide SSID toggle -->
						<div class="flex items-center w-full">
							<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('ssid_hidden') ? 'w-1 mr-2' : 'w-0'}"></div>
							<label class="label cursor-pointer justify-start gap-2 items-center">
								<input
									type="checkbox"
									bind:checked={f.current.ssid_hidden}
									class="toggle toggle-primary shrink-0"
								/>
								<div class="flex items-center gap-2">
									<span>Hide SSID</span>
									<DirtyMarker
										dirty={f.isDirty('ssid_hidden')}
										onrevert={() => f.revert('ssid_hidden')}
									/>
								</div>
							</label>
						</div>

						<div class="place-self-end">
							<button class="btn btn-primary" type="submit" disabled={!f.anyDirty || hasErrors}>
								Apply Settings
							</button>
						</div>
					</form>
				</div>
			{/await}
		</div>
	{/if}
</SettingsCard>
