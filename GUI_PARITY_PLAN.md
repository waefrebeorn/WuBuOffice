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

## Status (committed this session)
- [x] Shell skeleton: tab bar (click-switch), status bar, scroll, CLI routing.
- [x] File open/save: Editor loads by path (lexer by ext) + Ctrl+S; Document loads
      markdown/text via `wurender_doc_from_markdown`.
- [x] Editor Find/Replace (Phase B): Ctrl+F find, Ctrl+H replace, F3/Shift+F3
      next/prev, Ctrl+R replace-all, Esc close. Literal + regex. Match highlight
      + live count. Verified (7 matches→7 replaced).
- [x] Paint tab labels + status bar text (was blank). Hover highlight.
- [x] Headless `test_view` (ctest label `view`): render-check every view, file
      open/save round-trip, find/replace assertion.

## Phase C — Layout / Theming
- [ ] Dark theme variant (semantic token table; persist to `~/.wubuosrc`).
- [ ] Editor: sticky tab bar over multi-doc (dirty markers) — WuBuPad `docs.c`.
- [ ] Editor: settings dialog (font size, tab width, word-wrap, EOL view).
- [ ] Live word-wrap toggle (re-wrap lines in render).
- [ ] EOL indicator (CRLF/LF) + encoding (UTF-8) in status.

## Phase D — Editor features (Notepad++ parity)
- [ ] EOL convert (CRLF↔LF) on buffer.
- [ ] Column/block selection mode (cursor + buffer range ops).
- [ ] Macro record/play (command log + replay).
- [ ] Code folding (lexer fold levels + view).
- [ ] Auto-completion (lexer symbol index).
- [ ] More lexers (extend `src/lex.c`: py, js, sh, html, css, sql, go, rust).
- [ ] Bracket matching highlight.
- [ ] Go-to line (Ctrl+G).
- [ ] Multiple open files in tabs (Editor tab hosts several Docs).

## Phase E bis — cross-repo + suite breadth
- [ ] Cell tab: open real .csv/.xlsx via `wubucell` reader; click cell → formula bar;
      arrow navigation; edit a cell (recompute formula).
- [ ] OCR tab: open arbitrary PNG path (not just /tmp/ocr_in.png); show recognized
      text panel + confidence; save .txt.
- [ ] Document tab: open real .docx/.odt via `wubudoc` facade → render (not just md).
- [ ] Slide tab: open real slide deck; next/prev slide (arrows).
- [ ] Unified launcher `wubuoffice` → boots `wubuos` (one product, two names).

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
