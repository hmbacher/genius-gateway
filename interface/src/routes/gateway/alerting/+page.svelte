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
	import Adjustments from '~icons/tabler/adjustments-alt';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	type AlertingSettings = {
		alertOnUnknownDetectors: boolean;
	};

	let alertingSettings: AlertingSettings = $state();

	async function getAlertingSettings() {
		try {
			const response = await fetch('/rest/gateway-settings', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});

			alertingSettings = await response.json();
		} catch (error) {
			console.error('Error:', error);
		}
		return;
	}

	async function postAlertingSettings(data: AlertingSettings) {
		try {
			const response = await fetch('/rest/gateway-settings', {
				method: 'POST',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(data)
			});

			if (response.status == 200) {
				notifications.success('Gateway settings updated.', 3000);
				alertingSettings = await response.json();
			} else {
				notifications.error('User not authorized.', 3000);
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
		<SettingsCard collapsible={false}>
			{#snippet icon()}
				<Adjustments class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
			{/snippet}
			{#snippet title()}
				<span>Alerting settings</span>
			{/snippet}
			{#await getAlertingSettings()}
				<Spinner />
			{:then nothing}
				<div class="relative w-full overflow-visible">
					<div class="form-control">
						<label class="label cursor-pointer">
							<span class="">Forward alerts from unknown smoke detectors</span>
							<input
								type="checkbox"
								class="toggle toggle-primary"
								bind:checked={alertingSettings.alertOnUnknownDetectors}
								onchange={() => postAlertingSettings(alertingSettings)}
							/>
						</label>
					</div>
				</div>
			{/await}
		</SettingsCard>
	</div>
{:else}
	{goto('/')}
{/if}
