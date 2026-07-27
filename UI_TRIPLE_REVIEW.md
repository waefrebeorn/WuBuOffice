# WuBuOffice — Triple Devil's-Advocate UI Review
*(vision-model critique + architectural critique + artistic critique, informed by RESEARCH_GAPS_100.md)*

You have built a powerful *engine* but have never seen it. The research + audit
prove the suite is currently **headless libs + orphaned tests + one 80×24
headless bridge**. Below are three independent hostile reviews and a merged
action plan. Everything is framed as "what will make a user close this app in
under 10 seconds."

---

## 1) VISION-MODEL CRITIQUE (rendered mockup of target WuBuWord GUI)
Source: generated target mockup, critiqued by the vision model.

**What it nailed (keep):** calm blue accent, 8px-ish grid feel, centered page
on gray canvas, ribbon grouping, live word-count, accessibility panel present.
This is a credible *direction*.

**Brutal findings (verbatim themes):**
- **Identity crisis / chrome dominance.** Product name too loud; sidebars +
  ribbon consume ~60% of viewport; the *document* (the reason to open the app)
  is the afterthought. Eye never lands on content.
- **Dual sidebar cannibalism.** Left outline + right a11y panel eat 35% width.
  On 1920px that's 672px of chrome. The mockup even populated the outline with
  nonsense words — a real bug risk (GAP DOC-54 outline must come from *real*
  headings, not random data).
- **Icon-only ribbon, no labels/ARIA.** Screen-reader users hear "Button,
  Button, Button." No accessible names on custom icons (Equation, Math). This
  directly violates UXA-40/42.
- **Contrast failure on the a11y panel itself** — ironic. Light-gray 8pt text
  on white ≈ 2:1, fails WCAG AA 4.5:1 (UXA-44). The checker must not be the
  least-accessible thing in the app.
- **Chart color-blindness.** Blue bars + red line invisible to 8% of males
  (deutan). Needs dashed line / distinct hue + a data table for SR (GAP CHART).
- **No focus rings anywhere** (UXA-51). Keyboard nav is invisible.
- **Unfinished signals:** search field truncated "word cour", chart has no
  title/legend/axis labels, mixed-language placeholder text. Ships "broken".

**Vision verdict:** direction is good; must collapse chrome, label icons,
fix contrast, add focus, and make the document the hero.

---

## 2) ARCHITECTURAL CRITIQUE (how the code will betray the UI)
As the engineer who will build this, here is where the current architecture
will make the UI fail or rot:

- **A1 (showstopper) — No UI↔model binding.** `wubumodel` is mutated by tests
  but nothing redraws on edit. We need a dirty/relayout signal (INT-12). Without
  it, the first WuBuWord build will show a static page that ignores typing.
- **A2 — Headless-first rot.** Every lib was written test-first with no app
  consumer. The moment we attach a UI, we'll find missing entry points
  (e.g. `wubuepub` takes a doc but has no "insert chart as OEBPS image"
  path; `wubuautosave` needs a lifecycle owner). Wire one app NOW or the libs
  drift from reality.
- **A3 — Immediate-mode vs retained.** WuBuPad's `ui.h` is a retained vtable
  (draw_line/caret/present). Good. But there is no *layout* layer between model
  and draw calls. We'll reinvent pagination per feature. Build a `Layout` pass
  (model → boxes → pages) once, reused by screen + PDF + EPUB.
- **A4 — Single-thread autosave will jank** (PRF-104). `wubuautosave` writes
  synchronously; on a big doc the UI freezes. Must move to a POSIX worker /
  dirty-flag polled off the event loop.
- **A5 — No incremental layout** (PRF-101). Full relayout per keystroke kills
  large docs. Dirty-region layout required before shipping to real users.
- **A6 — Shaping not shared** (INT-7). RTL/BIDI lives in WuBuPad; Office apps
  will render Arabic/Hebrew wrong. The shaping backend must be a shared lib
  both repos link.
- **A7 — Clipboard/IME gap.** No rich-text paste, no IME composition hook for
  CJK. A "word processor" without IME is dead on arrival in half the world
  (GAP EXP-88, EXP-89).
- **A8 — Command system missing** (UI-17). Menus/ribbon/shortcuts/command-palette
  all need a single `CMD_*` dispatch. Build it before any chrome.
- **A9 — No document virtualization** (PRF-103). Rendering all pages at once
  bounds memory; need visible-page-only render.

---

## 3) ARTISTIC CRITIQUE (the craft that separates "tool" from "product")
- **B1 — Establish a real design language.** Spacing scale (4/8/12/16/24), a
  type scale (12/14/16/20/28), and *named color tokens* (not hex scattered in
  code). WuBuPad has a theme engine — promote it to a token system (ART-106).
- **B2 — Icon discipline.** One stroke weight, 24px grid, consistent metaphor.
  Generate the icon set as SVG via `wubusvg` so it's themeable + ARIA-labelable
  (ART-107). Icon-only buttons still need aria-label + tooltip.
- **B3 — Motion with restraint.** Caret blink, dialog ease-in (200ms
  cubic-bezier), but honor `prefers-reduced-motion` (UXA-43, ART-108).
- **B4 — The document is the hero.** Chrome should recede: collapsed ribbon by
  default, single contextual sidebar (outline) that can hide, a11y as a *bottom
  drawer* not a permanent right pane (fixes vision's "cannibalism").
- **B5 — Typography pairing.** UI font (clean sans) distinct from document
  font (a readable serif/sans the user chooses). Don't let UI chrome compete
  with the writing.
- **B6 — Empty/error/loading states with soul** (ART-110). A new doc should
  show a gentle prompt ("Start writing, or press / for commands"), not a blank
  gray void. Crash-recovery prompt must be calm, not a modal scream.
- **B7 — Contrast as a first-class token.** Every palette must pass AA; ship a
  verified high-contrast theme (UXA-41/44).

---

## MERGED P0 ACTION PLAN (do in this order)
1. **Command system** (`CMD_*` + dispatch + shortcuts) — A8, UI-17.
2. **WuBuWord SDL2/Freetype GUI surface** (reuse WuBuPad `ui_gfx`) — UI-16, INT-11.
3. **Model→Layout→Render pipeline** with dirty signal — A1, A3, PRF-101.
4. **Wire the orphaned libs into that surface:**
   - `wubuautosave` lifecycle (flush on close, recover on crash) — INT-2, A4.
   - `wubuspell` live scan + red squiggle — INT-8.
   - `wubuchart` insert-chart (with title/legend/axis + SR data table) — INT-1.
   - `wubudraw`/`wubumath` insert shape/equation — INT-3.
   - `wubuepub` export + `wubua11y` checker (bottom drawer) — INT-4,5.
5. **Chrome done right:** collapsed ribbon, single hideable outline sidebar,
   a11y as bottom drawer, status bar, labeled+ARIA icons, focus rings,
   high-contrast theme — vision + UXA findings.
6. **Shared shaping lib** for RTL/BIDI across repos — A6, INT-7.
7. **IME + rich clipboard** before calling it a word processor — A7.

## WHAT WE WILL SHIP FIRST (the "make it real" milestone)
A WuBuWord that: opens a doc in a real window, shows a caret + can type
(redraws!), auto-saves + recovers, underlines misspellings, inserts a chart,
exports EPUB, and runs an accessibility check — all behind a collapsed ribbon
with labeled icons, focus rings, and a high-contrast theme. That single
milestone turns 6 orphaned libraries into a *visible product* and closes the
user's "I've never seen any of this" gap.
