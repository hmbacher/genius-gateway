<script lang="ts">
	import type { PageData } from './$types';
	import { modals } from 'svelte-modals';
	import { geniusDevices } from '$lib/stores/geniusDevices.svelte';
	import { user } from '$lib/stores/user';
	import { notifications } from '$lib/components/toasts/notifications';
	import DeviceStatusCard from '$lib/components/DeviceStatusCard.svelte';
	import RadioSetupBanner from '$lib/components/RadioSetupBanner.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import { alarmLineNames } from '$lib/stores/alarmLineNames.svelte';
	import { deviceRuntime } from '$lib/stores/deviceRuntime.svelte';
	import { groupByAlarmLine, lineHex, lineKeyOf, type LineGroup } from '$lib/genius/lineGroups';
	import AlarmEndingDialog from './AlarmEndingDialog.svelte';
	import Info from '~icons/tabler/info-circle';
	import BellOff from '~icons/tabler/bell-off';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

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

	function authHeaders(): Record<string, string> {
		return {
			Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
			'Content-Type': 'application/json'
		};
	}

	// ── Smoke detectors grouped by alarm line (same grouping as the smoke detector list) ──
	const groups = $derived(groupByAlarmLine(geniusDevices.devices, alarmLineNames.names));
	// A heading only makes sense once there is at least one real alarm line
	const showHeadings = $derived(groups.some((g) => g.key !== 0));
	const alarmingIn = (g: LineGroup) => g.devices.filter((d) => d.isAlarming).length;
	const faultsIn = (g: LineGroup) => g.devices.filter((d) => deviceRuntime.byId[d.id]?.fault).length;
	const lineKey = $derived(geniusDevices.devices.map((d) => lineKeyOf(d)).join(','));

	$effect(() => {
		if (!geniusDevices.isLoaded) return;
		void lineKey; // names are fetched again when a detector moves to another line
		alarmLineNames.load(authHeaders());
	});

	// RAM-only runtime state of the gateway (radio link faults); never persisted
	$effect(() => deviceRuntime.start(authHeaders()));
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
				{#if geniusDevices.isAlarming}
					<div class="col-span-full">
						<div class="tooltip tooltip-left" data-tip="End all alarms">
							<button class="btn btn-primary btn-wide" onclick={handleEndAlarms}>
								<BellOff class="mr-2 h-6 w-6" />
								<span>End all active alarms</span>
							</button>
						</div>
					</div>
				{/if}
				{#each groups as group (group.key)}
					{#if showHeadings}
						<!-- Alarm line heading, spans the whole grid row -->
						<div class="col-span-full flex w-full flex-wrap items-center gap-x-3 gap-y-1 border-b border-base-300 pb-1">
							<h2 class="text-lg font-semibold">{group.name}</h2>
							{#if group.key !== 0}
								<span class="font-mono text-xs text-base-content/50">{lineHex(group.key)}</span>
							{/if}
							<span class="text-sm text-base-content/60">
								{group.devices.length}
								{group.devices.length === 1 ? 'detector' : 'detectors'}
							</span>
							{#if alarmingIn(group) > 0}
								<span class="badge badge-error badge-sm">{alarmingIn(group)} alarming</span>
							{/if}
							{#if faultsIn(group) > 0}
								<span
									class="badge badge-warning badge-sm"
									title="Radio link fault reported by a detector of this line"
								>
									{faultsIn(group)} link {faultsIn(group) === 1 ? 'fault' : 'faults'}
								</span>
							{/if}
						</div>
					{/if}
					{#each group.devices as device (device.id)}
						<a
							href="/gateway/smoke-detectors"
							aria-label="Open smoke detectors page"
							class="block w-full max-w-120 hover:scale-[1.03] active:scale-[0.97] transition-transform"
						>
							<DeviceStatusCard detector={device} />
						</a>
					{/each}
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
