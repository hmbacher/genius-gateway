# Development Insights

This section documents the **non-obvious engineering decisions** behind Genius Gateway - the kind of design rationale that isn't visible from reading the code alone, doesn't belong in user-facing setup docs, and would otherwise live only in commit messages or the author's head.

Each page tackles one constraint of the platform (ESP32 memory, single-bundle frontend, HTTP throughput) and explains how the codebase works around it.

## Pages

### :tabler-cpu-2: [Memory Considerations](memory.md)

How the project fits **50 smoke detectors × 14 Home Assistant entities = 700 sub-entities** into an ESP32-S3 without exhausting internal RAM. Covers the three-layer PSRAM strategy (SDK threshold routing, mbedTLS allocator, explicit `PsramAllocator` on growth collections), the hot-path exception for the CC1101 RX task, observed footprint at the design limit, and what changes when running on a no-PSRAM board.

### :tabler-layers-subtract: [Frontend Bundle Strategy](frontend-bundles.md)

Why the SvelteKit frontend is built as a **single bundle** rather than split per route, and how heavy on-demand dependencies (currently pdfmake, ~1.9 MB) are kept out of that bundle by staging them as static assets and loading them via classic `<script>` tags on demand. The trade-off is concurrency vs. eager-load size - covered in detail with the ESP32-side reasoning.
