<script lang="ts">
	import { socket } from '$lib/stores/socket';
	import { onMount, onDestroy } from 'svelte';
	
	interface SystemHealth {
		uptime: number;
		heap_free: number;
		heap_min: number;
		heap_total: number;
		heap_usage_percent: number;
		wifi_connected: boolean;
		wifi_rssi: number;
		health_score: number;
		health_status: string;
		temperature_celsius: number;
		task_count: number;
		cpu_freq_mhz: number;
	}
	
	let metrics: any = {};
	let debugLogs: any[] = [];
	let systemHealth: SystemHealth | null = null;
	let isVisible = false;
	let updateInterval: number;
	
	// Subscribe to system health events
	function handleSystemHealth(data: SystemHealth) {
		systemHealth = data;
	}
	
	onMount(() => {
		// Enable debug logging
		socket.enableDebug(true);
		
		// Subscribe to system health
		socket.on('system_health', handleSystemHealth);
		
		// Update metrics every second
		updateInterval = setInterval(() => {
			metrics = socket.getMetrics();
			debugLogs = socket.getDebugLogs();
		}, 1000);
	});
	
	onDestroy(() => {
		if (updateInterval) {
			clearInterval(updateInterval);
		}
		socket.off('system_health', handleSystemHealth);
	});
	
	function formatDuration(ms: number): string {
		if (ms < 1000) return `${ms}ms`;
		if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
		return `${(ms / 60000).toFixed(1)}m`;
	}
	
	function formatUptime(ms: number): string {
		const totalSeconds = Math.floor(ms / 1000);
		const days = Math.floor(totalSeconds / 86400);
		const hours = Math.floor((totalSeconds % 86400) / 3600);
		const minutes = Math.floor((totalSeconds % 3600) / 60);
		
		if (days > 0) {
			return `${days}d ${hours}h ${minutes}m`;
		} else if (hours > 0) {
			return `${hours}h ${minutes}m`;
		} else {
			return `${minutes}m`;
		}
	}
	
	function formatBytes(bytes: number): string {
		if (bytes < 1024) return `${bytes} B`;
		if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
		return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
	}
	
	function formatTimestamp(ts: number): string {
		return new Date(ts).toLocaleTimeString();
	}
	
	function getHealthColor(score: number): string {
		if (score >= 90) return 'text-green-500';
		if (score >= 80) return 'text-blue-500';
		if (score >= 60) return 'text-yellow-500';
		if (score >= 40) return 'text-orange-500';
		return 'text-red-500';
	}
</script>

<!-- Debug Panel Toggle -->
<div class="fixed bottom-4 right-4 z-50">
	<button 
		class="btn btn-circle btn-sm bg-base-300 hover:bg-base-200"
		on:click={() => isVisible = !isVisible}
		title="System Debug Info"
	>
		🔍
	</button>
</div>

<!-- Debug Panel -->
{#if isVisible}
	<div class="fixed bottom-16 right-4 z-50 bg-base-100 border border-base-300 rounded-lg shadow-lg p-4 w-96 max-h-96 overflow-auto">
		<div class="flex justify-between items-center mb-3">
			<h3 class="font-bold text-sm">Debug Info</h3>
			<button 
				class="btn btn-ghost btn-xs"
				on:click={() => socket.clearDebugLogs()}
			>
				Clear Logs
			</button>
		</div>
		
		<!-- System Health -->
		{#if systemHealth}
			<div class="mb-4">
				<h4 class="font-semibold text-xs mb-2">System Health</h4>
				<div class="grid grid-cols-2 gap-2 text-xs">
					<div>Score: <span class="{getHealthColor(systemHealth.health_score)} font-bold">{systemHealth.health_score}%</span></div>
					<div>Status: {systemHealth.health_status}</div>
					<div>Uptime: {formatUptime(systemHealth.uptime)}</div>
					<div>Tasks: {systemHealth.task_count}</div>
					<div>Heap Free: {formatBytes(systemHealth.heap_free)}</div>
					<div>Heap Usage: {systemHealth.heap_usage_percent.toFixed(1)}%</div>
					<div>WiFi: {systemHealth.wifi_connected ? `${systemHealth.wifi_rssi} dBm` : 'Disconnected'}</div>
					<div>CPU: {systemHealth.cpu_freq_mhz} MHz</div>
					{#if systemHealth.temperature_celsius > 0}
						<div>Temp: {systemHealth.temperature_celsius.toFixed(1)}°C</div>
					{/if}
				</div>
			</div>
		{/if}
		
		<!-- Connection Metrics -->
		<div class="mb-4">
			<h4 class="font-semibold text-xs mb-2">WebSocket Metrics</h4>
			<div class="grid grid-cols-2 gap-2 text-xs">
				<div>Connections: {metrics.totalConnections || 0}</div>
				<div>Disconnects: {metrics.totalDisconnections || 0}</div>
				<div>Duration: {formatDuration(metrics.connectionDuration || 0)}</div>
				<div>Messages: {metrics.messagesReceived || 0}</div>
				<div>Avg Interval: {formatDuration(metrics.avgMessageInterval || 0)}</div>
				<div>Last Message: {metrics.lastMessageAt ? formatDuration(Date.now() - metrics.lastMessageAt) + ' ago' : 'Never'}</div>
			</div>
		</div>
		
		<!-- Disconnect Reasons -->
		{#if metrics.disconnectReasons?.size > 0}
			<div class="mb-4">
				<h4 class="font-semibold text-xs mb-2">Disconnect Reasons</h4>
				<div class="text-xs">
					{#each Array.from(metrics.disconnectReasons.entries()) as [reason, count]}
						<div>{reason}: {count}</div>
					{/each}
				</div>
			</div>
		{/if}
		
		<!-- Recent Debug Logs -->
		<div>
			<h4 class="font-semibold text-xs mb-2">Recent Events</h4>
			<div class="text-xs space-y-1 max-h-32 overflow-y-auto">
				{#each debugLogs.slice(-10).reverse() as log}
					<div class="flex items-start gap-2">
						<span class="text-base-content/50 shrink-0">
							{formatTimestamp(log.timestamp)}
						</span>
						<span class="badge badge-xs" class:badge-error={log.level === 'error'} class:badge-warning={log.level === 'warn'} class:badge-info={log.level === 'info'}>
							{log.level}
						</span>
						<span class="flex-1">{log.event}</span>
					</div>
				{/each}
			</div>
		</div>
	</div>
{/if}