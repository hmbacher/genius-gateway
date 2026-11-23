/**
 * Wording for a Home Assistant entity-ID rename: who it is about.
 *
 * The gateway names the first detectors of a batch by their location and gives the size of
 * the batch; the rest is summed up. They are shown as a list below a fixed introduction, not
 * woven into the sentence.
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

/** `1 smoke detector` / `n smoke detectors` */
export function detectorCount(count: number): string {
	return count === 1 ? '1 smoke detector' : `${count} smoke detectors`;
}

/**
 * The detectors of a rename batch as list items: the locations the gateway named, and - if the
 * batch is larger - a last item that sums up the rest (`and 8 more`). Empty if the gateway
 * named none; the caller then falls back to the plain count.
 *
 * @param count number of detectors in the batch
 * @param names locations of the first detectors, as far as the gateway named them
 */
export function renamedDetectorItems(count: number, names: string[] = []): string[] {
	const shown = names.filter((name) => name.trim() !== '');
	if (shown.length === 0) return [];
	const more = count - shown.length;
	return more > 0 ? [...shown, `and ${more} more`] : shown;
}
