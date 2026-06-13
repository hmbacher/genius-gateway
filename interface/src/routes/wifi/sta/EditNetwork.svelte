<script lang="ts">
	import { modals } from 'svelte-modals';
	import { fly, slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import type { KnownNetworkItem } from '$lib/types/models';
	import InputPassword from '$lib/components/InputPassword.svelte';
	import FieldError from '$lib/components/FieldError.svelte';
	import { isIPv4, hasLength } from '$lib/utils/validators';
	import Cancel from '~icons/tabler/x';
	import Set from '~icons/tabler/check';

	interface Props {
		isOpen: boolean;
		title: string;
		networkEditable?: KnownNetworkItem;
		onSaveNetwork: any;
	}

	let {
		isOpen,
		title,
		networkEditable: _networkEditable = {
			ssid: '',
			password: '',
			static_ip_config: false,
			local_ip: undefined,
			subnet_mask: undefined,
			gateway_ip: undefined,
			dns_ip_1: undefined,
			dns_ip_2: undefined
		} as KnownNetworkItem,
		onSaveNetwork
	}: Props = $props();

	// Make passed object reactive to prevent Svelte warning 'binding_property_non_reactive'
	// https://github.com/sveltejs/svelte/issues/12320
	let networkEditable = $state(_networkEditable);

	// Create helper variable to achieve reactivity
	let staticIPConfig = $state(networkEditable.static_ip_config);

	const titleId = `edit-network-title-${Math.random().toString(36).slice(2)}`;

	const ssidError = $derived(!hasLength(networkEditable.ssid, 3, 32));
	const localIPError = $derived(staticIPConfig && !isIPv4(networkEditable.local_ip ?? ''));
	const gatewayIPError = $derived(staticIPConfig && !isIPv4(networkEditable.gateway_ip ?? ''));
	const subnetMaskError = $derived(staticIPConfig && !isIPv4(networkEditable.subnet_mask ?? ''));
	const dns1Error = $derived(staticIPConfig && !isIPv4(networkEditable.dns_ip_1 ?? ''));
	const dns2Error = $derived(
		staticIPConfig && !!networkEditable.dns_ip_2 && !isIPv4(networkEditable.dns_ip_2)
	);
	const hasErrors = $derived(
		ssidError || localIPError || gatewayIPError || subnetMaskError || dns1Error || dns2Error
	);

	function handleSubmit() {
		if (!hasErrors) {
			networkEditable.static_ip_config = staticIPConfig;
			onSaveNetwork(networkEditable);
		}
	}
</script>

{#if isOpen}
	<div
		role="dialog"
		aria-modal="true"
		aria-labelledby={titleId}
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center overflow-y-auto"
		transition:fly={{ y: 50 }}
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-md flex-col justify-between p-4 shadow-lg md:w-[28rem]"
		>
			<h2 id={titleId} class="text-base-content text-start text-2xl font-bold">{title}</h2>
			<div class="divider my-2"></div>
			<form
				class="fieldset"
				onsubmit={(e) => {
					e.preventDefault();
					handleSubmit();
				}}
				novalidate
			>
				<div
					class="grid w-full grid-cols-1 content-center gap-4 px-4 sm:grid-cols-2"
					transition:slide|local={{ duration: 300, easing: cubicOut }}
				>
					<div>
						<label class="label" for="ssid">SSID</label>
						<input
							type="text"
							class="input input-bordered invalid:border-error w-full invalid:border-2 {ssidError ? 'border-error border-2' : ''}"
							bind:value={networkEditable.ssid}
							id="ssid"
							minlength="3"
							maxlength="32"
							required
						/>
						<FieldError show={ssidError} message="SSID must be between 3 and 32 characters long." />
					</div>
					<div>
						<label class="label" for="pwd">Password</label>
						<InputPassword bind:value={networkEditable.password} id="pwd" />
					</div>
					<label
						class="label inline-flex cursor-pointer content-end justify-start gap-4 sm:col-span-2"
					>
						<input
							type="checkbox"
							bind:checked={staticIPConfig}
							class="checkbox checkbox-primary"
						/>
						<span>Use static IP config</span>
					</label>
				</div>

				{#if staticIPConfig}
					<div
						class="grid w-full grid-cols-1 content-center mt-4 gap-4 px-4 sm:grid-cols-2"
						transition:slide|local={{ duration: 300, easing: cubicOut }}
					>
						<div>
							<label class="label" for="localIP">Local IP</label>
							<input
								type="text"
								class="input input-bordered w-full {localIPError ? 'border-error border-2' : ''}"
								minlength="7"
								maxlength="15"
								size="15"
								bind:value={networkEditable.local_ip}
								id="localIP"
								required
							/>
							<FieldError show={localIPError} message="Local IP must be a valid IPv4 address." />
						</div>

						<div>
							<label class="label" for="gateway">Gateway IP</label>
							<input
								type="text"
								class="input input-bordered w-full {gatewayIPError ? 'border-error border-2' : ''}"
								minlength="7"
								maxlength="15"
								size="15"
								bind:value={networkEditable.gateway_ip}
								id="gateway"
								required
							/>
							<FieldError show={gatewayIPError} message="Gateway IP must be a valid IPv4 address." />
						</div>
						<div>
							<label class="label" for="subnet">Subnet Mask</label>
							<input
								type="text"
								class="input input-bordered w-full {subnetMaskError ? 'border-error border-2' : ''}"
								minlength="7"
								maxlength="15"
								size="15"
								bind:value={networkEditable.subnet_mask}
								id="subnet"
								required
							/>
							<FieldError show={subnetMaskError} message="Subnet Mask must be a valid IPv4 address." />
						</div>
						<div>
							<label class="label" for="dns_1">DNS 1</label>
							<input
								type="text"
								class="input input-bordered w-full {dns1Error ? 'border-error border-2' : ''}"
								minlength="7"
								maxlength="15"
								size="15"
								bind:value={networkEditable.dns_ip_1}
								id="dns_1"
								required
							/>
							<FieldError show={dns1Error} message="DNS 1 must be a valid IPv4 address." />
						</div>
						<div>
							<label class="label" for="dns_2">DNS 2</label>
							<input
								type="text"
								class="input input-bordered w-full {dns2Error ? 'border-error border-2' : ''}"
								minlength="7"
								maxlength="15"
								size="15"
								bind:value={networkEditable.dns_ip_2}
								id="dns_2"
								required
							/>
							<FieldError show={dns2Error} message="DNS 2 must be a valid IPv4 address." />
						</div>
					</div>
				{/if}

				<div class="divider my-2"></div>

				<div class="flex justify-end gap-2">
					<button
						class="btn btn-neutral text-neutral-content inline-flex items-center"
						onclick={() => {
							modals.close(1);
						}}
						type="button"
					>
						<Cancel class="mr-2 h-5 w-5" />
						<span>Cancel</span>
					</button>
					<button
						class="btn btn-primary text-primary-content inline-flex items-center"
						type="submit"
						disabled={hasErrors}
					>
						<Set class="mr-2 h-5 w-5" />
						<span>Set</span>
					</button>
				</div>
			</form>
		</div>
	</div>
{/if}
