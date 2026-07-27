# GUI Parity Plan — whole office suite + Notepad++ in one shell

Single unified SDL2 shell (`apps/wubuos`) hosts every WuBuOffice engine behind a
`WuView` adapter (tab). Goal: full GUI + Notepad++ parity for the *entire* suite.
Aligned to `~/WuBuPad/PLAN_BLITZ.md` (Phase A gfx → B find/replace → C layout/theming
→ D editor features → E cross-repo). `wubuos` IS Phase E + the shell.

Real engine surface (surveyed, not guessed):
- **Document** (`wuos_doc_create`): `wurender` (WordprocessingML render path).
  `wubudoc` facade ingests .txt/.md/.json/.csv/.svg/.xml/.html → docmodel.
- **Editor** (`wuos_editor_create`): REAL WuBuPad `Doc` core (piece-table, undo,
  column-sel) + `lex` highlighter, embedded cross-repo. `search.h` = DONE Thompson-NFA.
- **Spreadsheet** (`wuos_cell_create`): REAL `wubucell` builder (numbers, formulas,
  SUM, chart ref) → grid render.
- **Slide** (`wuos_slide_create`): simple slide render (reuses wuburender/chart).
- **OCR** (`wuos_ocr_create`): REAL `wubuocr` (`ocr_image_from_png` + `ocr_page_analyze`)
  → grayscale page + recognized text.

## Status (committed)
- [x] Shell skeleton: tab bar (click-switch), status bar, scroll, CLI routing.
- [x] File open/save: Editor loads by path (lexer by ext) + Ctrl+S; Document loads
      markdown/text via `wurender_doc_from_markdown`.
- [x] Editor Find/Replace (Phase B): Ctrl+F find, Ctrl+H replace, F3/Shift+F3
      next/prev, Ctrl+R replace-all, Esc close. Literal + regex. Match highlight
      + live count. Verified (7 matches→7 replaced).
- [x] Paint tab labels + status bar text (was blank). Hover highlight.
- [x] Headless `test_view` (ctest label `view`): render-check every view, file
      open/save round-trip, find/replace assertion.
- [x] **Phase C (Layout/Theming) — ALL CLOSED:**
      - Dark theme toggle (Ctrl+`) — `render` theme tokens.
      - Multi-doc tabs with dirty markers (Ctrl+T/W/Tab) — `src/docs`.
      - Go-to-line (Ctrl+G).
      - EOL indicator (LF/CRLF) + encoding label in status.
- [x] **Phase D (Editor features / Notepad++ parity) — ALL CLOSED:**
      - EOL convert (Ctrl+E) on buffer.
      - Column/block selection (Ctrl+Alt+C).
      - Macro record/play (Ctrl+Shift+R / Ctrl+Shift+P).
      - Code folding (Ctrl+Shift+F) via `lex_folds`.
      - Auto-completion (Ctrl+Space) via `lex_symbols` + builtin C words.
      - Function list (Ctrl+Shift+L) via `lex_symbols`.
      - More lexers (C, JSON) — see `GAPS_NOTEPAD.md`.
      - Bracket matching, bookmarks (Ctrl+F2 / F2).
      - Plugin architecture: stable C ABI v1 + `dlopen` loader + sample `.so`
        + Ctrl+Shift+K (INT-100 seed; see `RESEARCH_GAPS_100.md`).
- [x] **Phase E bis (suite breadth) — ALL CLOSED + made genuinely interactive:**
      - Cell tab: real `wubucell` workbook, live formula bar, arrow nav, edit a
        cell (recompute via engine), SUM verified (E1=79).
      - OCR tab: real `wubuocr` pipeline + **fontbank recognizer** wired so the
        panel shows actual recognized text (not empty geometry). Multi-line
        sample page for fair line/word segmentation.
      - Document tab: `wubudoc` facade ingests docx/odt/pdf/html/... ; find-in-doc
        (Ctrl+F) over loaded text; markdown renders via `wurender`.
      - Slide tab: renders sample slide (title + bullets + bar chart).
      - `apps_smoke` ctest target gates the interactive suite per-commit
        (`./tools/smoke.sh apps`).
- [x] **Engine hygiene:** fixed two sanitizer leaks in `wubuocr` (`ocr_page_block_text`
      caching) and `wubumodel`/`wurender` (style refcount) + all view teardown
      leaks; `test_view` is ASan-clean (0 leaks).
- [x] **P0 INT-2 wiring — `wubuautosave` now attached to the Editor (was never
      linked by any app):** on file open it creates an `Autosave` session,
      detects edits via doc-length change and snapshots atomically every ~60
      frames (5s min gap), flushes+clears on save, and on reopen offers+applies
      crash recovery (splices the recovered text into the buffer). Headless
      `test_view` verifies recovery round-trips into the editor.
- [ ] Unified launcher `wubuoffice` → boots `wubuos` (cosmetic name alias).
- [ ] Settings dialog (font size, tab width, word-wrap, EOL view) — currently
      engine-supported, no dialog UI yet.
- [ ] Live word-wrap toggle.

> Cross-repo P0 cluster still open (next frontier, see `RESEARCH_GAPS_100.md`):
> `wubuspell` live red-squiggle, `wubuchart` insert-chart, `wubudraw`/`wubumath`
> insert shape/equation, `wubuepub` export, `wubua11y` check, `wubuscript`
> computed fields. OCR/autosave are now GUI-wired.

> See `GAPS_NOTEPAD.md` (WuBuPad) for the authoritative Notepad++ closure list
> and `RESEARCH_GAPS_100.md` for the remaining 100-gap product backlog (the
> "engines built but not linked by any app" P0 cluster is the next frontier:
> `wubuautosave`, `wubuspell`, `wubuchart`, `wubudraw`/`wubumath`, `wubuepub`,
> `wubua11y`, `wubuscript`).

## Office-format fidelity (the "100 gaps" long tail)
Per engine, every open + save path:
- Word: .docx .odt .rtf .html .md .txt (open+save)
- Cell: .xlsx .ods .csv (open+save)
- Slide: .pptx .odp (open+save)
- OCR: image→text (png/jpg) + hOCR/ALTO export
- Drawing/SVG: .svg open+save
- EPUB/Note/Read: existing apps wired as tabs
- Charts/merged cells/RTL: incremental fidelity

## Verification gate (every slice)
- `ctest --test-dir build -R view` (headless smoke) stays green.
- New behavior gets a headless assertion in `test_view.c`.
- New code 0 warnings (`-Wall -Wextra -Wpedantic`).
- Commit per slice; no monolith; opaque modules.
