/**
 * Names of the configured alarm lines (id → name), used to title the groups of smoke detectors.
 * Read-only mirror of `GET /rest/alarm-lines`; stays empty if the request fails (the groups then show
 * "Line <hex id>").
 */
export function createAlarmLineNames() {
	let names: Record<number, string> = $state({});

	return {
		get names() {
			return names;
		},
		async load(headers: Record<string, string>) {
			try {
				const response = await fetch('/rest/alarm-lines', { method: 'GET', headers });
				if (response.status !== 200) return;
				const body = (await response.json()) as { lines?: { id: number; name: string }[] };
				const next: Record<number, string> = {};
				for (const line of body.lines ?? []) next[line.id >>> 0] = line.name;
				names = next;
			} catch (error) {
				console.error('Failed to load alarm line names:', error);
			}
		}
	};
}

export const alarmLineNames = createAlarmLineNames();
