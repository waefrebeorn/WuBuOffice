# Office Suite — Gap Analysis (research-grounded)

> Source: 20 web searches (LibreOffice modules, ODF/OOXML libs, spell/CRDT/
> shaping/fonts, editor perf, scripting, i18n, PDF) + local inventory of
> WuBuOffice apps + WuBuPad blitz. Posture: telegraphic, prioritized,
> C11-buildable first. "Wrap" = link a mature C/C++ lib behind a C11 ABI.

## 1. What we ALREADY have (don't rebuild)
- Word `wubuword`, Sheets `wubucell`, Present `wubushow`, Notes `wubunote`.
- Format IO: `wubuoxml` (OOXML), `wubuodf` (ODF), `wubucfb` (legacy .doc),
  `wubuconv`, `wubuzip`, `wubuxml`, `wubujson`, `wubupdf`, `wuburead`.
- Engine: `wubuformula`, `wubusvg`, `wubufont` (FreeType), `wubuimage`,
  `wubuocr` (CRNN+QR+PDF text search), `gpu` (CUDA/OpenCL BLAS).
- Editor core: WuBuPad `Doc`/`UI`/`search` (bridged as `wubupad_bridge`).
- Apps: `wubuview`, `wubuedit`, `wubutui`, `wubumodel`, `wubugauntlet`,
  `wubulegacy`.

## 2. The GAPS (prioritized)

### P0 — rendering correctness (blocks whole markets)
- **Text shaping (HarfBuzz).** gfx backend uses FreeType only → CJK, RTL
  (Arabic/Hebrew), Indic, Thai, complex ligatures render WRONG or break.
  This is not a feature, it is a correctness blocker for ~half the world.
  Research: HarfBuzz is THE lib (C++, wraps cleanly; we already pull
  FreeType). Add `src/wubushape/` wrapping hb-ft: buffer→shape→glyph run.
  Priority 0. Effort: M. Dep: harfbuzz-dev (apt via rootexec).
- **RTL / BIDI (Unicode UAX#9).** Pair with HarfBuzz. Needs a bidi resolver
  (libfribidi, C). Priority 0 (same market block). Effort: M.

### P1 — table-stakes features users expect day 1
- **Spell + grammar check.** No checker wired into `wubuword`. Research:
  Nuspell (C++, fast, morphology) or Hunspell (C). Wrap behind
  `src/wubuspell/`; underline + suggest in gfx. Priority 1. Effort: M.
  Dep: nuspell-dev / hunspell-dev + dictionaries.
- **Charts/visualization.** `wubucell`/`wubuword` have no chart object.
  Research: build a small from-scratch renderer (bars/lines/pie) on
  `wubusvg`/cairo; ingest `wubuformula` results. Priority 1. Effort: L.
- **Autosave + crash recovery + version history.** Reliability gap vs
  MS/Google. Piece-table (WuBuPad) makes snapshots cheap. Priority 1.
  Effort: M. Build on `Doc` undo stack + periodic snapshot to `~/.wubu/`.

### P2 — completeness vs LibreOffice's 6 modules
- **Draw / vector + diagramming.** `wubusvg` exists (SVG IO/render) but no
  Draw *module*: shapes, connectors, grouping, flowcharts. Priority 2.
  Effort: L. Reuse `wubusvg` as the canvas.
- **Math editor UI.** `wubuformula` is engine-only; no WYSIWYG Math editor
  (LibreOffice Math equivalent) to compose/edit formulas visually. Priority 2.
  Effort: M (UI over existing engine).
- **Base / database.** LibreOffice Base equivalent. Large, niche for a C
  suite. Priority 3 (defer). Effort: XL.

### P3 — modern expectations / differentiators
- **Real-time collaboration (CRDT).** Google Docs baseline. Research: Yjs/
  Automerge (TS/Rust) — no clean C core. Options: (a) embed a WASM/TS
  runtime (heavy), (b) port a RGA/Logoot text CRDT to C11 over WuBuPad
  `Doc` ops (ambitious, on-brand). Priority 3. Effort: XL. Note as
  "research project," not v1.
- **Cloud sync + AI surface.** MS 365 Ignite 2025 leans hard on this. For a
  local C suite this is an *integration surface* (plugin/CLI to an external
  LLM), not a bundled feature. Priority 3. Effort: M (API shim).
- **Web export: EPUB / HTML / Markdown round-trip.** Interop + publishing.
  `wubuconv` may partially cover; no first-class EPUB. Priority 2.
  Effort: M.
- **Accessibility (WCAG / PDF-UA / tagged PDF).** `wubupdf` should emit
  tagged PDF + an a11y checker (headings, alt-text, table headers). Legal/
  gov market. Priority 2. Effort: M.
- **Mail merge.** Data source + template fields in `wubuword`. Priority 3.
  Effort: M.
- **Plugin / extension ABI.** Ecosystem leverage (like LO extensions).
  Priority 3. Effort: L.

### P4 — engineering hardening
- **Large-doc pagination perf** in `wubuword` (piece-table already; add
  async layout). Low.
- **Scripting/macro engine** (VBA-like / Office Scripts TS). Research:
  embed a tiny C interpreter OR Lua (C, clean ABI) as the macro lang over
  `Doc`/`cell` APIs. Priority 2 (power-user). Effort: L.

## 3. Recommended next moves (sequenced)
1. `wubushape` (HarfBuzz+FreeType) + `wubibidi` (fribidi) — unblocks CJK/RTL.
   Highest leverage, fixes a correctness bug not just a gap.
2. `wubuspell` (Nuspell wrap) into `wubuword` — table stakes.
3. Charts engine on `wubusvg` + `wubuformula`.
4. Autosave/recovery + version snapshots.
5. Draw module (over `wubusvg`) + Math editor UI (over `wubuformula`).
6. EPUB/HTML export, tagged-PDF a11y, Lua macro ABI.
7. Defer: Base, CRDT collab (research), cloud/AI (shim only).

## 4. Library shortlist (C11-wrapable)
- HarfBuzz (shaping, C++), FreeType (have), FriBidi (bidi, C),
  Nuspell/Hunspell (spell, C++/C), MuPDF/Poppler (have, PDF),
  Lua (macros, C, clean), libxml2 (have via wubuxml), Cairo (charts, C).

## 5. What NOT to do
- Don't re-implement OOXML/ODF parsers — `wubuoxml`/`wubuodf` exist.
- Don't port a CRDT to v1 — too risky; revisit as research spike.
- Don't bundle cloud/AI as a core dep — keep it a thin optional shim.
