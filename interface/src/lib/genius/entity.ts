/**
 * Shared helpers for resolving Genius packet entities (radio modules, smoke detectors)
 * to display labels and their packet-visualizer styling variant.
 */

/** Display label for the Genius Gateway's own radio-module ID (0xFFFFFFFE). */
export const GATEWAY_NAME = 'Genius Gateway';

/** Display label used when a serial number / line ID cannot be resolved to a known entity. */
export const UNKNOWN_NAME = 'Unknown';

/**
 * Suffix appended to a packet-field `data-type` (byte blocks) or chip base class
 * (summary row) based on the resolved entity label:
 * - `-gateway` for the Genius Gateway itself,
 * - `-unknown` for an unresolved serial number / line,
 * - `''` for a known device / line.
 */
export function entityStyleSuffix(label: string | undefined): string {
	if (label === GATEWAY_NAME) return '-gateway';
	if (label === UNKNOWN_NAME) return '-unknown';
	return '';
}
