── WUBUOFFICE — GOAL MANTRA (v1, 2026-07-29) ──
Path: /home/wubu/WuBuOffice | Tests: 130 (ctest count, doc wrong at 71)
Build: cmake --build build && cd build && ctest -LE 'ocr|slow' -j4

=== STATE ===
✅ VERIFIED: 63 src/ modules, 7 wubuos views, 130 ctest green, ASan clean
✅ VERIFIED: wubuchart/wubudraw/wubushape/wubumath/wubuepub LINKED in CMake
⚠️ BROKEN: GAPS_REAL.md says 71 tests — actual 130 (underrates suite)
⚠️ FRAMING: "MODULE-only" chart gap is WRONG — view_cell.c:202 calls wubucell_chart; gap is SVG→SDL rasterization, not wiring
❌ GAP: HarfBuzz+FriBidi NOT in ui_gfx.c — CJK/RTL blocked
❌ GAP: PDF export menu 1023 says "not yet"
❌ GAP: Spell-check not in Document view

=== PRIORITIES ===
W1 [P0] HarfBuzz/FriBidi shaping into ui_gfx.c (ref: harfbuzz+fribidi)
W2 [P0] PDF export handler in view_doc.c (ref: own wubupdf)
W3-P9 [P1] Document wirings: spell/draw/math/epub/charts-rasterize/transitions/freeze/autosave
W10 [P2] Spreadsheet polish: merge/format/conditional/freeze
W11 [P2] wubuchart SDL rasterize gap (40 lines, not module gap)
W12 [P3] .docx fidelity port from abiword ie_imp.cpp

=== THE LOOP ===
pick gap → locate latent code in src/ → ref-repo trace (Kevin Bacon) →
write handler → headless test → build+ASan → ctest → commit → push
NO fake closures. NO monolith view_doc.c — split new features into doc_X.c modules.
ALL stale docs fixed in same atomic batch.

=== FULL CONTEXT ===
Read .hermes/mind-palace/da_plan_v1.md
Read .hermes/mind-palace/state.md
Read GAPS_REAL.md (correct the 71→130 test count)
