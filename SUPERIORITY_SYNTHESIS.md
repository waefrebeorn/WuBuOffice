# SUPERIORITY_SYNTHESIS.md -- online research -> WuBuOffice plan (2026-08-22)
Research: TDF architecture blog (2026-07-31), docx-editor.dev fidelity matrix,
Microsoft keyboard-interactions spec, WCAG 2.1, NN/g usability research,
LLM-GUI-agents survey (arXiv 2411.18279), a16z accessibility-tree pattern,
MCP ecosystem surveys.

## 1. FORMATTING (the fidelity doctrine)

Research anchor: LibreOffice wins fidelity because its INTERNAL MODEL is the
document standard; OOXML-shaped models lose "instructions" (slide-count
fields, formulas) and keep only last VALUES. The grab-bag pattern -- set aside
what you cannot express, restore on round-trip -- is a documented filter
architecture, not an accident.

Adopt for WuBuOffice:
  F1  GRAB-BAG on ingest: every construct wubumodel cannot express must be
      preserved as opaque (namespace, raw XML, anchor) in the model and
      re-emitted on save. NEVER drop what we cannot render. Audit: round-trip
      corpus test asserting zero silent loss.
  F2  Fidelity matrix as a TESTED artifact (like docx-editor.dev publishes):
      per feature x {load, render, round-trip}, wired into ctest so regressions
      are failures, not vibes. Start: tables/styles, floating images+wrap,
      headers/footers, lists/numbering, bidi, fields/TOC.
  F3  Known breakers first (research + user reports): floating images with
      text wrapping; table styles via basedOn chain + tblLook/cnfStyle;
      embedded fonts (de-obfuscate word/fonts on load).

## 2. HUMAN-FRIENDLY UI (the conventions doctrine)

Research anchors: Microsoft keyboard-interactions table (Ctrl+S/F/Z... are
ACCESSIBILITY infrastructure, not conveniences); NN/g 5-user testing;
recognition-over-recall; progressive disclosure; "remove/hide/shrink/organize".

Adopt:
  U1  Shortcut CONFORMANCE AUDIT: every command's accelerator must match the
      Microsoft/common convention table; deviations are bugs. Gate in ctest.
  U2  Button labels = verbs users already know (New/Open/Save/Undo), icons
      WITH text labels until recognition is proven; tooltips carry the
      shortcut ("Save (Ctrl+S)") so labels teach the accelerator.
  U3  Progressive disclosure: default toolbar = the 12 commands 95% of tasks
      use (already hive-driven); everything else reachable via menu/palette.
  U4  Guided empty states everywhere (Compare done; audit remaining views).
  U5  Real-data stress test: 40-char German names, billion-scale currency,
      empty charts (enterprise UX checklist) -- add to view tests.

## 3. AGENT BACKEND (the OS-substrate doctrine)

Research anchors: LLM-GUI-agents survey (accessibility-tree observations beat
screenshots); a16z: "the accessibility tree already exposes structured
elements"; MCP as universal adapter; our own cua-driver semantic snapshots.

WuBuOffice ALREADY has doc_agent NDJSON (open/ingest/json/text/set/media/
create/list/close/quit). Upgrade to superiority:
  A1  EXPAND the verb set to full CRUD: get_structure (outline/headings/
      tables/figures), query (find with context), edit ops (insert_heading,
      set_cell, apply_style), export (pdf/html/md). Every UI action must have
      an agent verb -- one action model, two frontends (human GUI, agent JSON).
  A2  ACCESSIBILITY TREE of the shell itself: expose tabs/menus/buttons as
      structured elements with roles + refs (like DOM refs in cua-driver),
      over the same NDJSON bus. Then agents drive WuBuOffice semantically --
      no screenshots, no pixel clicking -- while screen readers get it free.
  A3  Single-binary serve mode: `wubuos --serve` speaks NDJSON on stdin/out
      so ANY MCP bridge can wrap it (USB-C for the office).
  A4  Determinism contract: same command stream => same document state
      (hashable), enabling agi regression tests.

## Priority order (superiority = fidelity x humans x agents)
  P0: F1 grab-bag (stop data loss) + A1 expand agent verbs
  P1: F2 fidelity matrix tests; U1 shortcut conformance gate
  P2: A2 shell a11y tree; U4 empty-state sweep; F3 floating-image wrap


---
## RECURSIVE LEARNING LOOP LOG
### Hop 1 (2026-08-22): AX principles (apideck AX article, Karpathy CLI lesson)
- Learned: agents are terminal-native; self-describing interfaces beat docs;
  errors must tell the agent how to recover.
- Applied: `help` verb (self-describing verb list); unknown-command errors now
  actionable ("send {"cmd":"help"}"); serve accepts bare `quit`/`q` and
  answers {"ok":true,"bye":true}. Tests: agent_verbs extended.
- Verified: live NDJSON pipeline help->ingest->structure->find->error->quit.

### Hop 2 (2026-08-22): TDF fidelity doctrine applied (F1/F2 shipped)
- Grab-bag preservation (WUBUMODEL_FOREIGN) + verbatim re-emission on save;
  fidelity matrix ctest (fidelity_matrix) gates silent data loss.

### NEXT HOPS (tracked, not faked)
- H3: FLOATING IMAGES w/ text wrapping (docx anchor tblpPr/wp:anchor ->
  model float fields -> layout wrap-around pass). Biggest remaining breaker.
- H4: shell accessibility tree over NDJSON (tabs/menus/buttons with roles+refs).
- H5: embedded font de-obfuscation on load (word/fonts).
- H6: table styles via basedOn chain + tblLook/cnfStyle resolution.

### Hop 3 (2026-08-22): H4 shipped -- shell accessibility tree
- wuos_shell_a11y.c: full UI as JSON (application > tablist/menubar/toolbar/
  status) with stable refs ("tab:N", "menu:M:item:I", "tb:B") + shortcuts +
  cmd ids. WUOS_A11Y_DUMP=<path> headless hook; ctest a11y_tree validates
  JSON structure. Agents and screen readers share one semantic surface.
- Live-verified: 7 tabs, 30+ menu items with shortcuts+cmd ids, toolbar
  buttons all present in the dump.

### Hop 4 (2026-08-22): H4b shipped -- WUOS_ACT read-act-read loop
- Agents act on semantic refs: WUOS_ACT="tab:N"|"tb:N"|"menu:M:item:I"
  executes through the shell's own action path (run_tb_cmd/run_menu_cmd),
  then the a11y tree reflects the result. ctest covers the loop.
- Live-verified: WUOS_ACT="tab:4" -> tree reports Editor selected.

### Hop 5 (2026-08-22): H5 shipped -- ODTTF embedded fonts
- odttf.c: GUID-reversed XOR key decode, byte-exact round-trip test.
- BONUS real bug found by the negative test: font_open() SEGV'd on corrupt
  blobs (n_tables unchecked vs blob size). Bounds guard added.

### Hop 6 (2026-08-22): H6 shipped -- table styles
- table_styles.c: parses styles.xml table styles (type="table"), walks
  basedOn chains (cycle-safe), applies tblLook-selected conditional formats
  (firstRow/lastRow/band1H/band2H) per row. Header rows resolve bold+
  centered+accent shading with zero direct formatting. Test covers chain
  resolution, banding parity, unknown-id safety.

### Hop 7 (2026-08-22): H6b shipped -- merged cells
- gridSpan (horizontal span) + vMerge restart/continue parsed from w:tcPr
  onto CELL nodes; col_span getter defaults >=1. docx_cell_merge test covers
  origin cell (span2+restart), plain cell, and continue cells.
- Process note: a careless `mv` clobbered the pre-existing XLSX cell_merge
  test source; restored from git, both tests now coexist.

### Hop 8 (2026-08-22): geometry-aware PDF export shipped
- Research: "export a PDF alongside the editable file" is the universal
  fidelity escape hatch (tech-insider 2026 suite comparison).
- Gap found: wubuexp_pdf used page_text() -- reflowed everything to plain
  12pt Helvetica, dropping bold/sizes/tables/images entirely.
- Shipped: wubuexp_pdf_geometry() walks line boxes + runs so font size,
  bold/italic (3 font resources), per-run positioning, and object boxes
  (tables/images/floats drawn as rects) survive into the PDF.
- doccmd_export_pdf prefers the geometry path, legacy as fallback.
- pdf_geometry ctest asserts /Helvetica-Bold resource + heading font size
  preserved (not flattened to 12) + text present + valid EOF.
