#!/usr/bin/env node
/**
 * Stage pdfmake's build artifacts as static assets so they are served as
 * separate files instead of being inlined into the SvelteKit single bundle.
 *
 * Source:      node_modules/pdfmake/build/{pdfmake.min.js,vfs_fonts.js}
 * Destination: static/pdf/{pdfmake.min.js,vfs_fonts.js}
 *
 * Wired in via the `prebuild` npm script. `static/pdf/` is gitignored - the
 * files are produced from the pdfmake dependency on every build so upgrades
 * land automatically with `npm update pdfmake`.
 *
 * See docs/development/frontend-bundles.md for the rationale.
 */

import { copyFileSync, mkdirSync, existsSync, statSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __dir = dirname(fileURLToPath(import.meta.url));
const SRC_DIR = join(__dir, '..', 'node_modules', 'pdfmake', 'build');
const DEST_DIR = join(__dir, '..', 'static', 'pdf');

const FILES = ['pdfmake.min.js', 'vfs_fonts.js'];

if (!existsSync(SRC_DIR)) {
	console.error(
		`[stage-pdf-assets] pdfmake build dir not found at ${SRC_DIR} - run npm install first.`
	);
	process.exit(1);
}

mkdirSync(DEST_DIR, { recursive: true });

for (const file of FILES) {
	const src = join(SRC_DIR, file);
	const dst = join(DEST_DIR, file);
	if (!existsSync(src)) {
		console.error(`[stage-pdf-assets] missing source ${src}`);
		process.exit(1);
	}
	// Skip if destination is at least as fresh as the source - pdfmake's
	// build artifacts only change on dependency upgrade, so the same files
	// would otherwise be re-copied on every incremental rebuild.
	if (existsSync(dst) && statSync(dst).mtimeMs >= statSync(src).mtimeMs) {
		continue;
	}
	copyFileSync(src, dst);
	console.log(`[stage-pdf-assets] staged ${file}`);
}
