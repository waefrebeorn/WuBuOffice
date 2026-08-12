# GUI Research Synthesis — 25 searches, 5 Kevin-Bacon hops (2026-08-12)

Grounds GUI improvements for WuBuPad + WuBuOffice in online research.

## What the research confirms we ALREADY got right
- **Piece table buffer** (WuBuPad `src/buffer.c`): the research chain
  (0xkiire, coredumped, vibeengines) confirms piece table = the industry-proven
  choice (MS Word, VS Code piece-tree, Scintilla piece-chain). Fast local edits,
  cheap undo/redo, handles large files. KEEP.
- **Glyph atlas + batched GPU text** (osor.io, Warp, SDL discourse): per-glyph
  cached textures = the standard for crisp, fast text. WuBuPad `ui_gfx.c` does
  this (glyph cache + shape). KEEP.
- **HarfBuzz shaping** (GitHub, tchayen, SDL2_ttf docs): already integrated for
  complex scripts. KEEP.
- **Virtualization/windowing** (dev.to, openreplay, Sunil Band): render only
  visible rows + small buffer. Both editors already viewport-render. KEEP.
- **Semantic color tokens** (imperavi, Contentful): WuBuOffice `WUOS_DARK_*`
  macros + WuBuPad `UIToken` enum = the semantic-token pattern. KEEP.
- **Command palette / command bar** (Knock, VS Code): WuBuPad has one. KEEP.

## Highest-value NEW improvements (from research, implementable, verifiable)

### 1. Menu accelerator + shortcut display (NN/g, Knock, Software AG)
Research is unambiguous: shortcuts must be DISCOVERABLE. Menus should show the
accelerator (Ctrl+O etc.) on the right of each item, and a shortcuts reference
must exist. WuBuOffice has a "Shortcuts" help menu but the dropdowns do NOT
show accelerators. FIX: append `\t<shortcut>` to menu item labels.

### 2. Font-size-scaled chrome layout (Haiku DPI pattern, ArchWiki HiDPI)
Haiku scales ALL UI (menus, toolbars, tabs, spacing) from the base font pixel
size, not fixed pixel constants. WuBuOffice hardcodes TAB_H=30, MENU_H=24,
TOOLBAR_H=26. FIX: derive chrome heights from `wuos_font_height()` so zoom/DPI
scales the whole chrome consistently. Makes zoom genuinely useful.

### 3. Indent guides + whitespace viz (Notepad++ View menu)
Notepad++ shows dotted vertical indent guides and space/tab marks. Both are
pure render overlays over the glyph grid (no ref engine). Small, self-contained,
high-visibility. The top two "real" gaps from GAPS_NOTEPAD.md.

### 4. WCAG contrast audit baked into the tricorder
Research confirms WCAG AA = 4.5:1 normal text, 3:1 large text; dark mode needs
testing per color, not assumptions. The tricorder's `LOWCON` (cr<3.0) is the
floor; raise the gate for body text to 4.5:1. Both palettes already hit 4.6:1+
for primary text — the gate makes it permanent.

### 5. Shortcut discoverability via first-run + context hints
NN/g: surface shortcuts after the user performs the action. The first-run tour
(toast) already exists in WuBuOffice; extend it to name the 8 core shortcuts.
Low cost.

## Not worth doing (research says avoid)
- Coding-font LIGATURES for the core editor (betterwebtype: "ligatures in
  programming fonts are a terrible idea" for strict code reading). Skip.
- Rebuilding the buffer as a rope/gap (piece table is already optimal). Skip.
- A full MS Office ribbon (huge; the flat toolbar + menu is adequate for now).

## Priority
1. Menu accelerators (discoverability) — both apps, quick, verifiable by tricorder.
2. Font-size-scaled chrome (DPI/zoom correctness) — WuBuOffice.
3. Indent guides + whitespace viz — WuBuPad.
4. Raise tricorder LOWCON to 4.5:1 body-text gate.
