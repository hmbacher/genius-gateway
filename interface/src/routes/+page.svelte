<script lang="ts">
	import { user } from '$lib/stores/user';
	import type { PageData } from './$types';
	import type { GeniusDevices } from '$lib/types/models';
	import { jsonDateReviver } from '$lib/utils/misc';
	import DeviceStatusCard from '$lib/components/DeviceStatusCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import Info from '~icons/tabler/info-circle';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	let geniusDevices: GeniusDevices = $state({ devices: [] });

	async function getGeniusDevices() {
		try {
			const response = await fetch('/rest/gateway-devices', {
				method: 'GET',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			geniusDevices = JSON.parse(await response.text(), jsonDateReviver);

		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

</script>

<div>
	<div class="flex justify-center px-6 py-10">
		<div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 xl:grid-cols-4 2xl:grid-cols-5 gap-10 justify-items-center">
			{#await getGeniusDevices()}
				<div class="col-span-full flex justify-center">
					<Spinner />
				</div>
			{:then nothing}
				{#if geniusDevices.devices.length > 0}
					{#each geniusDevices.devices as device}
						<DeviceStatusCard detector={device} />
					{/each}
				{:else}
					<div class="col-span-full">
						<div class="alert alert-info shadow-lg">
							<Info class="h-6 w-6 shrink-0" />
							<div>
								<p>No Genius devices set up yet.</p>
								<p>Wanna <a class="link" href="/gateway/smoke-detectors">add some smoke detectors</a> now?</p>
							</div>
						</div>
					</div>
				{/if}
			{/await}
		</div>
	</div>
</div>
