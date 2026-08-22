# WuBuOffice — STATE (v1, 2026-07-29)

## Purpose
Live truth table for WuBuOffice build/test status and gap verification.

## Key Numbers (VERIFIED BY EXECUTION)
- src/ modules: 63
- apps/wubuos/ views: 7 (doc/cell/slide/ocr/compare/editor/settings)
- ctest count: 130 (NOT 71 — GAPS_REAL.md is stale)
- word_doc.c: 111 lines (YES, real but minimal)
- view_doc.c: 702 lines
- view_cell.c: 243 lines, line 202 calls `wubucell_chart(...)` — chart IS wired
- cell_model.c: 265 lines
- view_slide.c: 93 lines
- wubuchart/chart.c: 282 lines, outputs SVG via chart_render_svg()
- wubushape exists but NOT referenced in ui_gfx.c (CJK/RTL gap confirmed)
- wubudraw included in view_doc.c (#include "draw.h") but no WUOS_KEY_INSERT_SHAPE handler

## Build
- `cmake -S . -B build` + `cmake --build build`
- ASan: `cmake -S . -B build-asan -DCMAKE_C_FLAGS='-fsanitize=address,undefined -g'` then build+run
- Fast: `ctest -LE 'ocr|slow' -j4`

## Ref Repository Tracings
- ref/major/harfbuzz (132MB) — hb-ft C API for shaping
- ref/major/fribidi (20MB) — UAX#9 BIDI
- ref/major/abiword (69MB) — full .docx/.odt/.rtf import path
- ref/major/gnumeric (172MB) — cell merge, conditional format
- ref/major/calligra (385MB) — full office suite architecture ref
- ref/lesser-known/mujs (912KB) — JS engine for embedding
- ref/lesser-known/fredbuf — piece-tree alternative data structure

## Known Stale Doc Claims (need fix)
| Claim | Reality | Fix |
|---|---|---|
| 71/71 tests green | 130 tests | Update GAPS_REAL.md |
| wubuchart MODULE-only | view_cell.c:202 calls it; gap is rasterize, not wiring | Re-frame gap #6 |
| "40+ modules not wired to GUI" | 14 linked in wubuos CMake; rest are test-only / not-linked / not-needed | Refine per-module |
