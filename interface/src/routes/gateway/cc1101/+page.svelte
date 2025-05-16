<script lang="ts">
	import type { PageData } from './$types';
	import type { CC1101State } from '$lib/types/models';
	import { user } from '$lib/stores/user';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import IconCPU from '~icons/tabler/cpu';
	import IconAlert from '~icons/tabler/alert-hexagon';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';

	interface Props {
		data: PageData;
	}

	let { data }: Props = $props();

	type State = {
		name: string;
		state: string;
	};

	const CC1101_STATES = [
		{ name: 'SLEEP', state: 'SLEEP' },
		{ name: 'IDLE', state: 'IDLE' },
		{ name: 'XOFF', state: 'XOFF' },
		{ name: 'VCOON_MC', state: 'MANCAL' },
		{ name: 'REGON_MC', state: 'MANCAL' },
		{ name: 'MANCAL', state: 'MANCAL' },
		{ name: 'VCOON', state: 'FS_WAKEUP' },
		{ name: 'REGON', state: 'FS_WAKEUP' },
		{ name: 'STARTCAL', state: 'CALIBRATE' },
		{ name: 'BWBOOST', state: 'SETTLING' },
		{ name: 'FS_LOCK', state: 'SETTLING' },
		{ name: 'IFADCON', state: 'SETTLING' },
		{ name: 'ENDCAL', state: 'CALIBRATE' },
		{ name: 'RX', state: 'RX' },
		{ name: 'RX_END', state: 'RX' },
		{ name: 'RX_RST', state: 'RX' },
		{ name: 'TXRX_SWITCH', state: 'TXRX_SETTLING' },
		{ name: 'RXFIFO_OVERFLOW', state: 'RXFIFO_OVERFLOW' },
		{ name: 'FSTXON', state: 'FSTXON' },
		{ name: 'TX', state: 'TX' },
		{ name: 'TX_END', state: 'TX' },
		{ name: 'RXTX_SWITCH', state: 'RXTX_SETTLING' },
		{ name: 'TXFIFO_UNDERFLOW', state: 'TXFIFO_UNDERFLOW' }
	];

	let cc1101State: CC1101State = $state({
		state_success: true,
		state: 0
	});

	let error = $derived(
		cc1101State.state_success === false ||
			cc1101State.state < 0 ||
			cc1101State.state >= CC1101_STATES.length
	);

	async function getCC1101State() {
		try {
			const response = await fetch('/rest/cc1101/state', {
				method: 'GET',
				headers: {
					Authorization: data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				}
			});
			cc1101State = await response.json();
		} catch (error) {
			console.error('An error occurred while fetching the CC1101 state: ', error);
			throw error;
		}
	}
</script>

{#if page.data.features.cc1101_controller && $user.admin}
	<SettingsCard collapsible={false}>
		{#snippet icon()}
			<IconCPU class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
		{/snippet}
		{#snippet title()}
			<span>CC1101 State</span>
		{/snippet}
		{#await getCC1101State()}
			<Spinner text="Requesting state..." />
		{:then nothing}
			<div
				class="flex w-full flex-col space-y-1"
				transition:slide|local={{ duration: 300, easing: cubicOut }}
			>
				<div class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2">
					<div class="mask mask-hexagon {error ? 'bg-error' : 'bg-primary'} h-auto w-10 flex-none">
						{#if error}
							<IconAlert class="text-primary-content h-auto w-full scale-75" />
						{:else}
							<IconCPU class="text-primary-content h-auto w-full scale-75" />
						{/if}
					</div>
					<div class="{error ? 'text-error' : 'text-base-content'}">
						<div class="font-bold">Main Radio Control State Machine State (MARCSTATE)</div>
						<div class="text-sm opacity-75">
							{#if !cc1101State.state_success}
								SPI error while obtaining state.
							{:else if cc1101State.state < 0 || cc1101State.state >= CC1101_STATES.length}
								Invalid state: 0x{cc1101State.state.toString(16).padStart(2, '0')}
							{:else}
								{CC1101_STATES[cc1101State.state].state} / {CC1101_STATES[cc1101State.state].name}
							{/if}
						</div>
					</div>
				</div>
			</div>
		{:catch error}
			<div class="flex items-center justify-center text-error">
				<IconAlert class="h-5 w-5 mr-2" />
				<span>An error occurred while fetching the CC1101 state.</span>
			</div>
		{/await}
	</SettingsCard>
{:else}
	{goto('/')}
{/if}
