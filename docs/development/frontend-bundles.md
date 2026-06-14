---
icon: tabler/layers-subtract
---

# Frontend Bundle Strategy

The Genius Gateway SvelteKit frontend is built and served by the ESP32 itself: every JS, CSS, font, and image file lives in PROGMEM, registered by [`scripts/build_interface.py`](https://github.com/hmbacher/genius-gateway/blob/main/scripts/build_interface.py) and served by the embedded HTTP server. This page explains why the frontend is bundled as **one single JavaScript file** instead of using SvelteKit's default per-route code splitting, and how a heavy on-demand dependency (currently pdfmake, the client-side PDF generator) is kept out of that single bundle without breaking the single-bundle invariant.

!!! tip "TL;DR"
    One eager bundle for the SPA + N optional external assets fetched on demand. The SvelteKit option [`kit.output.bundleStrategy = 'single'`](https://github.com/hmbacher/genius-gateway/blob/main/interface/svelte.config.js) enforces the first part; an [`interface/scripts/stage-pdf-assets.mjs`](https://github.com/hmbacher/genius-gateway/blob/main/interface/scripts/stage-pdf-assets.mjs) prebuild step plus `<script>`-tag loading in [`pdfReport.ts`](https://github.com/hmbacher/genius-gateway/blob/main/interface/src/routes/gateway/smoke-detectors/pdfReport.ts) implements the second.

## :tabler-cpu: Why a single bundle

A typical Vite/Rollup build splits the SPA into many small chunks: one per route, per shared component group, per dynamically imported module. On a normal CDN this is the right thing — the browser opens 6+ parallel HTTP/2 streams, caches per-chunk hashes, and only refetches what changed.

The ESP32 is **not a normal CDN**.

| Constraint | Impact |
| --- | --- |
| Single-threaded HTTP server (PsychicHTTP / esp-http-server) with a small concurrent-connection limit (≈ 4–7) | The browser's parallel chunk requests serialize on the server side. Every "extra" chunk costs at least one round-trip-time plus server-handler overhead. |
| TLS handshake amortization | If the server runs HTTPS, each new connection means another expensive handshake on a CPU that takes a noticeable fraction of a second to do one. |
| Wi-Fi throughput (~100–300 KB/s typical for ESP32-S3 on STA mode) | One sequential 250 KB transfer is faster than ten 25 KB transfers because per-request overhead dominates. |
| Single physical user | There is no cache-efficiency argument from "different users hit different routes" — there is one user, and they will navigate the whole SPA. |

Empirically, the default chunk-splitting strategy made initial page load painful: the browser issued dozens of requests, the server queued them behind the few it could handle concurrently, and total load time climbed into the multi-second range purely from request overhead — even when the total bytes transferred were modest.

[`interface/svelte.config.js`](https://github.com/hmbacher/genius-gateway/blob/main/interface/svelte.config.js) therefore opts into SvelteKit's single-bundle strategy:

```js
kit: {
    output: {
        bundleStrategy: 'single'
    }
}
```

Everything reachable from the SPA entry point gets concatenated into one `build/_app/immutable/bundle.<hash>.js`. The page loads in **two requests** (HTML + bundle), the file is gzipped for transfer and held in PROGMEM in its gzipped form, and the browser only needs one HTTP transaction to get the entire app running.

## :tabler-weight: Why this needs a workaround for heavy libraries

The single-bundle strategy has one critical downside: **`import()` is no longer code-splitting**. Rollup respects the strategy by inlining every module the dependency graph touches, including those behind `await import('./lazy.ts')` calls. The dynamic-import syntax still compiles, but at runtime it just resolves a Promise to an already-loaded module.

That is invisible until a transitively imported library is heavy. The smoke-detector PDF report uses [pdfmake](https://pdfmake.github.io), a high-level PDF generator with native SVG support, table layout, font management — all the things that make it the right library for this report (a printable audit document with branded logo, per-device pages, page numbers). It is also, including the Roboto font VFS that ships separately, **roughly 1.9 MB of JavaScript**:

| Asset | Raw size | Gzipped |
| --- | --- | --- |
| `pdfmake.min.js` | ~1.05 MB | ~330 KB |
| `vfs_fonts.js` (Roboto regular/bold/italic/bold-italic) | ~854 KB | ~500 KB |

The original implementation used `await import('pdfmake/build/pdfmake')` in a dynamically imported file, intending to keep pdfmake out of the initial page load. Under the single-bundle strategy this had no effect: pdfmake was concatenated into the same `bundle.<hash>.js` as the SPA, inflating it from ~800 KB to ~2.7 MB raw (~250 KB → ~830 KB gzipped). Initial load on the ESP32 grew to **about 20 seconds** — unacceptable for a page the user visits every time they open the gateway, when the PDF is generated maybe once a month.

## :tabler-puzzle: The single-bundle + external-asset pattern

The fix preserves the single-bundle invariant for the SPA itself, and treats heavy on-demand libraries as **static assets fetched at runtime via classic `<script>` tags**. Three pieces collaborate:

### 1. Prebuild staging from `node_modules`

[`interface/scripts/stage-pdf-assets.mjs`](https://github.com/hmbacher/genius-gateway/blob/main/interface/scripts/stage-pdf-assets.mjs) runs before every Vite build via npm's `prebuild` lifecycle hook:

```json
"scripts": {
    "prebuild": "node scripts/stage-pdf-assets.mjs",
    "build": "vite build"
}
```

It copies `node_modules/pdfmake/build/pdfmake.min.js` and `vfs_fonts.js` into `interface/static/pdf/`. SvelteKit's `adapter-static` copies the `static/` directory into the build output verbatim — these two files therefore appear in `build/pdf/` as plain files, not as ESM chunks, and SvelteKit never tries to bundle them.

`interface/static/pdf/` is gitignored. The staged files are regenerated on every build from the pdfmake version pinned in `package.json`, so `npm update pdfmake` is the only step needed to upgrade the embedded library.

### 2. PROGMEM embedding inherits

[`scripts/build_interface.py:build_progmem()`](https://github.com/hmbacher/genius-gateway/blob/main/scripts/build_interface.py) walks `build/` with `rglob("*.*")` and registers every file it finds as an HTTP route with the correct MIME type. The two new files at `build/pdf/pdfmake.min.js` and `build/pdf/vfs_fonts.js` are picked up automatically; no firmware change is required.

### 3. Runtime loading via `<script>` tag

[`pdfReport.ts`](https://github.com/hmbacher/genius-gateway/blob/main/interface/src/routes/gateway/smoke-detectors/pdfReport.ts) replaces the dynamic `import()` calls with a cached `<script>`-tag loader:

```ts
let pdfMakeReady: Promise<PdfMakeGlobal> | null = null;

function loadScript(src: string): Promise<void> { /* … */ }

async function loadPdfMake(): Promise<PdfMakeGlobal> {
    if (pdfMakeReady) return pdfMakeReady;
    pdfMakeReady = (async () => {
        await loadScript('/pdf/pdfmake.min.js');
        await loadScript('/pdf/vfs_fonts.js');
        return (window as any).pdfMake;
    })();
    return pdfMakeReady;
}
```

`pdfmake.min.js` is a classic IIFE that assigns `window.pdfMake`. `vfs_fonts.js` detects an already-loaded `window.pdfMake` and self-attaches its Roboto VFS via `pdfMake.addVirtualFileSystem(vfs)` — no manual wiring required on our side.

The cached Promise ensures that clicking the PDF button a second time within the same session is instant: the scripts are already parsed and the global is still on `window`.

## :tabler-rocket: Result

| Metric | Before (pdfmake inlined) | After (pdfmake external) |
| --- | --- | --- |
| `bundle.<hash>.js` raw | ~2.7 MB | ~800 KB |
| `bundle.<hash>.js` gzipped | ~830 KB | ~250 KB |
| Initial page load on ESP32 (Wi-Fi STA, ~200 KB/s) | ~20 s | ~2–3 s |
| First PDF generation (cold) | ~0 extra (already loaded) | ~5–10 s (fetches pdfmake + VFS) |
| Subsequent PDF generations | Instant | Instant (cached promise) |

The cost moves from "every page load pays for pdfmake" to "the rare PDF action pays for pdfmake." For a feature used a few times a month on a page the user visits dozens of times a day, that is the right trade.

## :tabler-list-check: Adding another heavy library later

The same pattern applies any time you reach for a > 100 KB library that only matters for a single user action (think: chart library used only on one diagnostics page, a barcode scanner, a heavy validator). The recipe is:

1. **Install it as a normal `dependencies` entry** so `package.json` pins the version and `npm install` puts it in `node_modules`.
2. **Extend [`stage-pdf-assets.mjs`](https://github.com/hmbacher/genius-gateway/blob/main/interface/scripts/stage-pdf-assets.mjs)** (or write a new sibling script) to copy its UMD/IIFE build artifact into `interface/static/<libname>/`.
3. **Add the new staging path to `.gitignore`**.
4. **Load it on demand** with the same cached-`<script>` pattern. If the library exposes a global, mirror `loadPdfMake`; if it uses an alternative initialization, adapt the wrapper.
5. **Keep the ESM import for type definitions only.** TypeScript will type-check against `node_modules` even though no actual `import` reaches Vite — write the runtime path as `(window as any).<global>` to make this explicit.

Avoid this pattern for small libraries (< 50 KB raw): they don't move the needle on the eager bundle, and the extra HTTP request on first use is wasted overhead. The break-even point on the ESP32 sits somewhere around 100–200 KB raw; below that, just let it stay in the single bundle.

## :tabler-stethoscope: Diagnosing bundle size regressions

The single-bundle strategy makes regressions silent — any accidental top-level import of a heavy library lands in `bundle.<hash>.js` without warning. Two cheap checks:

- After a build, `ls -la interface/build/_app/immutable/*.js`. If the main bundle grows by hundreds of KB unexpectedly, something heavy slipped in.
- `interface/build/pdf/` should always contain exactly `pdfmake.min.js` and `vfs_fonts.js`. If staging is broken the directory will be empty, the PDF button will produce a network error on click, and the prebuild step output will say so.

If the bundle is mysteriously large but pdfmake is staged correctly, look for static imports of `pdfmake` or another `node_modules` package — the rule of thumb is that **anything heavier than ~100 KB raw should not appear in the SvelteKit dependency graph at all**; it should be staged like pdfmake and loaded at runtime.
