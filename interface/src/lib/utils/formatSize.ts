/** "640 B", "12.4 kB", "3.2 MB" - data amounts in decimal units, one decimal from kB on. */
export function formatBytes(bytes: number): string {
	if (!Number.isFinite(bytes) || bytes < 0) return '-';
	if (bytes < 1000) return `${bytes} B`;
	const units = ['kB', 'MB', 'GB'];
	let value = bytes / 1000;
	let unit = 0;
	while (value >= 1000 && unit < units.length - 1) {
		value /= 1000;
		unit++;
	}
	return `${value.toFixed(1)} ${units[unit]}`;
}

/** "45 s", "12 min 4 s", "3 h 12 min", "2 d 5 h" - an elapsed time, the two largest units. */
export function formatDuration(ms: number): string {
	if (!Number.isFinite(ms) || ms < 0) return '-';
	const s = Math.floor(ms / 1000);
	if (s < 60) return `${s} s`;
	const min = Math.floor(s / 60);
	if (min < 60) return `${min} min ${s % 60} s`;
	const h = Math.floor(min / 60);
	if (h < 24) return `${h} h ${min % 60} min`;
	return `${Math.floor(h / 24)} d ${h % 24} h`;
}
