<script lang="ts">
	import { onMount } from 'svelte';
	import type { PageData } from './$types';
	import type { GeniusDevice } from '$lib/types/models';
	import { modals } from 'svelte-modals';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import { user } from '$lib/stores/user';
	import { watchHaEntityRename } from '$lib/stores/haEntityRename.svelte';
	import { notifications } from '$lib/components/toasts/notifications';
	import DeviceStatusCard from '$lib/components/DeviceStatusCard.svelte';
	import RadioSetupBanner from '$lib/components/RadioSetupBanner.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import AlarmEndingDialog from './AlarmEndingDialog.svelte';
	import DeviceDetailsDialog from './gateway/smoke-detectors/DeviceDetailsDialog.svelte';
	import { openDeviceEditor } from './gateway/smoke-detectors/deviceSave.svelte';
	import ActivityLog from './gateway/smoke-detectors/ActivityLog.svelte';
	import Info from '~icons/tabler/info-circle';
	import BellOff from '~icons/tabler/bell-off';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	// Editing a location here can make the gateway rename the detector's Home Assistant
	// entities; it reports that through a WebSocket event (progress dialog). Only administrators
	// can edit, so only they need to watch for it on this page.
	onMount(() => {
		if ($user.admin) return watchHaEntityRename();
	});

	/**
	 * Opens the clicked detector on top of the Overview: the alarm log for alarming devices,
	 * the (read-only) details dialog for all others. Readout and "set line" stay on the
	 * Smoke Detectors page, so the user never leaves the Overview.
	 */
	function openDevice(device: GeniusDevice) {
		if (device.isAlarming) {
			modals.open(ActivityLog, {
				title: 'Activity log',
				geniusDevice: device,
				alarmsOnly: true,
				onDetails: () => openDetails(device)
			});
		} else {
			openDetails(device);
		}
	}

	/** Device Details are read-only and open for every user; editing is for administrators. */
	function openDetails(device: GeniusDevice) {
		modals.open(DeviceDetailsDialog, {
			title: 'Device Details',
			device,
			onEdit: $user.admin ? () => openDeviceEditor(device) : undefined
		});
	}

	/** Name of a detector for screen readers; the location may be empty. */
	function deviceName(device: GeniusDevice): string {
		return device.location || `detector ${device.smokeDetector.sn}`;
	}

	async function postEndAlarms(blockingTime: number) {
		try {
			const response = await fetch('/rest/end-alarms', {
				method: 'POST',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify({ alarmBlockingTime: blockingTime })	
			});

			const jsonData = await response.json();
			if (jsonData || jsonData.success === true) {
				notifications.success('Alarms ended successfully.', 3000);
			} else {
				notifications.error('Failed to end alarms.', 3000);
			}
		} catch (error) {
			console.error('Error ending alarms:', error);
			notifications.error('Failed to end alarms.', 3000);
		}
	}

	function handleEndAlarms() {
		modals.open(AlarmEndingDialog, {
			title: 'End alarms',
			onSubmit: async (time: number) => {
				await postEndAlarms(time);
				modals.close();
			}
		});
	}
</script>

<div>
	<RadioSetupBanner />
	<div class="flex justify-center px-6 py-10">
		<div
			class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 xl:grid-cols-4 2xl:grid-cols-5 gap-10 justify-items-center"
		>
			{#if !geniusDevices.isLoaded}
				<div class="col-span-full">
					<Spinner text="Loading devices..." />
				</div>
			{:else if geniusDevices.devices.length > 0}
				{#if geniusDevices.isAlarming && $user.admin}
					<!-- Ending alarms is an administrator action; other users only watch. -->
					<div class="col-span-full">
						<div class="tooltip tooltip-left" data-tip="End all alarms">
							<button class="btn btn-primary btn-wide" onclick={handleEndAlarms}>
								<BellOff class="mr-2 h-6 w-6" />
								<span>End all active alarms</span>
							</button>
						</div>
					</div>
				{/if}
				{#each geniusDevices.devices as device (device.id)}
					<button
						type="button"
						onclick={() => openDevice(device)}
						aria-label={device.isAlarming
							? `Open alarm log of ${deviceName(device)}`
							: `Open details of ${deviceName(device)}`}
						class="block w-full max-w-120 cursor-pointer text-left hover:scale-[1.03] active:scale-[0.97] transition-transform"
					>
						<DeviceStatusCard detector={device} />
					</button>
				{/each}
			{:else}
				<div class="col-span-full">
					<div class="alert alert-info shadow-lg">
						<Info class="h-6 w-6 shrink-0" />
						<div>
							<p>No Genius devices set up yet.</p>
							<p>
								Wanna <a class="link" href="/gateway/smoke-detectors">add some smoke detectors</a> now?
							</p>
						</div>
					</div>
				</div>
			{/if}
		</div>
	</div>
</div>
