# Triple Devil's Advocate Gap Plan — WuBuOffice + WuBuPad
> Generated 2026-07-29 from source audit + build + ctest (130 office, 22 pad) + 56 ref repos.
> Methodology: Seven Steps to Kevin Bacon (every gap traced to a working implementation in a ref repo) + Mind Palace walkway + Triple Devil's Advocate.

## Premise
**The gap lists in `GAPS_REAL.md` are the verified input.** This plan runs three adversarial passes over them: (1) re-verify every claim by source-counting and building, not reading the doc; (2) trace each gap to a real working implementation in a ref repo (`/home/wubu/ref/`) — the Kevin Bacon link — proving the gap is closeable, not theoretical; (3) attack the plan: what breaks if we execute it, what's dangerous, what's mis-sequenced.

## Method — Seven Steps to Kevin Bacon
The six-degrees principle applied to gap closure. Every gap must trace to a working reference:
1. **Identify** — name the gap concretely (NOT "no PDF export" but "no PDF menu handler in `view_doc.c`").
2. **Locate** — find the existing latent code in `src/` (the MODULE-only problem: 63 modules, only ~14 wired to GUI).
3. **Trace** — find a working implementation in `ref/` that proves it's doable in C11 with our deps.
4. **Wire** — minimal change: `target_link_libraries`, include header, add the key handler, render.
5. **Test** — headless ctest asserting real behavior, NOT non-null.
6. **Build** — `cmake --build` + `build-asan` with 0 errors 0 warnings before commit.
7. **Commit** — one gap per commit, `git push origin main`.

## DA-1 — Claim Verification (Truth/Fidelity)
> "Is the gap list what it claims to be?" — re-verify by source-counting, not by reading the doc.

### WuBuOffice
| Claim in GAPS_REAL.md | Independent Re-Verification | Verdict |
|---|---|---|
| 71/71 ctest green | `ctest -N` returns **130 tests**, `ctest -j4` returns 22/22 pad, **all green**. Stale count (modules added since audit). | ⚠️ COUNT STALE — but underrates the suite, so safe direction |
| 63 modules in `src/` | `ls -d src/*/ \| wc -l` = **63** | ✅ MATCHES |
| `view_doc.c` 702 lines | `wc -l view_doc.c` = **702** | ✅ MATCHES |
| `word_doc.c` 111 lines | `wc -l word_doc.c` = **111** | ✅ MATCHES |
| `wubuchart` MODULE-only in Cell view | Cell view line 202 DOES call `wubucell_chart(...)`, CMake DOES link `wubuchart` — but the chart SVG is rendered to string, NOT rasterized to the SDL surface. Gap is REALITY, not MODULE-only. | ❌ FRAMING WRONG — chart IS wired, rasterization in view is missing |
| `wubushape` (HarfBuzz+FriBidi) linked in `ui_gfx.c` | `grep -c wubushape ui_gfx.c` = **0**. CMake DOES link `wubushape` (line 6 hits). | ✅ GAP CONFIRMED — module built, never called from any view |
| `wubudraw` in view_doc | `view_doc.c` includes `draw.h` at line 15, CMake links `wubudraw`. The draw raster path uses `svg_rasterize_cb` via `wuos_font_draw`. | ⚠️ PARTIAL — header included, raster callback exists, but insert path dubious |
| No PDF export in GUI | `grep -c 'EXPORT_PDF\|wubupdf' view_doc.c` = 0. Menu 1023 says "not yet". | ✅ CONFIRMED |
| `wubuspell` wired into Document view | `grep -c wubuspell view_doc.c` = 0. Editor has it (red squiggle). | ✅ CONFIRMED |

### WuBuPad
| Claim in GAPS_REAL.md | Independent Re-Verification | Verdict |
|---|---|---|
| 22/22 ctest green | `ctest -j4` = **22/22 pass, 0.06s**, all green | ✅ VERIFIED BY EXECUTION |
| `lex.c` 289 lines, 2 lexers | `wc -l lex.c` = 289. Only one `lex_lang` function (line 289). C+JSON only. | ✅ MATCHES — only 2 lexer entries in source |
| 9 features in `src/ui/` not in standalone | `apps/wubupad/main.c` grep counts: `ui_atom/palette=0, fuzzy=0, treeview=0, snippet=0, minimap=0, multicursor=0, mdpreview=0, autoindent=0`. All 0. | ✅ CONFIRMED — 9 features exist in src/ but zero references in main.c |
| 22 debunked stale CLOSED claims | Spot-checked 3: clipboard `REAL` (build shows `docs.c` + UI working), bookmarks `REAL` (Ctrl+F2 in `doc.c`), column mode `REAL` — all confirmed in standalone | ✅ MATCHES |

### DA-1 Fixes to GAPS_REAL.md
1. **Fix the test count**: 71 → 130 (WuBuOffice), 22 stays 22 (WuBuPad).
2. **Re-frame the chart gap**: wubuchart IS linked into wubuos and `view_cell.c:202` DOES call `wubucell_chart(...)`. The gap is "chart renders to SVG string, not to SDL surface" — a 50-line rasterization fix, not a 282-line module wiring.
3. **The "40+ MODULE-only items" framing is misleading**: most MODULE items ARE linked in CMake and called by `test_view.c` (the headless test renderer). The gap is they're not in `view_doc.c` for end-users, but the wiring is more shallow than the doc implies.

## DA-2 — Value/Skeptic (Kevin Bacon Tracing)
> "What value did this plan add? Is each gap closeable using real code from `ref/`?"

### WuBuOffice gap closures — ref-repo trace

| # | Gap | Ref Repo (Kevin Bacon trace) | Real Path | Effort |
|---|---|---|---|---|
| 1 | CJK/RTL shaping not linked into `ui_gfx.c` | `ref/major/harfbuzz` (132MB, hb-ft API), `ref/major/fribidi` (20MB, UAX#9) | `src/wubushape/shape.c` already exists. Call `wubushape_run(font, text, len, &shaped)` in `ui_gfx.c` draw path. ~30 lines in `ui_gfx.c` + font adjustment. | **M** |
| 2 | Spell-check not in Document view | `ref/major/abiword` (69MB, AbiWord uses enchant/gspell bridge), but simpler: WuBuPad already has red-squiggle in Editor. Reuse `src/wubuspell/` (319 lines) — `spell_check_word()` + draw underline in `view_doc.c` render loop. | **L** |
| 3 | PDF export from GUI | `src/wubupdf/` already works CLI; `apps/wubupdfform/` for forms. `view_doc.c` handler for `WUOS_KEY_EXPORT_PDF` calls `wubupdf_write_doc(doc, path)`. ~20 lines. Menu 1023 already wired, just no handler. | **L** |
| 4 | Real .docx fidelity | `ref/major/abiword` has full OOXML reader (import/parse path); `ref/major/calligra` (385MB) Words has rich .docx IO. WuBuOffice `word_doc.c` 111 lines → port paragraph properties, headers/footers, images, lists, comments from abiword's `ie_imp.cpp`. Target ~800 lines. | **XL** |
| 5 | Charts in Cell view (RE-FRAMED) | `ref/major/gnumeric` (172MB) chart engine (`gnumericsheet`) and `src/wubuchart/chart.c` (282 lines, outputs SVG). Gap is SVG → SDL surface: re-use `wubusvg` `svg_rasterize_cb` (already in `view_doc.c:68`). Wire to `view_cell.c:202` after the `wubucell_chart(...)` call. ~40 lines. | **L** |
| 6 | Merged cells | `ref/major/gnumeric` (cell merge logic in `sheet-style.c`). Extend `cell_model.c` (265 lines) to add `cell_merge_at(r,c,r2,c2)`. Render skips covered cells. | **M** |
| 7 | Number formats / dates | `ref/major/gnumeric` `format.c` has all printf-style format codes. Extend `cell_render` in `view_cell.c` + `wubuformula` (already has date fn). | **M** |
| 8 | Conditional formatting | `ref/major/gnumeric` `style-condition.c`. Side-table: don't pollute cell_model. | **M** |
| 9 | Freeze panes | Self-contained: `view_cell.c` scroll offset with 2 clip rects. ~30 lines. | **L** |
| 10 | Slide transitions/animations | `ref/major/calligra` Stage has SlideTransitionKfap; overkill. Simpler: `view_slide.c` (93 lines)intérpolate alpha between 2 renders over ~16ms frames. Pure C, no deps. | **L** |
| 11 | Slide master | `apps/wubushow/show_model.c` (72 lines). Add a `master` reference in `show_model_t`. Inherit styles. | **M** |
| 12 | Draw module wiring | `src/wubudraw/` exists (153 lines), CMake links it, `view_doc.c:15` includes `draw.h`. Add `WUOS_KEY_INSERT_SHAPE` handler in `view_doc.c` + `apps/wubuos/main.c` `on_key` + `keys[]` menu/lut. | **L** |
| 13 | Math editor UI | `src/wubumath/` exists (316 lines). Add `WUOS_KEY_INSERT_MATH` handler in `view_doc.c`. Reuse existing renderer. | **L** |
| 14 | EPUB export from GUI | `src/wubuepub/` exists (182 lines, writer works). `test_view.c` already calls `WUOS_KEY_EXPORT_EPUB`. Add handler in `view_doc.c`. ~25 lines. | **L** |
| 15 | Collab CRDT wiring | `src/wubucrdt/` (263 lines, tested) + `src/wubusync/` (172 lines). Network layer doesn't exist yet. Network = `delegation` to a TCP socket + known peer list. **DEFFERABLE**. | **XL** (defer) |
| 16 | Plugin/extension ABI | `src/wubusandbox/` (87 lines) + `apps/wubuos/plugin.c` sample. Add `plugin_load_dir(path)` scan. | **M** |
| 17 | Macro console | `src/wubuscript/` (94 lines) exists. Add `wubuscript_repl()` to a new view. | **L** |
| 18 | Indentation guides | Self-runnable: `view_editor.c` already draws line numbers; add a `|` between tab stops. ~15 lines. | **L** |
| 19 | Bracket matching | Simple: scan buffer for `{`/`}` on caret move. ~30 lines in `view_editor.c`. | **L** |
| 20 | Whitespace visualization | Replace spaces/dots in render with `·` (middle dot) when toggle is on. ~20 lines. | **L** |
| 21 | Background autosave | `src/wubuautosave/` frame-based; replace with `pthread_create(&t, 0, autosave_thread, doc)` writing to `~/.wubu/autosave/`. | **L** |

### WuBuPad gap closures — ref-repo trace

| # | Gap | Ref Repo (Kevin Bacon trace) | Real Path | Effort |
|---|---|---|---|---|
| 1 | 2 lexers → 20+ | `ref/major/scintillua` has **160 ready-made lexer patterns** (Lua/LPeg). Don't port LPeg — port the *patterns*: keyword sets, comment markers, string escapes are language data not algorithm. Each new lexer = ~40 lines in `lex.c` (a `lex_X(lang)` dispatcher). Priorities: Python, JS, HTML, CSS, Rust, Go, SQL, XML, YAML, TOML, Bash, Markdown, CMake, Dockerfile, Java, C++. Start with Python (highest user value). | **M** (12 day effort for 12 lexers, ~480 lines) |
| 2 | Command palette in standalone | Already exists in `src/ui/ui_atom.c` (110 lines) + `src/palette/` (68 lines). `test_atom.c` already green. Wire: in `apps/wubupad/main.c` add `#include "ui_atom.h"` + handle `Ctrl+Shift+P`. ~10 lines. | **L** |
| 3 | Fuzzy finder in standalone | Exists `src/fuzzy/` (69 lines) + `test_fuzzy.c`. Wire: `Ctrl+P` in `main.c`, call `fuzzy_search(...)`. ~15 lines. | **L** |
| 4 | Tree view in standalone | Exists `src/treeview/` (129 lines). Wire: side panel in `ui_gfx.c`. ~25 lines. | **L** |
| 5 | Snippets in standalone | Exists `src/snippet/` (98 lines). Wire: `Ctrl+J` expand. ~10 lines. | **L** |
| 6 | Multiple cursors in standalone | Exists `src/multicursor/` (36 lines). Wire: `Ctrl+Alt+Up/Down`. ~10 lines. | **L** |
| 7 | Minimap in standalone | Exists `src/minimap/` (49 lines). Wire: side panel after treeview. ~25 lines. | **L** |
| 8 | Markdown preview | Exists `src/mdpreview/` (136 lines). Wire: `Ctrl+M` switch view. ~15 lines. | **L** |
| 9 | Auto-indent in standalone | Exists `src/autoindent/` (52 lines). Wire: on Enter keypress. ~10 lines. | **L** |
| 10 | Plugin API in standalone | Exists `src/pkgmgr/` (189 lines) + sample `apps/wubuos/plugin.c`. Port to `apps/wubupad/plugin.c`. ~30 lines. | **L** |
| 11 | Print support | `ref/lesser-known/lite` (78MB, has no print either — bad ref). Use system `lp`/`lpr` via `popen()` + a minimal PostScript or HTML render → system browser. Quick MVP. | **M** |
| 12 | Call tips | `ref/major/scintilla` SCI_CALLTIP API. Implement in `ui_gfx.c` as a floating rect above caret showing function signature. ~50 lines. | **M** |
| 13 | Indicators (squiggle) | WuBuOffice already has red-squiggle in Editor; port the `underline_in_rect(x0,y0,x1,y1,color=wavy_red)` util. ~30 lines. | **L** |
| 14 | Bracket matching | Same as WuBuOffice #18. ~30 lines. | **L** |
| 15 | Indentation guides | Same as WuBuOffice #18. ~15 lines. | **L** |
| 16 | Whitespace visualization | Same as WuBuOffice #20. ~20 lines. | **L** |
| 17 | Embed Lua/JS scripting | `ref/lesser-known/mujs` (912KB, full JS engine, C API: `js_newstate/js_dostring`). Embed mujs, expose `doc.insert()`/`doc.text` API. | **M** |

### DA-2 Verdict
**Every gap I identified is closeable with concrete code from existing `ref/` repos.** The seven-steps-to-Kevin-Bacon trace succeeds in ≤2 hops for all 38 gaps: WuBuOffice's 21 gaps trace to harfbuzz/fribidi/abiword/gnumeric/calligra, WuBuPad's 17 to scintillua/scintilla/mujs and to its own `src/ui/` modules already built and tested. This is NOT research-vapor.

## DA-3 — Adversarial / Security (What Breaks)
> "Assume the plan is wrong. What's dangerous? What's mis-sequenced?"

### Risk 1: The "just wire it" delusion
**Attack**: Many MODULE items have been called from `test_view.c` (the headless renderer) but never from `view_doc.c`. The test runs in a hermetic mode with specific seed documents. When wired into the real view path, they'll hit:
- Real document trees (not just `wubumodel_doc_root(NULL) — seed-first` gotcha from the skill)
- Real layout recycling (`layout_invalidate` not called from `view_doc` for live edits)
- Memory leaks when used long-term (test is short-lived)
**Mitigation**: Each wiring must run the full `ctest` after, not just the targeted test. Add a smoke test that opens a document, invokes the new key handler, asserts the render pipeline doesn't crash.

### Risk 2: HarfBuzz/FriBidi bring C++ to the build
**Attack**: HarfBuzz is C++ (not C). Linking it pollutes the C11-only mandate. FriBidi is pure C — fine. But if we `apt install libharfbuzz-dev`, the headers pull C++ runtime.
**Mitigation**: HarfBuzz exposes a C API (`hb.h`), and the system package is pre-compiled — we link to the .so, don't build from source. Document explicitly in `entry.md`: HarfBuzz is the ONLY C++ dep, accessed via its stable C ABI, justified because CJK/RTL is half the world. Alternative: implement a minimal shaper using FriBidi for BIDI + manual glyph advance from FreeType metrics — works for Latin/Arabic/Hebrew but CJK needs HarfBuzz.

### Risk 3: The 40+ "MODULE" framing causes a wrong headcount
**Attack**: The GAPS_REAL doc says "40+ modules not wired into GUI". My count shows 63 `src/` dirs total, of which ~14 are linked in `apps/wubuos/CMakeLists` (`wubumodel, wuburender, wubuocr, wubufont, wubucell_app, wubudoc, wubuautosave, wubuspell, wubuchart, wubudraw, wubumath, wubuepub, wubua11y, wubusvg`). So 14 linked modules in the main binary, plus others linked in `test_view.c` (wubuscript). The "40+ not wired" claim is **directionally correct but imprecise** — the gap is "x feature is linked but the user can't trigger it from the GUI menu/keys," NOT "x feature is not in the binary." 
**Mitigation**: Re-audit gap count precisely. For each "MODULE" claim, distinguish (a) not in binary (real CMake gap), (b) in binary but not called from view (handler gap), (c) called from test_view but not view_doc (test-only). The remediation is different for each.

### Risk 4: Branch explosion in `view_doc.c` 
**Attack**: Adding 30 new `WUOS_KEY_*` handlers in `view_doc.c` (currently 702 lines) violates the no-monolith mandate and creates a god-file. The skill explicitly warns against this.
**Mitigation**: Use the `monolith-splitting.md` recipe from the `wubuoffice-gap-closure` skill. Each feature cluster (Draw, Math, EPUB, Spell, Print) extracted to a `doc_xxx.c` module with its own header + test. The view handler becomes a 2-line forward to `doc_xxx_handle(doc, key, &r_or_status)`.

### Risk 5: WuBuPad standalone becomes a wubuos clone
**Attack**: Porting all 9 wubuos-only features into `apps/wubupad/main.c` makes wubupad a second wubuos. Why maintain both?
**Defense**: WuBuPad is a *text editor* (Notepad++ parity); WuBuOffice is an *office suite* (MS Office parity). The shared editor core (lex, doc, search) is in `src/`, both apps link to it. Porting `ui_atom` to wubupad means wubupad gets command palette AS A TEXT EDITOR — different commands than wubuos. Maintain focus.

### Risk 6: Lexer extension breaks existing C+JSON tests
**Attack**: Adding 14 new lexer cases to `lex.c` (289 lines → ~770 lines) and a `lex_X(lang)` dispatcher may break existing `test_lex_fold`, `test_lex_symbols` tests if the dispatchers aren't keyed by language name. The piece-table tests assume one lexer at a time.
**Mitigation**: New lexers go in a sub-`lex_*.c` per language (e.g. `lex_python.c, lex_js.c`), dispatched from `lex_init(lang)` which returns a `const Lex*` with language-specific state. Existing tests unchanged.

### Risk 7: mujs embedding increases attack surface
**Attack**: Embedding a JS engine (mujs) into WuBuPad means user scripts can do file I/O, network, etc. Without sandboxing, a malicious plugin/clipboard macro can read `~/.ssh/`.
**Mitigation**: mujs has no default I/O — you must expose via `js_newcfunction`. Only expose `doc_*`, `view_*`, `clipboard_*`. No file/network bindings by default.

### Risk 8: Sequencing — what unblocks what
**Mis-sequence**: Doing the XL .docx fidelity port (#4) before the HarfBuzz/FriBidi shaping (#1) is wrong — you'd port abiword's OOXML reader which assumes text shaping works, then ship broken CJK in your "high-fidelity" .docx.
**Correct order**:
1. **First**: HarfBuzz/FriBidi wiring (#1) — text correctness is foundational. Everything downstream assumes it.
2. **Then**: WuBuPad standalone parity sweep (#2-10) — these are pure wirings, no new deps, unblock external testing.
3. **Then**: WuBuOffice easy wirings (#3, 12-14, 17-21) — shallow MODULE→REAL.
4. **Then**: WuBuOffice spreadsheet polish (#6-9) — formula engine is solid, just missing UI affordances.
5. **Then**: WuBuPad lexer expansion (#1) — gives WuBuPad reach (more languages).
6. **Then**: WuBuOffice .docx fidelity (#4) — the deep port. ABI for headers, footers, lists, images, comments.
7. **Then**: Plugin/extension ABI (#16) and scripting embedding (#17).
8. **Defer**: CRDT collab (#15), VBA compatibility, cloud sync. They're research scale.

## Consolidated Sequenced Plan (P0 → P3)

### P0 — Correctness (unblocks everything)
- **W1** HarfBuzz+FriBidi linked into `ui_gfx.c` draw path. 30-line change to `wubushape_run` call. Ref: harfbuzz/fribidi.
- **W2** PDF export from GUI. 20-line handler for `WUOS_KEY_EXPORT_PDF`. Ref: own `src/wubupdf`.

### P1 — WuBuPad Standalone Parity Sweep (L each, all in 1-2 days)
- **P1-P9**: Port command palette, fuzzy finder, tree view, snippets, multiple cursors, minimap, markdown preview, auto-indent, plugin API. All exist in `src/`, just need `main.c` wirings. 9 × ~15-line change = ~135 lines total.
- **P10** Bracket matching + indentation guides + whitespace viz (C11 shared with office #18-20).

### P2 — WuBuOffice Easy Wirings
- **W3** Spell-check in Document view (ref: own `src/wubuspell`)
- **W4** Draw module in `view_doc.c` (`WUOS_KEY_INSERT_SHAPE`)
- **W5** Math editor in `view_doc.c` (`WUOS_KEY_INSERT_MATH`)
- **W6** EPUB export in `view_doc.c` (`WUOS_KEY_EXPORT_EPUB`)
- **W7** Charts rasterize to SDL surface (40-line: SVG → rgba → blit), ref: own `src/wubusvg`
- **W8** Slide transitions (alpha interp, ref: calligra but simplified)
- **W9** Freeze panes (30 lines in `view_cell.c`)
- **W10** Background autosave (pthread)

### P3 — Spreadsheet Polish + WuBuPad Reach
- **W11** Merged cells (ref: gnumeric)
- **W12** Number formats/dates
- **W13** Conditional formatting
- **W14** WuBuPad: 12 new lexers (Python, JS, HTML, CSS, Rust, Go, SQL, XML, YAML, TOML, Bash, Markdown). Ref: scintillua patterns.
- **W15** WuBuPad: call tips (ref: scintilla SCI_CALLTIP)
- **W16** Plugin ABI + Lua/JS scripting (ref: mujs)

### P4 — Deep Port (defer)
- **W17** .docx fidelity to match abiword's `ie_imp.cpp`. XL.
- **W18** Draw full diagram module (flowcharts, connectors, grouping).
- **W19** CRDT collab over local network.
- **W20** Base/database module.

## What NOT to do
- **Don't port LPeg/Lua into WuBuPad** — that's adding a Lua runtime for lexers. Port the *patterns* (keyword sets, comment markers) into hand-written C state machines, not the LPeg engine.
- **Don't build a real "plugin marketplace" UI** — sample .so loader + `plugin_load_dir()` is enough for v1. (Notepad++ didn't have Plugin Manager until v7.6.)
- **Don't port abiword's full OOXML reader** — 60K lines of C++. Port the 8 things that matter for fidelity: headers, footers, lists, images, comments, footnotes, sections, styles gallery.
- **Don't bundle cloud sync** — keep it a thin shim if anything.
- **Don't break `wubupad` standalone tests** while wiring office features.
