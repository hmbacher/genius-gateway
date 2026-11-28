<script lang="ts">
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { WSLoggerClient, WSLoggerClients, WSLoggerSettings } from '$lib/types/models';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Collapsible from '$lib/components/Collapsible.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import DirtyMarker from '$lib/components/DirtyMarker.svelte';
	import ConnectedClients from './ConnectedClients.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';
	import IconLogger from '~icons/tabler/brackets-angle';
	import IconLink from '~icons/tabler/link';
	import IconSettings from '~icons/tabler/adjustments';
	import IconSave from '~icons/tabler/device-floppy';
	import IconWarning from '~icons/tabler/alert-triangle';

	/** How often the status (and the connected clients) is fetched while the page is open. */
	const POLL_MS = 2000;

	const defaultSettings: WSLoggerSettings = {
		wsLoggerEnabled: false
	};

	const f = createDirtyState<WSLoggerSettings>({ ...defaultSettings });
	let settingsLoaded = $state(false);

	/** Saved state and connected clients as the gateway reports them; null until the first answer. */
	let loggerEnabled = $state<boolean | null>(null);
	let clients = $state<WSLoggerClient[]>([]);
	let fetchedAt = $state(0);
	let now = $state(Date.now());

	const loggerUrl = `ws://${window.location.host}/ws/logger`;

	function headers(): HeadersInit {
		return {
			Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
			'Content-Type': 'application/json'
		};
	}

	async function getStatus() {
		try {
			const response = await fetch('/rest/wslogger/clients', { headers: headers() });
			if (response.ok) {
				const data = (await response.json()) as WSLoggerClients;
				loggerEnabled = data.enabled;
				clients = data.clients ?? [];
				fetchedAt = Date.now();
			}
		} catch (error) {
			console.error('Error fetching WebSocket logger status:', error);
		}
	}

	async function getWSLoggerSettings() {
		try {
			const response = await fetch('/rest/wslogger', { method: 'GET', headers: headers() });
			f.reset(await response.json());
			settingsLoaded = true;
		} catch (error) {
			console.error('Error:', error);
		}
	}

	async function postWSLoggerSettings() {
		try {
			const response = await fetch('/rest/wslogger', {
				method: 'POST',
				headers: headers(),
				body: JSON.stringify(f.current)
			});

			if (response.status == 200) {
				notifications.success('WebSocket Logger settings updated.', 3000);
				f.reset(await response.json());
				getStatus();
			} else {
				notifications.error('Error on updating WebSocket Logger settings.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
	}

	onMount(() => {
		getWSLoggerSettings();
		getStatus();
		const poll = setInterval(getStatus, POLL_MS);
		const tick = setInterval(() => (now = Date.now()), 1000);
		return () => {
			clearInterval(poll);
			clearInterval(tick);
		};
	});
</script>

{#if $user.admin}
	<div
		class="mx-0 my-1 flex flex-col space-y-4
     sm:mx-8 sm:my-8"
	>
		<SettingsCard collapsible={false}>
			{#snippet icon()}
				<IconLogger class="h-6 w-6" />
			{/snippet}
			{#snippet title()}
				<span>WebSocket Logger</span>
			{/snippet}
			{#if loggerEnabled === null}
				<Spinner />
			{:else}
				<div
					class="flex w-full flex-col space-y-1"
					transition:slide|local={{ duration: 300, easing: cubicOut }}
				>
					<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
						<div
							class="mask mask-hexagon h-auto w-10 shrink-0 {loggerEnabled
								? 'bg-success'
								: 'bg-base-300'}"
						>
							<IconLogger
								class="h-auto w-full scale-75 {loggerEnabled
									? 'text-success-content'
									: 'text-base-content/60'}"
							/>
						</div>
						<div>
							<div class="font-bold">Status</div>
							<div class="text-sm opacity-75" data-testid="wslogger-status">
								{loggerEnabled ? 'Active' : 'Disabled - connections are refused'}
							</div>
						</div>
					</div>

					{#if loggerEnabled}
						<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
							<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
								<IconLink class="text-primary-content h-auto w-full scale-75" />
							</div>
							<div class="min-w-0">
								<div class="font-bold">URL</div>
								<div class="text-sm break-all opacity-75"><code>{loggerUrl}</code></div>
							</div>
						</div>

						<ConnectedClients {clients} {fetchedAt} {now} />
					{/if}
				</div>
			{/if}

			<Collapsible
				open={true}
				class="shadow-lg"
				isDirty={f.anyDirty}
				onRevert={() => f.revertAll()}
			>
				{#snippet icon()}
					<IconSettings class="h-6 w-6" />
				{/snippet}
				{#snippet title()}
					<span>Settings</span>
				{/snippet}
				{#if !settingsLoaded}
					<Spinner />
				{:else}
					<div class="flex w-full flex-col gap-2">
						<div class="flex items-center w-full">
							<div
								class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty(
									'wsLoggerEnabled'
								)
									? 'w-1 mr-2'
									: 'w-0'}"
							></div>
							<label
								class="label cursor-pointer flex-1 justify-between items-center whitespace-normal"
							>
								<div class="flex items-center gap-1 min-w-0 mr-4">
									<span>Enable WebSocket Logger</span>
									<DirtyMarker
										dirty={f.isDirty('wsLoggerEnabled')}
										onrevert={() => f.revert('wsLoggerEnabled')}
									/>
								</div>
								<input
									type="checkbox"
									class="toggle toggle-primary shrink-0"
									bind:checked={f.current.wsLoggerEnabled}
								/>
							</label>
						</div>
					</div>
					{#if f.current.wsLoggerEnabled}
						<div
							class="alert alert-warning shadow-md mt-1"
							transition:slide|local={{ duration: 300, easing: cubicOut }}
						>
							<IconWarning class="h-6 w-6 shrink-0" />
							<span>
								Be aware that Genius packets are sent to the connected logging clients synchronously
								on receiving them. A slow network connection or connecting multiple clients can
								significantly impact packet reception/processing and lead to packet miss.
							</span>
						</div>
					{:else if f.isDirty('wsLoggerEnabled')}
						<div
							class="alert alert-info shadow-md mt-1"
							transition:slide|local={{ duration: 300, easing: cubicOut }}
						>
							<span>Clients that are still connected are disconnected when this is saved.</span>
						</div>
					{/if}
					<div class="divider my-2"></div>
					<div class="mb-4 flex flex-wrap justify-end gap-2">
						<div class="tooltip tooltip-left" data-tip="Save WebSocket Logger settings">
							<button
								class="btn btn-primary"
								type="button"
								disabled={!f.anyDirty}
								onclick={() => {
									postWSLoggerSettings();
								}}
							>
								<IconSave class="h-6 w-6" />
								Save
							</button>
						</div>
					</div>
				{/if}
			</Collapsible>
		</SettingsCard>
	</div>
{:else}
	{goto('/')}
{/if}
