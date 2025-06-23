<script lang="ts">
	import { user } from '$lib/stores/user';
	import type { PageData } from './$types';
	import type { GeniusDevices } from '$lib/types/models';
	import { jsonDateReviver } from '$lib/utils';
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

<div >
	<div class="flex flex-wrap gap-10 px-10 py-10" style="justify-content: center;">
		{#await getGeniusDevices()}
				<Spinner />
		{:then nothing}
			{#if geniusDevices.devices.length > 0}
				{#each geniusDevices.devices as device}
					<DeviceStatusCard detector={device} />
				{/each}
			{:else}
			<div class="mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8">
				<div class="alert">
					<Info class="h-6 w-6 shrink-0 stroke-current" />
					<div>
						<p>No Genius devices set up yet.</p>
						<p>Wanna <a class="link link-primary" href="/gateway/smoke-detectors">add some smoke detectors</a> now?</p>
					</div>
				</div>
			</div>
			{/if}
		{/await}
	</div>
</div>
