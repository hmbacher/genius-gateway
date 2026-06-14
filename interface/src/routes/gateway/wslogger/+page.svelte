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
	import DirtyMarker from '$lib/components/DirtyMarker.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';
	import IconSettings from '~icons/tabler/adjustments';
	import IconSave from '~icons/tabler/device-floppy';
	import IconInfo from '~icons/tabler/info-circle';
	import IconWarning from '~icons/tabler/alert-triangle';

		const defaultSettings: WSLoggerSettings = {
		wsLoggerEnabled: false
	};

	const f = createDirtyState<WSLoggerSettings>({ ...defaultSettings });

	async function getWSLoggerSettings() {
		try {
			const response = await fetch('/rest/wslogger', {
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

	async function postWSLoggerSettings() {
		try {
			const response = await fetch('/rest/wslogger', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(f.current)
			});

			if (response.status == 200) {
				notifications.success('WebSocket Logger settings updated.', 3000);
				f.reset(await response.json());
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
		<SettingsCard collapsible={false} isDirty={f.anyDirty} onRevert={() => f.revertAll()}>
			{#snippet icon()}
				<IconSettings class="h-6 w-6" />
			{/snippet}
			{#snippet title()}
				<span>WebSocket Logger Settings</span>
			{/snippet}
			{#await getWSLoggerSettings()}
				<Spinner />
			{:then nothing}
				<div class="flex w-full flex-col gap-2">
					<div class="flex items-center w-full">
						<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('wsLoggerEnabled') ? 'w-1 mr-2' : 'w-0'}"></div>
						<label class="label cursor-pointer flex-1 justify-between items-center whitespace-normal">
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
						class="alert alert-info shadow-md mt-1"
						transition:slide|local={{ duration: 300, easing: cubicOut }}
					>
						<IconInfo class="h-6 w-6 shrink-0" />
						<div>
							<div>Logging tools can connect via WebSocket using the following URL:</div>
							<div class="mt-1 font-semibold">
								<code>
									{@html ('ws://' + window.location.host + '/ws/logger').replace(/\//g, '/<wbr>')}
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
			{/await}
		</SettingsCard>
	</div>
{:else}
	{goto('/')}
{/if}
