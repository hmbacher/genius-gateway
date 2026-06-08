<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Collapsible from '$lib/components/Collapsible.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import DirtyField from '$lib/components/DirtyField.svelte';
	import DirtyMarker from '$lib/components/DirtyMarker.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import { TIME_ZONES } from './timezones';
	import NTP from '~icons/tabler/clock-check';
	import Server from '~icons/tabler/server';
	import Clock from '~icons/tabler/clock';
	import UTC from '~icons/tabler/clock-pin';
	import Stopwatch from '~icons/tabler/24-hours';
	import type { NTPSettings, NTPStatus } from '$lib/types/models';

	const f = createDirtyState<NTPSettings>({
		enabled: false,
		server: '',
		tz_label: '',
		tz_format: ''
	});
	let settingsLoaded = $state(false);
	let ntpStatus: NTPStatus = $state({
		status: 0,
		utc_time: '',
		local_time: '',
		server: '',
		uptime: 0
	});

	async function getNTPStatus() {
		try {
			const response = await fetch('/rest/ntpStatus', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			ntpStatus = await response.json();
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function getNTPSettings() {
		try {
			const response = await fetch('/rest/ntpSettings', {
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
		return;
	}

	const interval = setInterval(async () => {
		getNTPStatus();
	}, 5000);

	onDestroy(() => clearInterval(interval));

	onMount(() => {
		if (!page.data.features.security || $user.admin) {
			getNTPSettings();
		}
	});

	let formErrors = $state({
		server: false
	});

	async function postNTPSettings() {
		try {
			const response = await fetch('/rest/ntpSettings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(f.current)
			});

			if (response.status == 200) {
				notifications.success('NTP settings updated.', 3000);
				f.reset(await response.json());
			} else {
				notifications.error('Updating NTP settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
	}

	function handleSubmitNTP() {
		let valid = true;

		const regexExpIPv4 =
			/\b(?:(?:2(?:[0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9])\.){3}(?:(?:2([0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9]))\b/;
		const regexExpURL =
			/[-a-zA-Z0-9@:%_\+.~#?&//=]{2,256}\.[a-z]{2,4}\b(\/[-a-zA-Z0-9@:%_\+.~#?&//=]*)?/i;

		if (!regexExpURL.test(f.current.server) && !regexExpIPv4.test(f.current.server)) {
			valid = false;
			formErrors.server = true;
		} else {
			formErrors.server = false;
		}

		f.current.tz_format = TIME_ZONES[f.current.tz_label];

		if (valid) {
			postNTPSettings();
		}
	}

	function convertSeconds(seconds: number) {
		let minutes = Math.floor(seconds / 60);
		let hours = Math.floor(minutes / 60);
		let days = Math.floor(hours / 24);

		hours = hours % 24;
		minutes = minutes % 60;
		seconds = seconds % 60;

		let result = '';
		if (days > 0) {
			result += days + ' day' + (days > 1 ? 's' : '') + ' ';
		}
		if (hours > 0) {
			result += hours + ' hour' + (hours > 1 ? 's' : '') + ' ';
		}
		if (minutes > 0) {
			result += minutes + ' minute' + (minutes > 1 ? 's' : '') + ' ';
		}
		result += seconds + ' second' + (seconds > 1 ? 's' : '');

		return result;
	}
</script>

<SettingsCard collapsible={false}>
	{#snippet icon()}
		<Clock class="h-6 w-6" />
	{/snippet}
	{#snippet title()}
		<span>Network Time</span>
	{/snippet}
	<div class="w-full">
		{#await getNTPStatus()}
			<Spinner />
		{:then nothing}
			<div
				class="flex w-full flex-col space-y-1"
				transition:slide|local={{ duration: 300, easing: cubicOut }}
			>
				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div
						class="mask mask-hexagon h-auto w-10 shrink-0 {ntpStatus.status === 1
							? 'bg-success'
							: 'bg-error'}"
					>
						<NTP
							class="h-auto w-full scale-75 {ntpStatus.status === 1
								? 'text-success-content'
								: 'text-error-content'}"
						/>
					</div>
					<div>
						<div class="font-bold">Status</div>
						<div class="text-sm opacity-75">
							{ntpStatus.status === 1 ? 'Active' : 'Inactive'}
						</div>
					</div>
				</div>

				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
						<Server class="text-primary-content h-auto w-full scale-75" />
					</div>
					<div>
						<div class="font-bold">NTP Server</div>
						<div class="text-sm opacity-75">
							{ntpStatus.server}
						</div>
					</div>
				</div>

				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
						<Clock class="text-primary-content h-auto w-full scale-75" />
					</div>
					<div>
						<div class="font-bold">Local Time</div>
						<div class="text-sm opacity-75">
							{new Intl.DateTimeFormat('en-GB', {
								dateStyle: 'long',
								timeStyle: 'long'
							}).format(new Date(ntpStatus.local_time))}
						</div>
					</div>
				</div>

				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
						<UTC class="text-primary-content h-auto w-full scale-75" />
					</div>
					<div>
						<div class="font-bold">UTC Time</div>
						<div class="text-sm opacity-75">
							{new Intl.DateTimeFormat('en-GB', {
								dateStyle: 'long',
								timeStyle: 'long',
								timeZone: 'UTC'
							}).format(new Date(ntpStatus.utc_time))}
						</div>
					</div>
				</div>

				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
						<Stopwatch class="text-primary-content h-auto w-full scale-75" />
					</div>
					<div>
						<div class="font-bold">Uptime</div>
						<div class="text-sm opacity-75">
							{convertSeconds(ntpStatus.uptime)}
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
			icon={null}
			opened={() => {}}
			closed={() => {}}
			isDirty={f.anyDirty}
			onRevert={() => f.revertAll()}
		>
			{#snippet title()}
				<span>Change NTP Settings</span>
			{/snippet}
			{#if !settingsLoaded}
				<Spinner />
			{:else}
				<form
					class="fieldset"
					onsubmit={(e) => {
						e.preventDefault();
						handleSubmitNTP();
					}}
					novalidate
				>
					<!-- Enable NTP -->
					<div class="flex items-center w-full">
						<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('enabled') ? 'w-1 mr-2' : 'w-0'}"></div>
						<label class="label cursor-pointer justify-start gap-2 items-center">
							<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.enabled} />
							<div class="flex items-center gap-2">
								<span>Enable NTP</span>
								<DirtyMarker dirty={f.isDirty('enabled')} onrevert={() => f.revert('enabled')} />
							</div>
						</label>
					</div>

					<!-- Server -->
					<label class="label" for="server">Server</label>
					<DirtyField dirty={f.isDirty('server')} onrevert={() => f.revert('server')}>
						<input
							type="text"
							min="3"
							max="64"
							class="input w-full pr-10 {formErrors.server ? 'border-error border-2' : ''}"
							bind:value={f.current.server}
							id="server"
							required
						/>
					</DirtyField>
					{#if formErrors.server}
						<p class="text-error text-xs">Please enter a valid NTP server.</p>
					{/if}

					<!-- Timezone -->
					<label class="label" for="tz">Pick Time Zone</label>
					<div class="relative flex items-center gap-2">
						{#if f.isDirty('tz_label')}
							<div class="pointer-events-none absolute inset-y-0 left-0 z-10 w-1 rounded-l-[var(--radius-field)] bg-red-300"></div>
						{/if}
						<select class="select flex-1" bind:value={f.current.tz_label} id="tz">
							{#each Object.entries(TIME_ZONES) as [tz_label, tz_format]}
								<option value={tz_label}>{tz_label}</option>
							{/each}
						</select>
						<DirtyMarker dirty={f.isDirty('tz_label')} onrevert={() => f.revert('tz_label')} />
					</div>

					<div class="mt-4 place-self-end">
						<button class="btn btn-primary" type="submit" disabled={!f.anyDirty}>Apply Settings</button>
					</div>
				</form>
			{/if}
		</Collapsible>
	{/if}
</SettingsCard>
