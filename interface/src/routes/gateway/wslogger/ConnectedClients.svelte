<script lang="ts">
	import type { WSLoggerClient } from '$lib/types/models';
	import { shortUserAgent } from '$lib/utils/userAgent';
	import { formatBytes, formatDuration } from '$lib/utils/formatSize';
	import { formatDateTimeSeconds } from '$lib/utils/formatDate';
	import IconPlugOff from '~icons/tabler/plug-connected-x';
	import IconVisualizer from '~icons/tabler/zoom-scan';
	import IconBrowser from '~icons/tabler/browser';
	import IconTool from '~icons/tabler/terminal-2';

	interface Props {
		clients: WSLoggerClient[];
		/** When the list was fetched (Date.now()). */
		fetchedAt: number;
		/** Current time, ticking - the connection ages count on between two fetches. */
		now: number;
	}

	let { clients, fetchedAt, now }: Props = $props();

	/** Name the built-in Packet Visualizer gives itself when it connects. */
	const VISUALIZER = 'packet-visualizer';

	/** Age of the connection now: the gateway's value plus the time since it was fetched. */
	function age(c: WSLoggerClient): number {
		return c.connectedMs + Math.max(0, now - fetchedAt);
	}

	function label(c: WSLoggerClient): string {
		if (c.name === VISUALIZER) return 'Packet Visualizer';
		if (c.name) return c.name;
		return shortUserAgent(c.userAgent) || 'Unknown client';
	}

	/** Address, and the browser / tool if the first line is a name (else it is the first line). */
	function detail(c: WSLoggerClient): string {
		const address = `${c.address}:${c.port}`;
		const agent = c.name ? shortUserAgent(c.userAgent) : '';
		return agent ? `${address} · ${agent}` : address;
	}

	function iconFor(c: WSLoggerClient) {
		if (c.name === VISUALIZER) return IconVisualizer;
		return /Mozilla\//.test(c.userAgent ?? '') ? IconBrowser : IconTool;
	}
</script>

<!-- Heading between the status of the logger and its clients: the rows below are connections -->
<div
	class="divider divider-start my-1 text-sm font-semibold text-base-content/70"
	data-testid="wslogger-clients-heading"
>
	Connected Clients ({clients.length})
</div>
{#each clients as c (c.id)}
	{@const Icon = iconFor(c)}
	<div
		class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2"
		data-testid="wslogger-client"
	>
		<div class="mask mask-hexagon bg-primary h-auto w-10 shrink-0">
			<Icon class="text-primary-content h-auto w-full scale-75" />
		</div>
		<div class="min-w-0 flex-1" title={c.userAgent ?? ''}>
			<div class="truncate font-bold">{label(c)}</div>
			<div class="truncate text-sm opacity-75">{detail(c)}</div>
		</div>
		<div class="shrink-0 text-right text-sm">
			<div
				class="tooltip tooltip-left whitespace-nowrap"
				data-tip={'Connected since ' + formatDateTimeSeconds(new Date(fetchedAt - c.connectedMs))}
			>
				connected for {formatDuration(age(c))}
			</div>
			<div
				class="tooltip tooltip-left block whitespace-nowrap opacity-75"
				data-tip="Radio packets sent to the client (every repetition counted) and their data"
			>
				{c.packets.toLocaleString()}
				{c.packets === 1 ? 'packet' : 'packets'} · {formatBytes(c.bytes)}
			</div>
		</div>
	</div>
{:else}
	<div
		class="rounded-box bg-base-100 flex items-center space-x-3 px-4 py-2"
		data-testid="wslogger-no-clients"
	>
		<div class="mask mask-hexagon bg-base-300 h-auto w-10 shrink-0">
			<IconPlugOff class="h-auto w-full scale-75 text-base-content/60" />
		</div>
		<div>
			<div class="font-bold">No client connected</div>
			<div class="text-sm opacity-75">
				Logging tools and open Packet Visualizers appear here as soon as they connect
			</div>
		</div>
	</div>
{/each}
