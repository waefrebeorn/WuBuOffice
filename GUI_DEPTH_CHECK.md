# GUI Depth-Check — Research Synthesis Round 3 (2026-08-12)

Companion to GUI_MATHEMATICS.md + GUI_EXCELLENCE.md. This round's focus:
**round-trip format integrity** (load → edit → save → reload) and **true depth
testing** (not just existence checks). 25-search Kevin-Bacon pass.

## Confirmed right (already implemented, research-verified)
- **Word count in status bar** (Word: live words/chars) — WuBuOffice doc view
  ("%zu words") + editor ("words %zu chars %zu") both show it.
- **Spell-check red squiggles** (Word '95 signature) — editor draws red zigzag
  under misspelled words (view_editor.c).
- **Find/Replace dialogs separate** (UX consensus) — WuBuOffice has Find vs
  Replace vs Go-to-Line as distinct commands.
- **OOXML + ODF both** (Collabora: an office must handle .docx and .odt) —
  WuBuOffice loads/writes DOCX/ODT/RTF/EPUB; spreadsheet CSV/XLSX/ODS.

## Depth-check gaps FOUND + FIXED this round
1. **RTF/EPUB were load-only** — opened fine but Ctrl+S silently wrote a .docx
   sidecar (no writer). doccmd_save now saves back to the loaded format:
   .odt→ODT, .rtf→RTF (wubuexp_rtf via layout), .epub→EPUB (epub_write),
   else .docx. Round-trip test added (save a fresh doc to each of 4 formats,
   RELOAD it, assert text survives) — all pass.
2. **open_doc_path only routed .md/.c/.h/.py/.txt to the editor** — every other
   text format (.json/.js/.css/.sql/.cpp/.hpp/.tex/.html/.xml/.yml/.yaml/.ini/
   .sh/.toml) fell through to a blank doc view. Now routed to the editor
   (round-trip save works there).
3. **Spreadsheet had NO save hook** — loaded CSV/XLSX/ODS but Ctrl+S did
   nothing. Added CellV.path + save() (CSV via wubucell_write_csv, XLSX/ODS via
   wubucell_assemble), wired as v->save. Also routed .csv/.xlsx/.ods/.xlsm/.tsv
   to the spreadsheet view in open_doc_path.
4. **set_cell APPENDED instead of REPLACING** — editing an existing cell created
   a duplicate; wubucell_get returns the FIRST match, so edits were invisible
   on read. set_cell now replaces in place. Verified: edit A1→'hi', save,
   reload gives 'hi'.
5. **Modal dialog didn't suppress the context menu** — right-click during a
   modal stacked a context menu over it. Now suppressed while dialog_active().
6. **Find reported a boolean '1 match'** — now counts ALL occurrences
   (VS Code/Word pattern).

## How to do the depth check (recursive loop)
For each feature a user actually does, verify the FULL task, not existence:
- **Round-trip**: can the app LOAD format X, EDIT it, and SAVE back to X? Test
  = save a known doc → reload → assert content (not just "file exists").
- **Routing**: does opening path.ext dispatch to the RIGHT view (not a blank
  fallthrough)? A text format must open as editable text.
- **Mutation visibility**: after an edit, does a READ return the new value
  (duplicates hidden by first-match = bug)?
- **Modal exclusivity**: a modal dialog must own ALL input (keyboard AND mouse).
- **Status accuracy**: feedback must report the real state (all matches, not a
  boolean).

Remaining known gap (not fixed, larger): the Slide view is a static preview
with no editing/save model — a full presentation editor is a major feature.
