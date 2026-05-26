<script lang="ts">
	import { goto } from '$app/navigation';
	import { onMount } from 'svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { socket } from '$lib/stores/socket';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { ReportSettings } from '$lib/types/models';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import IconSettings from '~icons/tabler/adjustments';
	import IconAlarm from '~icons/tabler/alert-hexagon';
	import IconAlarmLine from '~icons/tabler/topology-ring-2';
	import IconSave from '~icons/tabler/device-floppy';
	import IconReport from '~icons/tabler/file-type-pdf';

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

	// ── Report Settings ──────────────────────────────────────────────────────

	const defaultReportSettings: ReportSettings = {
		propertyName: '',
		propertyAddress: '',
		customerName: ''
	};

	let reportSettings: ReportSettings = $state({ ...defaultReportSettings });
	let strReportSettings: string = $state(JSON.stringify(defaultReportSettings));

	let isReportSettingsDirty: boolean = $derived(
		JSON.stringify(reportSettings) !== strReportSettings
	);

	async function getReportSettings() {
		try {
			const response = await fetch('/rest/report-settings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			reportSettings = await response.json();
			strReportSettings = JSON.stringify(reportSettings);
		} catch (error) {
			console.error('Error:', error);
		}
	}

	async function postReportSettings() {
		try {
			const response = await fetch('/rest/report-settings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(reportSettings)
			});
			if (response.status === 200) {
				notifications.success('Report settings updated.', 3000);
				reportSettings = await response.json();
				strReportSettings = JSON.stringify(reportSettings);
			} else {
				notifications.error('Updating report settings failed.', 3000);
			}
		} catch (error) {
			console.error('Error:', error);
		}
	}

	// WebSocket synchronization for real-time updates from backend (e.g., MQTT changes)
	onMount(() => {
		const unsubscribe = socket.on<GatewaySettings>('gateway-settings', (data) => {
			if (data) {
				// Only update if settings are not currently being edited locally
				if (!isSettingsDirty) {
					gatewaySettings = data;
					strSettings = JSON.stringify(data);
				}
			}
		});

		return () => {
			unsubscribe();
		};
	});
</script>

{#if $user.admin}
	<div
		class="mx-0 my-1 flex flex-col space-y-4
     sm:mx-8 sm:my-8"
	>
		<SettingsCard collapsible={false} isDirty={isSettingsDirty}>
			{#snippet icon()}
				<IconSettings class="h-6 w-6" />
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
						<label class="label cursor-pointer w-full justify-between items-start whitespace-normal">
							<span class="min-w-0 mr-4">Process alerts from unknown smoke detectors</span>
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
						<label class="label cursor-pointer w-full justify-between items-start whitespace-normal">
							<span class="min-w-0 mr-4">Add alarm line ID of received <em>comissioning</em> packets automatically</span
							>
							<input
								type="checkbox"
								class="toggle toggle-primary"
								bind:checked={gatewaySettings.add_alarm_line_from_commissioning_packet}
							/>
						</label>
					</div>
					<div>
						<label class="label cursor-pointer w-full justify-between items-start whitespace-normal">
							<span class="min-w-0 mr-4">Add alarm line ID of received <em>alarming/silencing</em> packets automatically</span
							>
							<input
								type="checkbox"
								class="toggle toggle-primary"
								bind:checked={gatewaySettings.add_alarm_line_from_alarm_packet}
							/>
						</label>
					</div>
					<div>
						<label class="label cursor-pointer w-full justify-between items-start whitespace-normal">
							<span class="min-w-0 mr-4">Add alarm line ID of received <em>line test</em> packets automatically</span>
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

		<SettingsCard collapsible={false} isDirty={isReportSettingsDirty}>
			{#snippet icon()}
				<IconReport class="h-6 w-6" />
			{/snippet}
			{#snippet title()}
				<span>Report Settings</span>
			{/snippet}
			{#await getReportSettings()}
				<Spinner />
			{:then}
				<div class="flex w-full flex-col gap-4 px-2">
					<p class="text-sm text-base-content/60">
						These fields appear in the header of exported PDF reports. All fields are optional.
					</p>
					<label class="form-control w-full">
						<div class="label">
							<span class="label-text">Property Name</span>
						</div>
						<input
							type="text"
							class="input input-bordered w-full"
							placeholder="e.g. Mustermann House"
							maxlength="256"
							bind:value={reportSettings.propertyName}
						/>
					</label>
					<label class="form-control w-full">
						<div class="label">
							<span class="label-text">Property Address</span>
						</div>
						<textarea
							class="textarea textarea-bordered w-full"
							placeholder={"e.g. Musterstraße 1\n12345 Berlin"}
							maxlength="256"
							rows="3"
							bind:value={reportSettings.propertyAddress}
						></textarea>
					</label>
					<label class="form-control w-full">
						<div class="label">
							<span class="label-text">Customer / Owner</span>
						</div>
						<input
							type="text"
							class="input input-bordered w-full"
							placeholder="e.g. Max Mustermann"
							maxlength="256"
							bind:value={reportSettings.customerName}
						/>
					</label>
				</div>
				<div class="divider my-2"></div>
				<div class="mb-4 flex flex-wrap justify-end gap-2">
					<div class="tooltip tooltip-left" data-tip="Save report settings">
						<button
							class="btn btn-primary"
							type="button"
							disabled={!isReportSettingsDirty}
							onclick={postReportSettings}
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
