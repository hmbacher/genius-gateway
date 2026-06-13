const IPV4_RE =
	/^(?:(?:2(?:[0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9])\.){3}(?:2(?:[0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9])$/;
// Anchored FQDN: each label is 1–63 chars (letters, digits, internal hyphens), TLD is letters-only.
const HOSTNAME_RE = /^([a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$/;

export function isIPv4(v: string): boolean {
	return IPV4_RE.test(v);
}

export function isHostnameOrIP(v: string): boolean {
	return isIPv4(v) || HOSTNAME_RE.test(v);
}

/** Valid MQTT publish topic path (no wildcards). maxLen defaults to 128. */
export function isMQTTTopicPath(v: string, maxLen = 128): boolean {
	if (!v || typeof v !== 'string') return false;
	if (v.length < 1 || v.length > maxLen) return false;
	if (!/^[a-zA-Z0-9\-_.\/]+$/.test(v)) return false;
	if (v.startsWith('/') || v.endsWith('/') || v.includes('//')) return false;
	return v.split('/').every((level) => level.length > 0 && level.trim().length > 0);
}

/** Valid Home Assistant MQTT discovery prefix (may end with /). */
export function isDiscoveryPrefix(v: string): boolean {
	if (!v || v.length > 64) return false;
	if (v.includes('//')) return false;
	const path = v.endsWith('/') ? v.slice(0, -1) : v;
	return path.length > 0 && /^[a-zA-Z0-9\-_.\/]+$/.test(path) && !path.startsWith('/');
}

export function inRange(n: number, min: number, max: number): boolean {
	return n >= min && n <= max;
}

export function hasLength(s: string, min: number, max: number): boolean {
	return s.length >= min && s.length <= max;
}
