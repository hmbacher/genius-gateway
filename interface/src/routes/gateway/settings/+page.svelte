<script lang="ts">
	import { goto } from '$app/navigation';
	import { onMount } from 'svelte';
	import { user } from '$lib/stores/user';
	import { page } from '$app/state';
	import { socket } from '$lib/stores/socket';
	import { notifications } from '$lib/components/toasts/notifications';
	import type { ReportSettings } from '$lib/types/models';
	import FieldError from '$lib/components/FieldError.svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import DirtyMarker from '$lib/components/DirtyMarker.svelte';
	import DirtyField from '$lib/components/DirtyField.svelte';
	import { createDirtyState } from '$lib/utils/dirtyState.svelte';
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

	const f = createDirtyState<GatewaySettings>({ ...defaultSettings });

	async function getGatewaySettings() {
		try {
			const response = await fetch('/rest/gateway-settings', {
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

	async function postGatewaySettings() {
		try {
			const response = await fetch('/rest/gateway-settings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(f.current)
			});
			if (response.status == 200) {
				notifications.success('Gateway settings updated.', 3000);
				f.reset(await response.json());
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

	const r = createDirtyState<ReportSettings>({ ...defaultReportSettings });

	async function getReportSettings() {
		try {
			const response = await fetch('/rest/report-settings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			r.reset(await response.json());
		} catch (error) {
			console.error('Error:', error);
		}
	}

	const MAX_NAME_LEN = 80;
	const MAX_ADDRESS_LEN = 200;

	const propertyNameError = $derived(r.current.propertyName.length > MAX_NAME_LEN);
	const propertyAddressError = $derived(r.current.propertyAddress.length > MAX_ADDRESS_LEN);
	const customerNameError = $derived(r.current.customerName.length > MAX_NAME_LEN);
	const reportHasErrors = $derived(propertyNameError || propertyAddressError || customerNameError);

	async function postReportSettings() {
		try {
			const response = await fetch('/rest/report-settings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(r.current)
			});
			if (response.status === 200) {
				notifications.success('Report settings updated.', 3000);
				r.reset(await response.json());
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
			if (data && !f.anyDirty) {
				f.reset(data);
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
		<SettingsCard collapsible={false} isDirty={f.anyDirty} onRevert={() => f.revertAll()}>
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
					<div class="flex items-center w-full">
						<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('alert_on_unknown_detectors') ? 'w-1 mr-2' : 'w-0'}"></div>
						<label class="label cursor-pointer flex-1 justify-between items-center whitespace-normal">
							<div class="flex items-center gap-1 min-w-0 mr-4">
								<span>Process alerts from unknown smoke detectors</span>
								<DirtyMarker dirty={f.isDirty('alert_on_unknown_detectors')} onrevert={() => f.revert('alert_on_unknown_detectors')} />
							</div>
							<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.alert_on_unknown_detectors} />
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
					<div class="flex items-center w-full">
						<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('add_alarm_line_from_commissioning_packet') ? 'w-1 mr-2' : 'w-0'}"></div>
						<label class="label cursor-pointer flex-1 justify-between items-center whitespace-normal">
							<div class="flex items-center gap-1 min-w-0 mr-4">
								<span>Add alarm line ID of received <em>comissioning</em> packets automatically</span>
								<DirtyMarker dirty={f.isDirty('add_alarm_line_from_commissioning_packet')} onrevert={() => f.revert('add_alarm_line_from_commissioning_packet')} />
							</div>
							<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.add_alarm_line_from_commissioning_packet} />
						</label>
					</div>
					<div class="flex items-center w-full">
						<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('add_alarm_line_from_alarm_packet') ? 'w-1 mr-2' : 'w-0'}"></div>
						<label class="label cursor-pointer flex-1 justify-between items-center whitespace-normal">
							<div class="flex items-center gap-1 min-w-0 mr-4">
								<span>Add alarm line ID of received <em>alarming/silencing</em> packets automatically</span>
								<DirtyMarker dirty={f.isDirty('add_alarm_line_from_alarm_packet')} onrevert={() => f.revert('add_alarm_line_from_alarm_packet')} />
							</div>
							<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.add_alarm_line_from_alarm_packet} />
						</label>
					</div>
					<div class="flex items-center w-full">
						<div class="self-stretch bg-red-300 transition-all duration-200 ease-out {f.isDirty('add_alarm_line_from_line_test_packet') ? 'w-1 mr-2' : 'w-0'}"></div>
						<label class="label cursor-pointer flex-1 justify-between items-center whitespace-normal">
							<div class="flex items-center gap-1 min-w-0 mr-4">
								<span>Add alarm line ID of received <em>line test</em> packets automatically</span>
								<DirtyMarker dirty={f.isDirty('add_alarm_line_from_line_test_packet')} onrevert={() => f.revert('add_alarm_line_from_line_test_packet')} />
							</div>
							<input type="checkbox" class="toggle toggle-primary shrink-0" bind:checked={f.current.add_alarm_line_from_line_test_packet} />
						</label>
					</div>
				</div>
				<div class="divider my-2"></div>
				<div class="mb-4 flex flex-wrap justify-end gap-2">
					<div class="tooltip tooltip-left" data-tip="Save gateway settings">
						<button
							class="btn btn-primary"
							type="button"
							disabled={!f.anyDirty}
							onclick={postGatewaySettings}
						>
							<IconSave class="h-6 w-6" />
							Save
						</button>
					</div>
				</div>
			{/await}
		</SettingsCard>

		<SettingsCard collapsible={false} isDirty={r.anyDirty} onRevert={() => r.revertAll()}>
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
						<DirtyField dirty={r.isDirty('propertyName')} onrevert={() => r.revert('propertyName')}>
							<input
								type="text"
								class="input input-bordered w-full pr-10"
								placeholder="e.g. Mustermann House"
								maxlength={MAX_NAME_LEN}
								bind:value={r.current.propertyName}
							/>
						</DirtyField>
						<FieldError show={propertyNameError} message="Property name must not exceed {MAX_NAME_LEN} characters." />
					</label>
					<label class="form-control w-full">
						<div class="label">
							<span class="label-text">Property Address</span>
						</div>
						<DirtyField dirty={r.isDirty('propertyAddress')} onrevert={() => r.revert('propertyAddress')}>
							<textarea
								class="textarea textarea-bordered w-full pr-10"
								placeholder={"e.g. Musterstraße 1\n12345 Berlin"}
								maxlength={MAX_ADDRESS_LEN}
								rows="3"
								bind:value={r.current.propertyAddress}
							></textarea>
						</DirtyField>
						<FieldError show={propertyAddressError} message="Address must not exceed {MAX_ADDRESS_LEN} characters." />
					</label>
					<label class="form-control w-full">
						<div class="label">
							<span class="label-text">Customer / Owner</span>
						</div>
						<DirtyField dirty={r.isDirty('customerName')} onrevert={() => r.revert('customerName')}>
							<input
								type="text"
								class="input input-bordered w-full pr-10"
								placeholder="e.g. Max Mustermann"
								maxlength={MAX_NAME_LEN}
								bind:value={r.current.customerName}
							/>
						</DirtyField>
						<FieldError show={customerNameError} message="Customer name must not exceed {MAX_NAME_LEN} characters." />
					</label>
				</div>
				<div class="divider my-2"></div>
				<div class="mb-4 flex flex-wrap justify-end gap-2">
					<div class="tooltip tooltip-left" data-tip="Save report settings">
						<button
							class="btn btn-primary"
							type="button"
							disabled={!r.anyDirty || reportHasErrors}
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
