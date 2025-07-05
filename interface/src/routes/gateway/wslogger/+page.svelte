<script lang="ts">
	import type { PageData } from './$types';
	import { goto } from '$app/navigation';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { WSLoggerSettings } from '$lib/types/models';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import IconSettings from '~icons/tabler/adjustments';
	import IconSave from '~icons/tabler/device-floppy';
	import IconInfo from '~icons/tabler/info-circle';
	import IconWarning from '~icons/tabler/alert-triangle';

		const defaultSettings: WSLoggerSettings = {
		wsLoggerEnabled: false
	};

	let loggerSettings: WSLoggerSettings = $state(defaultSettings);
	let strSettings: string = $state(JSON.stringify(defaultSettings)); // to recognize changes

	let isSettingsDirty: boolean = $derived(JSON.stringify(loggerSettings) !== strSettings);

	async function getWSLoggerSettings() {
		try {
			const response = await fetch('/rest/wslogger', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			loggerSettings = await response.json();
			strSettings = JSON.stringify(loggerSettings); // Store the recently loaded settings in a string variable
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function postWSLoggerSettings() {
		try {
			const response = await fetch('/rest/wslogger', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(loggerSettings)
			});

			if (response.status == 200) {
				notifications.success('WebSocket Logger settings updated.', 3000);
				loggerSettings = await response.json();
				strSettings = JSON.stringify(loggerSettings); // Store the recently loaded settings in a string variable
			} else {
				notifications.error('Error on updating WebSocket Logger settings.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}
</script>

{#if $user.admin}
	<div
		class="mx-0 my-1 flex flex-col space-y-4
     sm:mx-8 sm:my-8"
	>
		<SettingsCard collapsible={false} isDirty={isSettingsDirty}>
			{#snippet icon()}
				<IconSettings class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
			{/snippet}
			{#snippet title()}
				<span>WebSocket Logger Settings</span>
			{/snippet}
			{#await getWSLoggerSettings()}
				<Spinner />
			{:then nothing}
				<div class="flex w-full flex-col gap-2 px-2">
					<div>
						<label class="label cursor-pointer w-full justify-between">
							<span class="">Enable WebSocket Logger</span>
							<input
								type="checkbox"
								class="toggle toggle-primary"
								bind:checked={loggerSettings.wsLoggerEnabled}
							/>
						</label>
					</div>
				</div>
				{#if loggerSettings.wsLoggerEnabled}
					<div
						class="alert alert-info shadow-md mt-1"
						transition:slide|local={{ duration: 300, easing: cubicOut }}
					>
						<IconInfo class="h-6 w-6 shrink-0" />
						<div>
							<div>Logging tools can connect via WebSocket using the following URL:</div>
							<div class="mt-1 font-semibold">
								<code>
									ws://{window.location.host}/ws/logger
								</code>
							</div>
						</div>
					</div>
					<div
						class="alert alert-warning shadow-md mt-1"
						transition:slide|local={{ duration: 300, easing: cubicOut }}
					>
						<IconWarning class="h-6 w-6 shrink-0" />
						<span>
							Be aware that Genius packets are sent to the connected logging clients synchronously on receiving
							them. A slow network connection or connecting multiple clients can significantly impact
							packet reception/processing and lead to packet miss.
						</span>
					</div>
				{/if}
				<div class="divider my-2"></div>
				<div class="mb-4 flex flex-wrap justify-end gap-2">
					<div class="tooltip tooltip-left" data-tip="Save WebSocket Logger settings">
						<button
							class="btn btn-primary"
							type="button"
							disabled={!isSettingsDirty}
							onclick={() => {
								postWSLoggerSettings();
							}}
						>
							<IconSave class="h-6 w-6" />
							Save
						</button>
					</div>
				</div>
			{/await}
		</SettingsCard>
	</div>
{:else}
	{goto('/')}
{/if}
