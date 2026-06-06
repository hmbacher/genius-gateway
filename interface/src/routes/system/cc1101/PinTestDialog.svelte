<script lang="ts">
	import { modals, type ModalProps } from 'svelte-modals';
	import { fly } from 'svelte/transition';
	import { onMount } from 'svelte';
	import { notifications } from '$lib/components/toasts/notifications';
	import Spinner from '$lib/components/Spinner.svelte';
	import IconCheck from '~icons/tabler/circle-check-filled';
	import IconX from '~icons/tabler/circle-x-filled';
	import IconAlert from '~icons/tabler/alert-triangle';
	import IconSave from '~icons/tabler/device-floppy';
	import IconRetry from '~icons/tabler/reload';
	import IconEdit from '~icons/tabler/pencil';
	import IconCancel from '~icons/tabler/x';
	import type { CC1101ProbeResult } from '$lib/types/models';

	interface Props extends ModalProps {
		probe: () => Promise<CC1101ProbeResult>;
		save: () => Promise<void>;
	}
	let { isOpen, probe, save }: Props = $props();

	let result = $state<CC1101ProbeResult | null>(null);
	let testing = $state(false);
	let saving = $state(false);
	let failed = $state(false); // the probe request itself could not run

	let probeOk = $derived(result?.chip_detected === true && result?.gdo0_ok === true);

	async function runTest() {
		testing = true;
		failed = false;
		result = null;
		try {
			result = await probe();
		} catch (e) {
			console.error('Probe failed:', e);
			failed = true;
		} finally {
			testing = false;
		}
	}

	async function doSave() {
		saving = true;
		try {
			await save();
			notifications.success('Pins saved — re-initializing radio…', 4000);
			modals.close(1);
		} catch (e) {
			console.error('Save failed:', e);
			notifications.error('Failed to save pin configuration.', 4000);
		} finally {
			saving = false;
		}
	}

	const titleId = `pin-test-title-${Math.random().toString(36).slice(2)}`;
	onMount(runTest);
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
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-md flex-col p-4 shadow-lg md:w-[28rem]"
		>
			<h2 id={titleId} class="text-base-content text-start text-2xl font-bold">Test wiring</h2>
			<div class="divider my-2"></div>

			{#if testing}
				<Spinner text="Testing wiring…" />
			{:else if failed}
				<div class="flex items-center text-error">
					<IconAlert class="h-5 w-5" />
					<span>The self-test could not be run.</span>
				</div>
				<div class="divider my-2"></div>
				<div class="flex justify-end gap-2">
					<button class="btn btn-neutral" type="button" onclick={() => modals.close()}>
						<IconCancel class="h-5 w-5" /> Close
					</button>
					<button class="btn btn-primary" type="button" onclick={runTest}>
						<IconRetry class="h-5 w-5" /> Retry
					</button>
				</div>
			{:else if result}
				<div class="space-y-2">
					{#each [{ ok: result.spi_ok, label: 'SPI bus initialized' }, { ok: result.chip_detected, label: 'Chip detected' }, { ok: result.gdo0_ok, label: 'GDO0 line verified' }] as row}
						<div class="flex items-center gap-2">
							{#if row.ok}
								<IconCheck class="text-success h-5 w-5" />
							{:else}
								<IconX class="text-error h-5 w-5" />
							{/if}
							<span>{row.label}</span>
						</div>
					{/each}
				</div>
				<div class="divider my-2"></div>
				<div class="flex justify-end gap-2">
					{#if probeOk}
						<button
							class="btn btn-neutral"
							type="button"
							onclick={() => modals.close()}
							disabled={saving}
						>
							<IconCancel class="h-5 w-5" /> Cancel
						</button>
						<button class="btn btn-primary" type="button" onclick={doSave} disabled={saving}>
							{#if saving}
								<span class="loading loading-spinner loading-sm"></span>
							{:else}
								<IconSave class="h-5 w-5" />
							{/if}
							Save
						</button>
					{:else}
						<button class="btn btn-secondary" type="button" onclick={runTest}>
							<IconRetry class="h-5 w-5" /> Retry
						</button>
						<button class="btn btn-primary" type="button" onclick={() => modals.close()}>
							<IconEdit class="h-5 w-5" /> Change Pin Config
						</button>
					{/if}
				</div>
			{/if}
		</div>
	</div>
{/if}
