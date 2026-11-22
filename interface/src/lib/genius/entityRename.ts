/**
 * Wording for a Home Assistant entity-ID rename: who it is about.
 *
 * The gateway names the first detectors of a batch by their location and gives the size of
 * the batch; the rest is summed up.
 */

/** Escape a text for use inside an HTML message (locations are entered by the user). */
export function escapeHtml(text: string): string {
	return text
		.replace(/&/g, '&amp;')
		.replace(/</g, '&lt;')
		.replace(/>/g, '&gt;')
		.replace(/"/g, '&quot;')
		.replace(/'/g, '&#39;');
}

/**
 * The detectors of a rename batch as a phrase, e.g. `"Kitchen"`, `"Kitchen" and "Hall"` or
 * `"Kitchen", "Hall", "Attic" and 8 more smoke detectors`. Without names: the plain count.
 *
 * @param count number of detectors in the batch
 * @param names locations of the first detectors, as far as the gateway named them
 * @param format formats one name, e.g. to escape and emphasise it for an HTML message
 */
export function renamedDetectors(
	count: number,
	names: string[] = [],
	format: (name: string) => string = (name) => `"${name}"`
): string {
	const shown = names.filter((name) => name.trim() !== '').map(format);
	if (shown.length === 0) return count === 1 ? '1 smoke detector' : `${count} smoke detectors`;

	const more = count - shown.length;
	if (more > 0) {
		return `${shown.join(', ')} and ${more} more smoke detector${more === 1 ? '' : 's'}`;
	}
	if (shown.length === 1) return shown[0];
	return `${shown.slice(0, -1).join(', ')} and ${shown[shown.length - 1]}`;
}
