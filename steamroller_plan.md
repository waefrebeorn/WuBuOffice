# WuBuOffice + WuBuPad GUI Steamroller Plan

Based on research docs: office_docket.md, ws11_design.md, RESEARCH_GAPS_100.md,
PLAN_BLITZ.md, GAPS_NOTEPAD.md, GAPS_GUI.md, RESEARCH_UX_UI.md, UI_TRIPLE_REVIEW.md

## Steamroller Pass 1: Render Consistency (DONE)
- [x] Fixed view_cell.c spacing: gy0-lh-2 -> gy0-fh-6, gy0-lh-4 -> gy0-fh-6
- [x] Fixed view_slide.c spacing: fh+14 -> fh+6 (consistent with editor)
- [x] Rebuilt wubuos clean, 0 warnings, all tests pass
- [x] SDL2 clipboard wired (cut/copy/paste/select-all, Ctrl+X/C/V/Shift+V/A)
- [x] Notepad++ keyboard parity complete (Ctrl+X/C/V/Shift+V/A + Ctrl+A + Ctrl+Z/Y)
- [x] Menu bar with File/Edit/View/Help dropdowns + mouse handling
- [x] Headless screenshot tool (`viewshot`) produces pixel-matching PNGs

## Steamroller Pass 2: Paste-Plain Integration (PARTIAL)
- [x] Added include path for pasteplain in apps/wubuos/CMakeLists.txt
- [x] Added WUOS_KEY_PASTE_PLAIN to wuos.h enum
- [x] Added Ctrl+Shift+V handler in main.c key event loop
- [ ] Wire pasteplain_strip into editor on_key handler (needs doc_type access)

## Steamroller Pass 3: Remaining GUI items from RESEARCH_GAPS_100
- [ ] UXA-37: Modal dialog focus management verification
- [ ] UI scaling for high-DPI: wubusettings_ui_scale applied in Document view only, need Editor view parity
- [ ] Voice control / command palette consistency across all 5 views

## Steamroller Pass 4: Visual Craft
- [ ] Window title bar branding (wuos_window_title set to "WuBuOffice" not "wubuos")
- [ ] About dialog with license info
- [ ] Consistent icon set across all views

## Steamroller Pass 5: Virtualized Canvas (per office_docket 1407, 1461)
- [ ] Render only visible region for large documents (performance)
- [ ] Async layout for documents > 100 pages

## Verification gate (every pass)
- WuBuOffice: `ctest --test-dir build -R view` (headless smoke) stays green.
- New behavior gets a headless assertion in `test_view.c`.
- New code 0 warnings (`-Wall -Wextra -Wpedantic`).
- Commits per pass; no monolith; opaque modules.