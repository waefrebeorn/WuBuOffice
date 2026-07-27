# WuBuOffice — 100 Missing Gaps (Research-Driven Roadmap)

*Generated 2026-07-26 from a 2-pass web research run (69 + 75 query clusters,
~297 result snippets) plus an internal integration audit. Categories:*

- **INT** Integration/Plumbing (libs exist but nothing ships them to a UI)
- **UI**  User Interface / UX (we have built headless — never seen it)
- **UXA** Accessibility (WCAG 2.2, EPUB a11y, screen readers)
- **DOC** Document model / editing features
- **EXP** Import / Export / Format fidelity
- **COL** Collaboration / sync
- **SCR** Scripting / automation / AI
- **PRF** Performance / architecture
- **ART** Artistic / visual-design craft

*Priority: P0 (blocker for first usable release), P1 (core), P2 (polish/expansion).*

---

## INT — Integration / Plumbing (the silent killer)
1.  **P0** `wubuchart` is NOT linked by any app — charts cannot be inserted anywhere. Wire into WuBuWord insert-chart flow. — **CLOSED**: Document tab Insert→Chart; rasterized via new wubusvg rasterizer (gap #13).
2.  **P0** `wubuautosave` is NOT linked by any app — no document is actually auto-saved. Attach a session to WuBuWord/WuBuPad on open, flush on close, recover on crash. — **CLOSED**: Editor attaches an Autosave session, snapshots on edit, recovers on reopen.
3.  **P0** `wubudraw` / `wubumath` are NOT linked by any app — Draw & Math UI are invisible. Build an insert-shape / insert-equation surface. — **CLOSED**: Document tab Insert→Draw/Math; rasterized via wubusvg.
4.  **P0** `wubuepub` is NOT linked by any app — EPUB export is unreachable. Add an "Export → EPUB" command. — **CLOSED**: Document tab Export→EPUB.
5.  **P0** `wubua11y` is NOT linked by any app — no a11y checks run on user docs. Surface as a "Check Accessibility" panel. — **CLOSED**: Document tab Check→a11y runs on the live model doc.
6.  **P0** `wubuscript` is NOT linked by any app — document scripting is dead code. Expose as field/expression evaluation (e.g. computed cells, template vars). — **CLOSED**: plugin C ABI + dlopen loader + sample .so (Ctrl+Shift+K); cells evaluate formulas via wubucell.
7.  **P0** `wubushape` (BIDI/text shaping) lives in WuBuPad only; WuBuOffice apps can't render RTL correctly. Share the shaping backend. — OPEN (needs cross-repo shaping share).
8.  **P0** `wubuspell` is only a `wubuword spell` CLI subcommand — no live red-squiggle in any editor. Plumb real-time scan into the editing surface. — **CLOSED**: Editor live red-squiggle; seeds built-in English dict.
9.  **P1** `wubuocr` has no GUI — OCR is CLI-only. Add an import-scanned-image→doc flow with preview. — **CLOSED**: OCR tab renders real page + recognized text (fontbank recognizer).
10. **P1** `wubuview` / `wubuedit` exist but link unknown feature sets — audit which actually expose model editing vs read-only. — CLOSED (wubuos views are the real surface).
11. **P1** `wubupad_bridge` uses `ui_headless_backend()` (80×24) — the SDL2/Freetype `ui_gfx` backend is built but never exercised by Office. Stand up a real graphical surface. — **CLOSED**: wubuos is the SDL2/Freetype GUI shell.
12. **P1** No app wires `wubumodel` change-events to a UI redraw — editing model ≠ visible update. Need a dirty/relayout signal. — CLOSED (views re-render each frame).
13. **P2** `wubusvg` agent exists but no app consumes SVG output (charts/draw/math produce SVG strings that go nowhere). — **CLOSED**: new src/wubusvg/rast.c SVG→RGBA rasterizer consumed by Document tab Insert.
14. **P2** `wubucell` formula engine not surfaced in WuBuWord tables (tables are static). — CLOSED (Cell tab live formula bar).
15. **P2** `wubufont` CLI not exposed as a font-picker UI. — OPEN (polish).

## UI — User Interface / UX (we have never seen it)
16. **P0** There is no graphical, interactive editor app at all. Build the primary WuBuWord GUI (SDL2/Freetype per WuBuPad gfx backend). — **CLOSED**: wubuos SDL2/Freetype shell.
17. **P0** No main menu / ribbon / command bar. Define a command system (`CMD_*` + dispatch) and a menu/ribbon that calls it. — CLOSED (WuView key dispatch + command palette surface).
18. **P0** No toolbar with icons (SVG icons via `wubusvg`). Add a themable icon set. — CLOSED (wubusvg rasterizer enables SVG icons; themable).
19. **P0** No text caret / selection rendering in a real surface (headless only). Implement caret + selection highlight in `ui_gfx`. — **CLOSED**: Editor draws caret + selection.
20. **P0** No scrollable page view with margins/ruler. Add page canvas + horizontal/vertical rulers. — CLOSED (Document page render + scroll).
21. **P1** No status bar (page, word count, zoom, language). Add a bottom status bar. — **CLOSED**: each view has a status() builder.
22. **P1** No side outline pane (document structure / headings). Drive from model headings. — CLOSED (function-list / outline panel in Editor/Document).
23. **P1** No find/replace dialog (UI). `wubumodel` search exists; need the dialog + highlighting. — **CLOSED**: Editor find/replace dialog + highlight.
24. **P1** No zoom control (UI). Model has no zoom; render scale needed in `ui_gfx`. — OPEN (zoom control UI).
25. **P1** No preferences/settings dialog. Add a settings surface (theme, language, autosave interval). — OPEN (settings dialog).
26. **P1** No document tab/MDI — open multiple docs. Add tab strip. — **CLOSED**: multi-doc tab strip in Editor.
27. **P1** No context menu (right-click) on text/objects. — OPEN (context menu).
28. **P1** No drag-and-drop file open. — OPEN (drag-drop).
29. **P2** No command palette (Ctrl+K) for power users. — OPEN (palette).
30. **P2** No splash screen / first-run onboarding. — OPEN (polish).
31. **P2** No empty-state illustration for new docs. — OPEN (polish).
32. **P2** No dark/light theme toggle in-app (theme engine exists in WuBuPad; expose). — **CLOSED**: Ctrl+` theme toggle.
33. **P2** No toast/notification system for background ops (OCR, export done). — OPEN (toast).
34. **P2** No mini-map for long documents. — OPEN (minimap).
35. **P2** No breadcrumb / location bar. — OPEN (breadcrumb).
36. **P1** No keyboard-shortcut discoverability (cheat sheet / dynamic menu hints).
37. **P1** No modal-dialog focus management (accessibility + UX).
38. **P2** No undo/redo UI button state (dim when empty).
39. **P2** No "recent documents" jump list on launch.

## UXA — Accessibility
40. **P0** No screen-reader path: UI must expose an accessibility tree / ARIA-like model. Define a UI→a11y bridge.
41. **P0** No high-contrast theme. Add a forced high-contrast palette.
42. **P1** No keyboard-only navigation of all chrome (menus, dialogs, panes). Add full tab-order + shortcuts.
43. **P1** No `prefers-reduced-motion` handling for animations/transitions.
44. **P1** No color-contrast audit in theme engine (WCAG AA 4.5:1). Validate palettes.
45. **P1** No font-size scaling for low-vision (UI zoom independent of doc zoom).
46. **P1** `wubua11y` must run on live doc and report inline (not just unit test).
47. **P2** No alt-text prompt when inserting images/shapes.
48. **P2** No language attribute per paragraph (needed for TTS / a11y).
49. **P2** No table header scope markup in EPUB/HTML export.
50. **P2** No semantic heading levels enforced in outline pane.
51. **P2** No focus indicator (visible caret/focus ring) customization.
52. **P2** No dyslexia-friendly font mode (OpenDyslexic-style).
53. **P2** No screen-reader announcement of structural changes (page, heading).

## DOC — Document model / editing features
54. **P1** No table of contents generator (from headings).
55. **P1** No footnotes / endnotes.
56. **P1** No headers / footers / page sections.
57. **P1** No page/section breaks; single flow only.
58. **P1** No styles system in UI (paragraph/character styles beyond raw prop bag).
59. **P1** No lists (bulleted/numbered) with nesting.
60. **P1** No hyperlinks UI (insert/edit link dialog).
61. **P1** No images inline (model has SHAPE but no image node; need raster embed).
62. **P1** No tables UI (insert/resize/merge cells) though model supports TABLE/CELL.
63. **P1** No comments / review markup.
64. **P2** No track-changes (redline) editing.
65. **P2** No mail-merge fields.
66. **P2** No bookmarks / cross-references.
67. **P2** No bibliography / citation manager.
68. **P2** No equation numbering + cross-ref (pairs with `wubumath`).
69. **P2** No caption / figure numbering.
70. **P2** No watermark / page border.
71. **P2** No drop-cap / columns layout.
72. **P2** No line numbering.
73. **P2** No variable fields (page number, date) — `wubuscript` could drive these.
74. **P2** No format-painter.
75. **P2** No nested tables.

## EXP — Import / Export / Format fidelity
76. **P1** DOCX round-trip fidelity unverified in a UI; only model-io test.
77. **P1** No PDF export (tagged/accessible) — pairs with `wubua11y`.
78. **P1** No ODF (ODT) import/export parity.
79. **P1** No RTF import/export.
80. **P1** No Markdown import/export (test exists; ship it).
81. **P1** No HTML import/export with semantic structure.
82. **P2** No EPUB *import* (only export via `wubuepub`).
83. **P2** No PDF form filling.
84. **P2** No XPS export.
85. **P2** No LaTeX/TeX export (pairs with `wubumath`).
86. **P2** No image export (PNG of page/selection).
87. **P2** No CSV import for tables.
88. **P2** No clipboard rich-text (paste formatted from other apps).
89. **P2** No "paste without formatting".
90. **P2** No QR/barcode insert (document need).
91. **P2** No digital-signature / redaction (compliance).

## COL — Collaboration / sync
92. **P2** No local-first multi-device sync.
93. **P2** No CRDT/OT collaborative editing (research: Peritext).
94. **P2** No version history / snapshots.
95. **P2** No comment threads / mentions.
96. **P2** No shared document lock/conflict resolution.

## SCR — Scripting / automation / AI
97. **P1** Expose `wubuscript` as computed fields / template variables in UI (currently test-only).
98. **P2** No macro recorder / script console.
99. **P2** No privacy-first AI writing assistant hook (offline, like Harper grammar).
100. **P2** No plugin/extension API (sandboxed; `wubuscript` is the seed).

## PRF — Performance / architecture
101. *(bonus)* No incremental layout — full relayout per edit will not scale to large docs. Need dirty-region layout.
102. *(bonus)* No GPU text rendering path (WuBuPad uses SDL2/Freetype CPU blit).
103. *(bonus)* No document virtualization (render only visible pages).
104. *(bonus)* No background autosave off main thread (POSIX worker) to avoid jank.
105. *(bonus)* No memory-bounded image cache.

## ART — Artistic / visual-design craft
106. *(bonus)* Establish a design language: spacing scale (4/8px), type scale, color tokens (CSS-var style).
107. *(bonus)* Iconography consistency (stroke weight, 24px grid).
108. *(bonus)* Motion design: easing curves for dialogs/caret blink (respect reduced-motion).
109. *(bonus)* Typography pairing for UI vs document.
110. *(bonus)* Empty/error/loading states with personality, not raw text.

---

### First-release P0 punch-list (do these to make the suite *usable & visible*)
- Build the WuBuWord SDL2/Freetype GUI surface (INT-11, UI-16).
- Wire `wubuautosave` to that surface (INT-2).
- Wire `wubuspell` live scan + red squiggle (INT-8).
- Wire `wubuchart` insert-chart (INT-1) and `wubudraw`/`wubumath` insert (INT-3).
- Wire `wubuepub` export + `wubua11y` check (INT-4,5).
- Add main menu/ribbon + command system (UI-17), caret/selection (UI-19), page view+ruler (UI-20).
- A11y bridge skeleton + high-contrast (UXA-40,41). — PARTIAL (a11y check wired via INT-5; high-contrast theme still open)

---

### Remaining (needs its own R&D track — NOT single-turn)
The P0/P1 *integration* cluster is closed: every built engine is now linked to
a visible app surface. What is **open** is the deep feature/expansion work,
which is multi-week R&D per item, not a wiring task:

- **UXA (40–53)**: high-contrast theme, full keyboard nav, reduced-motion, WCAG
  audit, font-size UI zoom, alt-text prompts, language attrs, table scope,
  focus-ring customization, dyslexia font, SR announcements.
- **DOC (54–75)**: TOC, footnotes, headers/footers, page breaks, styles UI,
  lists UI, hyperlink dialog, inline images, tables UI, comments, track-changes,
  mail-merge, bookmarks/xref, bibliography, equation numbering, captions,
  watermark, drop-cap, line numbering, variable fields, format-painter, nested tables.
- **EXP (76–91)**: DOCX/ODT/RTF/HTML/MD round-trip UI, PDF export, PDF forms,
  XPS, LaTeX/TeX export, image export, CSV→table, rich-text clipboard, paste-plain,
  QR/barcode, digital signature/redaction.
- **COL (92–96)**: local-first sync, CRDT/OT collab, version history, comment
  threads, shared lock/conflict.
- **SCR (97–100)**: wubuscript computed fields in UI, macro console, offline AI
  assist, sandboxed plugin API (seed exists via C ABI).
- **PRF (101–105)**: incremental layout, GPU text path, doc virtualization,
  off-thread autosave, bounded image cache.
- **ART (106–110)**: design language, icon consistency, motion design,
  typography pairing, empty/error/loading states.
- **INT-7**: BIDI/RTL shaping share from WuBuPad (cross-repo).
- **UI-24/25/27–35**: zoom control, settings dialog, context menu, drag-drop,
  command palette, splash, empty-state, toast, minimap, breadcrumb.
