<script lang="ts">
	import type { PageData } from './$types';
	import { goto } from '$app/navigation';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { notifications } from '$lib/components/toasts/notifications';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import IconSettings from '~icons/tabler/adjustments';
	import IconAlarm from '~icons/tabler/alert-hexagon';
	import IconDetector from '~icons/tabler/alarm-smoke';
	import IconAlarmLine from '~icons/tabler/topology-ring-2';
	import IconSave from '~icons/tabler/device-floppy';

	type GatewaySettings = {
		alert_on_unknown_detectors: boolean;
		add_alarm_line_from_commissioning_packet: boolean;
		add_alarm_line_from_alarm_packet: boolean;
		add_alarm_line_from_line_test_packet: boolean;
	};

	const defaultSettings: GatewaySettings = {
		alert_on_unknown_detectors: false,
		add_alarm_line_from_commissioning_packet: false,
		add_alarm_line_from_alarm_packet: false,
		add_alarm_line_from_line_test_packet: false
	};

	let gatewaySettings: GatewaySettings = $state(defaultSettings);
	let strSettings: string = $state(JSON.stringify(defaultSettings)); // to recognize changes

	let isSettingsDirty: boolean = $derived(JSON.stringify(gatewaySettings) !== strSettings);

	async function getGatewaySettings() {
		try {
			const response = await fetch('/rest/gateway-settings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			gatewaySettings = await response.json();
			strSettings = JSON.stringify(gatewaySettings); // Store the recently loaded settings in a string variable
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function postGatewaySettings() {
		try {
			const response = await fetch('/rest/gateway-settings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(gatewaySettings)
			});

			if (response.status == 200) {
				notifications.success('Gateway settings updated.', 3000);
				gatewaySettings = await response.json();
				strSettings = JSON.stringify(gatewaySettings); // Store the recently loaded settings in a string variable
			} else {
				notifications.error('Updating Gateway settings failed.', 3000);
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
				<span>Gateway Settings</span>
			{/snippet}
			{#await getGatewaySettings()}
				<Spinner />
			{:then nothing}
				<div class="flex w-full flex-col gap-2 px-2">
					<div>
						<span class="inline-flex items-center gap-2">
							<IconAlarm class="h-6 w-6" />
							<span class="font-medium">Alarming</span>
						</span>
					</div>
					<div>
						<label class="label cursor-pointer w-full justify-between">
							<span class="">Process alerts from unknown smoke detectors</span>
							<input
								type="checkbox"
								class="toggle toggle-primary"
								bind:checked={gatewaySettings.alert_on_unknown_detectors}
							/>
						</label>
					</div>
					<div class="divider my-2"></div>
				</div>
				<div class="flex w-full flex-col gap-2 px-2">
					<div>
						<span class="inline-flex items-center gap-2">
							<IconAlarmLine class="h-6 w-6" />
							<span class="font-medium">Alarm lines</span>
						</span>
					</div>
					<div>
						<label class="label cursor-pointer w-full justify-between">
							<span class="">Add alarm line ID of received <em>comissioning</em> packets automatically</span
							>
							<input
								type="checkbox"
								class="toggle toggle-primary"
								bind:checked={gatewaySettings.add_alarm_line_from_commissioning_packet}
							/>
						</label>
					</div>
					<div>
						<label class="label cursor-pointer w-full justify-between">
							<span class=""
								>Add alarm line ID of received <em>alarming/silencing</em> packets automatically</span
							>
							<input
								type="checkbox"
								class="toggle toggle-primary"
								bind:checked={gatewaySettings.add_alarm_line_from_alarm_packet}
							/>
						</label>
					</div>
					<div>
						<label class="label cursor-pointer w-full justify-between">
							<span class="">Add alarm line ID of received <em>line test</em> packets automatically</span>
							<input
								type="checkbox"
								class="toggle toggle-primary"
								bind:checked={gatewaySettings.add_alarm_line_from_line_test_packet}
							/>
						</label>
					</div>
				</div>
				<div class="divider my-2"></div>
				<div class="mb-4 flex flex-wrap justify-end gap-2">
					<div class="tooltip tooltip-left" data-tip="Save gateway settings">
						<button
							class="btn btn-primary"
							type="button"
							disabled={!isSettingsDirty}
							onclick={() => {
								postGatewaySettings();
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
