/**
 * Locale-aware date formatting helpers.
 *
 * Prefer the user's browser locale (navigator.language). Falls back to
 * 'en-GB' on the server / non-DOM contexts so dates remain readable.
 */

function getLocale(): string {
	if (typeof navigator !== 'undefined' && navigator.language) return navigator.language;
	return 'en-GB';
}

const DATE_OPTS: Intl.DateTimeFormatOptions = {
	day: '2-digit',
	month: '2-digit',
	year: 'numeric'
};

const DATE_TIME_OPTS: Intl.DateTimeFormatOptions = {
	day: '2-digit',
	month: '2-digit',
	year: 'numeric',
	hour: '2-digit',
	minute: '2-digit'
};

const DATE_TIME_SECONDS_OPTS: Intl.DateTimeFormatOptions = {
	day: '2-digit',
	month: '2-digit',
	year: 'numeric',
	hour: '2-digit',
	minute: '2-digit',
	second: '2-digit'
};

/** "20/11/2024" (browser locale). Returns "-" for null/undefined. */
export function formatDate(date?: Date | null): string {
	if (!date) return '-';
	return date.toLocaleDateString(getLocale(), DATE_OPTS);
}

/** "20/11/2024, 14:35" (browser locale). Returns "-" for null/undefined. */
export function formatDateTime(date?: Date | null): string {
	if (!date) return '-';
	return date.toLocaleString(getLocale(), DATE_TIME_OPTS);
}

/** "20/11/2024, 14:35:12" (browser locale). Returns "-" for null/undefined. */
export function formatDateTimeSeconds(date?: Date | null): string {
	if (!date) return '-';
	return date.toLocaleString(getLocale(), DATE_TIME_SECONDS_OPTS);
}

/**
 * "just now", "3 min ago", "5 h ago", "2 d ago" - coarse relative time for status displays.
 * `nowMs` is passed in so callers can re-render on a clock tick.
 */
export function formatAgo(date?: Date | null, nowMs: number = Date.now()): string {
	if (!date) return '-';
	const s = Math.max(0, Math.floor((nowMs - date.getTime()) / 1000));
	if (s < 60) return 'just now';
	const min = Math.floor(s / 60);
	if (min < 60) return `${min} min ago`;
	const h = Math.floor(min / 60);
	if (h < 48) return `${h} h ago`;
	return `${Math.floor(h / 24)} d ago`;
}

/**
 * "5y 32d" or "147d" - coarse human age, locale-independent.
 * Uses calendar-year boundaries (anniversary in the current year) rather than
 * dividing by 365.25, so "1y 0d" lines up with the actual anniversary.
 */
export function formatAge(date?: Date | null): string {
	if (!date) return '-';
	const now = new Date();
	let years = now.getFullYear() - date.getFullYear();
	const anniversaryThisYear = new Date(date);
	anniversaryThisYear.setFullYear(now.getFullYear());
	if (anniversaryThisYear > now) years--;
	if (years < 1) {
		const totalDays = Math.floor((now.getTime() - date.getTime()) / 86400000);
		return `${totalDays}d`;
	}
	const lastAnniversary = new Date(date);
	lastAnniversary.setFullYear(date.getFullYear() + years);
	const remainingDays = Math.floor((now.getTime() - lastAnniversary.getTime()) / 86400000);
	return `${years}y ${remainingDays}d`;
}
