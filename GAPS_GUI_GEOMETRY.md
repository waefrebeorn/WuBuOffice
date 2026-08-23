# WuBuOffice — GUI MARGIN & FEATURE GAP LIST (2026-08-23)

Audit trigger: "margins are hardcoded, cells are one ecstatic shape, not
adjustable — people can't actually make documents." This file lists every
hardcoded-geometry site found and the real-suite features still missing.
Format: `[P0|P1|P2] area — gap — status`.

## A. Hardcoded geometry (the "ecstatic shape" class)

1. **[CLOSED 2026-08-23] Spreadsheet columns all 88px.** `view_cell.c` drew
   every column at `WUOS_SPACE_8*11` and mapped clicks by integer division.
   Fixed end-to-end:
   - Model: `wubucell_col_width_set/get` per sheet+col (Excel char units),
     cloned with the undo snapshots, freed in `wubucell_free`.
   - XLSX: `<cols><col min max width customWidth="1"/>` written before
     `<sheetData>`; reader parses it back (round-trip).
   - View: column x = running sum of per-col widths; header band drawn per
     column with a divider edge; cell body + value clip use the column's own
     width.
   - Interaction: generic `WuView.drag_start/drag_move/drag_end` API wired in
     `main.c`; dragging a column-header's right edge resizes that column
     (24px minimum), Excel-style.
   - Click hit-testing walks the same running-sum geometry, so clicks land
     correctly after any resize.
2. **[OPEN P1] Row heights hardcoded** to font-height+8 (`rh2 = lh`). Real
   suites allow per-row height + wrap. Needs the same model/view/drag
   treatment as columns (model field `rowh[]`, drag on row-header divider,
   xlsx `<row ht=...>` round-trip).
3. **[OPEN P1] Grid margins hardcoded** (`margin_x=40`, `margin_y=56`) and
   duplicated as literals inside click handlers. Centralize in one
   geometry module shared by render AND input so they can never drift.
4. **[OPEN P1] OCR panel width hardcoded** at 300px (`panel_x = w-300`);
   no splitter, no resize.
5. **[OPEN P2] Document view page width/margins fixed** — no ruler, no
   margin handles, no zoom-aware reflow of the page box.
6. **[OPEN P2] Slide view: single fixed layout** — no movable/resizable
   text boxes or chart frames (real Impress/PowerPoint are freeform).

## B. Missing features that block "actually making documents"

7. **[OPEN P0] Spreadsheet editing depth**: no cell number formats surfaced
   (currency/%/date), no bold/border/fill on cells via the style registry in
   the UI (registry exists in the model), no multi-sheet tab bar in the
   view, no sort/filter.
8. **[OPEN P1] No selection ranges**: click = single cell only. Need
   shift-click / drag marquee, range formulas (SUM(A1:B4)) entered from a
   selection, fill handle.
9. **[OPEN P1] No clipboard for cells** (copy/cut/paste range incl. CSV
   flavor); paste-special transpose.
10. **[OPEN P1] Document: no table insert/edit UI**, no image resize
    handles, no floating-object drag (objects render inline only).
11. **[OPEN P2] No find & replace across views** (editor has search;
    doc/sheet/slide do not share it).
12. **[OPEN P2] No print/page-setup dialog** (paper size, orientation,
    scale-to-fit) though PDF export exists.

## C. Rule going forward

Every new pixel metric goes through a token (`WUOS_SPACE_*`) or the model —
never a bare literal in a render loop. Any geometry used by input must come
from the SAME function the renderer uses (see `cellv_col_x`).
