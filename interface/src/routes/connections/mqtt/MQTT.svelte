<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import InputPassword from '$lib/components/InputPassword.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import UriInput, { type UriProtocol } from '$lib/components/UriInput.svelte';
	import DirtyField from '$lib/components/DirtyField.svelte';
	import DirtyMarker from '$lib/components/DirtyMarker.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';
	import FieldError from '$lib/components/FieldError.svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import Spinner from '$lib/components/Spinner.svelte';
	import Collapsible from '$lib/components/Collapsible.svelte';
	import IconMQTT from '~icons/tabler/topology-star-3';
	import IconSettings from '~icons/tabler/adjustments-alt';
	import Client from '~icons/tabler/robot';
	import type { MQTTSettings, MQTTStatus } from '$lib/types/models';
	import AlarmPublishingConfig from './AlarmPublishingConfig.svelte';
	import HAConfig from './HAConfig.svelte';

	const mqttProtocols: UriProtocol[] = [
		{ scheme: 'mqtt', defaultPort: 1883 },
		{ scheme: 'mqtts', defaultPort: 8883 },
		{ scheme: 'ws', defaultPort: 80 },
		{ scheme: 'wss', defaultPort: 443 }
	];

	const defaultMQTTSettings: MQTTSettings = {
		enabled: false,
		uri: '',
		username: '',
		password: '',
		client_id: '',
		keep_alive: 60,
		clean_session: true,
		message_interval_ms: 0
	};

	// Per-field dirty tracking: f.current is the bindable form object, f.isDirty(key) drives the
	// per-field marker, f.anyDirty rolls up to the card. uriDirty additionally catches the case
	// where UriInput has an invalid intermediate edit that hasn't been written back to f.current.uri.
	const f = createDirtyState<MQTTSettings>({ ...defaultMQTTSettings });
	let uriDirty = $state(false);
	let uriInput = $state<{ revert: () => void; commit: () => void }>();
	let isSettingsDirty: boolean = $derived(f.anyDirty || uriDirty);
	let settingsLoaded = $state(false);

	let mqttStatus: MQTTStatus = $state({
		enabled: false,
		connected: false,
		client_id: '',
		last_error: ''
	});

	async function getMQTTStatus() {
		try {
			const response = await fetch('/rest/mqttStatus', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			if (response.ok) {
				mqttStatus = await response.json();
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return mqttStatus;
	}

	async function getMQTTSettings() {
		try {
			const response = await fetch('/rest/mqttSettings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			if (response.ok) {
				f.reset(await response.json());
				settingsLoaded = true;
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return f.current;
	}

	const interval = setInterval(async () => {
		getMQTTStatus();
	}, 5000);

	onDestroy(() => clearInterval(interval));

	onMount(() => {
		if (!page.data.features.security || $user.admin) {
			getMQTTSettings();
		}
	});

	let uriError = $state(false);
	let keepAliveError = $derived(
		!Number.isFinite(Number(f.current.keep_alive)) ||
			Number(f.current.keep_alive) < 1 ||
			Number(f.current.keep_alive) > 600
	);
	let rateLimitError = $derived(
		!Number.isFinite(Number(f.current.message_interval_ms)) ||
			Number(f.current.message_interval_ms) < 0 ||
			Number(f.current.message_interval_ms) > 1000
	);
	let hasErrors = $derived(uriError || keepAliveError || rateLimitError);

	async function postMQTTSettings() {
		try {
			const response = await fetch('/rest/mqttSettings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(f.current)
			});
			if (response.status == 200) {
				notifications.success('MQTT settings updated.', 3000);
				f.reset(await response.json());
				uriInput?.commit(); // self-contained field: re-baseline even if its value was unchanged
			} else {
				notifications.error('Updating MQTT settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	function handleSubmitMQTT() {
		if (!hasErrors) {
			postMQTTSettings();
		}
	}
</script>

<SettingsCard collapsible={false}>
	{#snippet icon()}
		<IconMQTT class="h-6 w-6" />
	{/snippet}
	{#snippet title()}
		<span>MQTT</span>
	{/snippet}
	<div class="w-full overflow-x-auto">
		{#await getMQTTStatus()}
			<Spinner />
		{:then nothing}
			<div
				class="flex w-full flex-col space-y-1"
				transition:slide|local={{ duration: 300, easing: cubicOut }}
			>
				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div
						class="mask mask-hexagon h-auto w-10 shrink-0 {mqttStatus.connected === true
							? 'bg-success'
							: 'bg-error'}"
					>
						<IconMQTT
							class="h-auto w-full scale-75 {mqttStatus.connected === true
								? 'text-success-content'
								: 'text-error-content'}"
						/>
					</div>
					<div>
						<div class="font-bold">Status</div>
						<div class="text-sm opacity-75">
							{#if mqttStatus.connected}
								Connected
							{:else if !mqttStatus.enabled}
								MQTT Disabled
							{:else}
								{mqttStatus.last_error}
							{/if}
						</div>
					</div>
				</div>

				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
						<Client class="text-primary-content h-auto w-full scale-75" />
					</div>
					<div>
						<div class="font-bold">Client ID</div>
						<div class="text-sm opacity-75">
							{mqttStatus.client_id}
						</div>
					</div>
				</div>
			</div>
		{/await}
	</div>

	{#if !page.data.features.security || $user.admin}
		<Collapsible
			open={false}
			class="shadow-lg"
			closed={getMQTTSettings}
			isDirty={isSettingsDirty}
			onRevert={() => {
				f.revertAll();
				uriInput?.revert();
			}}
		>
			{#snippet icon()}
				<IconSettings class="h-6 w-6" />
			{/snippet}
			{#snippet title()}
				<span>General Settings</span>
			{/snippet}

			{#if !settingsLoaded}
				<Spinner />
			{:else}
			<form
				onsubmit={(e) => {
					e.preventDefault();
					handleSubmitMQTT();
				}}
				novalidate
				class="fieldset"
			>
				<div class="grid w-full grid-cols-1 content-center gap-x-4 gap-y-2 px-4 sm:grid-cols-2">
					<!-- Enable -->
					<div class="flex items-center w-full">
						<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('enabled') ? 'w-1 mr-2' : 'w-0'}"></div>
						<label class="label cursor-pointer justify-start gap-2 items-center">
							<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.enabled} />
							<div class="flex items-center gap-2">
								<span>Enable MQTT</span>
								<DirtyMarker dirty={f.isDirty('enabled')} onrevert={() => f.revert('enabled')} />
							</div>
						</label>
					</div>
					<div class="hidden sm:block"></div>
					<!-- URI (self-contained: own dirty line + in-field revert, like a simple field) -->
					<div class="sm:col-span-2">
						<UriInput
							bind:this={uriInput}
							bind:value={f.current.uri}
							bind:error={uriError}
							bind:dirty={uriDirty}
							protocols={mqttProtocols}
							id="mqtt-uri"
						/>
					</div>
					<!-- Username -->
					<div>
						<label class="label" for="user">Username</label>
						<DirtyField dirty={f.isDirty('username')} onrevert={() => f.revert('username')}>
							<input
								type="text"
								class="input w-full pr-10"
								bind:value={f.current.username}
								id="user"
							/>
						</DirtyField>
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
					<!-- Client ID -->
					<div>
						<label class="label" for="clientid">Client ID</label>
						<DirtyField dirty={f.isDirty('client_id')} onrevert={() => f.revert('client_id')}>
							<input
								type="text"
								class="input w-full pr-10"
								bind:value={f.current.client_id}
								id="clientid"
							/>
						</DirtyField>
					</div>
					<!-- Keep Alive -->
					<div>
						<label class="label" for="keepalive">Keep Alive</label>
						<div class="relative">
						{#if f.isDirty('keep_alive')}
							<div class="pointer-events-none absolute inset-y-0 left-0 z-10 w-1 rounded-l-[var(--radius-field)] bg-red-300"></div>
						{/if}
						<label
							for="keepalive"
							class="input w-full {keepAliveError ? 'border-error border-2' : ''}"
						>
							<input
								type="number"
								min="1"
								max="600"
								class=""
								bind:value={f.current.keep_alive}
								id="keepalive"
								required
							/>
							<span class="label">Seconds</span>
							<DirtyMarker dirty={f.isDirty('keep_alive')} onrevert={() => f.revert('keep_alive')} />
						</label>
					</div>
						<FieldError show={keepAliveError} message="Must be between 1 and 600 seconds." />
					</div>
					<!-- Rate Limit -->
					<div>
						<label class="label" for="ratelimit">Publish Message Interval</label>
						<div class="relative">
						{#if f.isDirty('message_interval_ms')}
							<div class="pointer-events-none absolute inset-y-0 left-0 z-10 w-1 rounded-l-[var(--radius-field)] bg-red-300"></div>
						{/if}
						<label
							for="ratelimit"
							class="input w-full {rateLimitError ? 'border-error border-2' : ''}"
						>
							<input
								type="number"
								min="0"
								max="1000"
								class=""
								bind:value={f.current.message_interval_ms}
								id="ratelimit"
								required
							/>
							<span class="label">Milliseconds</span>
							<DirtyMarker
								dirty={f.isDirty('message_interval_ms')}
								onrevert={() => f.revert('message_interval_ms')}
							/>
						</label>
					</div>
						<FieldError show={rateLimitError} message="Must be between 0 and 1000 milliseconds." />
					</div>
					<!-- Clean Session -->
					<div class="flex items-center w-full mt-2 sm:mt-4">
						<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('clean_session') ? 'w-1 mr-2' : 'w-0'}"></div>
						<label class="label cursor-pointer justify-start gap-2 items-center">
							<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.clean_session} />
							<div class="flex items-center gap-2">
								<span>Clean Session?</span>
								<DirtyMarker dirty={f.isDirty('clean_session')} onrevert={() => f.revert('clean_session')} />
							</div>
						</label>
					</div>
				</div>
				<div class="divider mb-2 mt-0"></div>
				<div class="mx-4 flex flex-wrap justify-end gap-2">
					<button class="btn btn-primary" type="submit" disabled={hasErrors || !isSettingsDirty}
						>Apply Settings</button
					>
				</div>
			</form>
			{/if}
		</Collapsible>

		<HAConfig />
		<AlarmPublishingConfig />
	{/if}
</SettingsCard>
