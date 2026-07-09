/**
 * Reactive per-field dirty tracking for settings forms.
 *
 * Replaces the per-form `JSON.stringify(current) !== snapshot` pattern with a small
 * reusable helper that also answers *which* field changed, so individual inputs can show
 * a dirty marker and offer a per-field revert - while the enclosing SettingsCard still
 * gets a single rolled-up `anyDirty` flag.
 *
 * Usage:
 *   const f = createDirtyState({ ...defaultSettings });
 *   // bind:  bind:value={f.current.username}
 *   // field: f.isDirty('username')          -> marker / f.fieldClass('username') -> input border
 *   // card:  f.anyDirty                      -> <SettingsCard isDirty>
 *   // load:  f.reset(loadedSettings)         (after GET)
 *   // save:  f.commit()                      (after a successful POST)
 *   // undo:  f.revert('username')            (per field) / f.revertAll()
 *
 * No app-specific dependencies - portable to the ESP32-SvelteKit base interface.
 */

function clone<T>(v: T): T {
	// JSON round-trip: detaches from Svelte's deeply-reactive $state proxy (which structuredClone
	// rejects with DataCloneError) and stays consistent with the JSON-based equality below.
	// Form values are JSON-serializable by construction (they round-trip over REST).
	return JSON.parse(JSON.stringify(v));
}

/**
 * Structural equality, adequate for form value shapes (primitives and nested plain
 * objects/arrays). Both sides share the same shape, so serialized key order matches.
 */
function equal(a: unknown, b: unknown): boolean {
	return JSON.stringify(a) === JSON.stringify(b);
}

export interface DirtyState<T extends object> {
	/** The live, bindable form object. Bind inputs to `current.<key>`. */
	readonly current: T;
	/** The committed baseline (last loaded or saved values). Read-only snapshot. */
	readonly baseline: T;
	/** True when the given field differs from the committed baseline. */
	isDirty(key: keyof T): boolean;
	/** True when any field differs from the baseline (feeds SettingsCard `isDirty`). */
	readonly anyDirty: boolean;
	/** Left-accent border class for an input whose field is dirty (empty otherwise). */
	fieldClass(key: keyof T): string;
	/** Reset a single field back to its baseline value. */
	revert(key: keyof T): void;
	/** Reset every field back to the baseline. */
	revertAll(): void;
	/** Adopt the current values as the new baseline (call after a successful save). */
	commit(): void;
	/** Replace baseline *and* current with freshly loaded values (call after a load). */
	reset(next: T): void;
}

export function createDirtyState<T extends object>(initial: T): DirtyState<T> {
	let current = $state<T>(clone(initial));
	let baseline = $state<T>(clone(initial));

	const dirty = (key: keyof T) => !equal(current[key], baseline[key]);

	return {
		get current() {
			return current;
		},
		get baseline() {
			return baseline;
		},
		isDirty: dirty,
		get anyDirty() {
			return (Object.keys(baseline) as (keyof T)[]).some(dirty);
		},
		fieldClass(key) {
			// Red left accent, matching the SettingsCard dirty bar (bg-red-300). Drawn as an inset
			// box-shadow rather than a left border so it does NOT shift the field's content (the
			// SettingsCard bar is an overlay, not a border). Distinct from the full red
			// `border-error` box used for invalid fields.
			return dirty(key) ? 'shadow-[inset_4px_0_0_0_var(--color-red-300)]' : '';
		},
		revert(key) {
			current[key] = clone(baseline[key]);
		},
		revertAll() {
			current = clone(baseline);
		},
		commit() {
			baseline = clone(current);
		},
		reset(next) {
			current = clone(next);
			baseline = clone(next);
		}
	};
}
