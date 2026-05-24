<script lang="ts">
	import { APP_VERSION_FULL } from '$lib/version';
	import { socket } from '$lib/stores/socket';
	import { telemetry } from '$lib/stores/telemetry';
	import { onMount, onDestroy } from 'svelte';
	import Refresh from '~icons/tabler/refresh';
	import AlertTriangle from '~icons/tabler/alert-triangle';

	let firmwareVersion = $state<string | null>(null);
	let off: (() => void) | null = null;

	onMount(() => {
		// In `vite dev` the frontend is hot-served from the developer's machine
		// while the firmware runs whatever version it was flashed with — a
		// permanent, expected mismatch. Suppress the banner there.
		if (import.meta.env.DEV) return;
		off = socket.on<{ version: string }>('app_version', (data) => {
			firmwareVersion = data?.version ?? null;
		});
	});

	onDestroy(() => off?.());

	// FirmwareUpdateDialog owns the UX during an OTA initiated from this tab:
	// it shows a progress dialog and a 15-second auto-reload countdown. We
	// suppress the banner during those states to avoid two reload prompts
	// racing each other. The banner remains the fallback for the case the
	// dialog never sees (OTA flashed from CLI, Home Assistant, or another tab).
	const otaActive = $derived(
		$telemetry.ota_status.status === 'preparing' ||
			$telemetry.ota_status.status === 'progress' ||
			$telemetry.ota_status.status === 'finished'
	);

	const mismatch = $derived(
		!otaActive && firmwareVersion !== null && firmwareVersion !== APP_VERSION_FULL
	);

	function reload() {
		// Cache-busting query param forces a fresh fetch even if the currently
		// cached shell was pinned as immutable by a pre-fix firmware (older
		// builds shipped /index.html with max-age=31536000, immutable). The new
		// shell references content-hashed bundles which are also fetched fresh;
		// old hashed bundles stay cached but are never re-referenced.
		// `replace` keeps the ?v= URL out of history.
		location.replace(location.pathname + '?v=' + Date.now());
	}
</script>

{#if mismatch}
	<div
		role="alert"
		class="bg-warning text-warning-content sticky top-0 z-50 flex flex-wrap items-center justify-between gap-2 px-4 py-2 shadow"
	>
		<div class="flex items-center gap-2">
			<AlertTriangle class="h-5 w-5 shrink-0" />
			<span>The gateway firmware was updated. Reload to use the latest UI.</span>
		</div>
		<button class="btn btn-sm btn-primary" onclick={reload}>
			<Refresh class="h-4 w-4" />
			Reload
		</button>
	</div>
{/if}
