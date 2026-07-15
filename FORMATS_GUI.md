# WuBuGUI — native terminal human interface (`wubutui` + `wubuview`)

The whole WuBuOffice suite was headless: NDJSON agent buses + CLIs, model-driven,
zero-dependency. This adds the **human-facing** layer without breaking that
ethos — a from-scratch terminal UI in pure C11 + POSIX. No Electron, no toolkit,
no ncurses, no dependency. Works in WSL, over SSH, anywhere there's a terminal.

## `wubutui` — native TUI toolkit (`src/wubutui/`)

A clean split between a **pure, unit-testable core** and a **thin impure edge**:

| Module | Role | Pure? |
|--------|------|-------|
| `screen.{h,c}` | opaque WxH cell grid; put/get/clear; plain-text dump; ANSI renderer with **minimal diff repaint** | ✅ pure (no TTY) |
| `input.{h,c}` | byte-stream → key events: ASCII, CSI/SS3 arrows, PgUp/Dn, Home/End, Ins/Del; reports INCOMPLETE for split escape sequences | ✅ pure |
| `draw.{h,c}` | text / hline / vline / box / fill; **pure word-wrap** (space-break + long-word hard-break + explicit newlines) and scrolled wrapped-text draw | ✅ pure |
| `term.{h,c}` | raw termios enter/leave, alt-screen, cursor hide, size query, diffed present, input read | ⚠️ POSIX edge |

Because the render and input paths never touch a file descriptor, the entire
logic is tested by feeding byte strings and inspecting screen dumps — no TTY
required. `term.c` isolates the only impure code (soul.md: isolate the edge).

The renderer keeps a previous-frame copy and emits only changed cells, so
scrolling repaints are cheap (no full-screen flicker).

## `wubuview` — human document viewer (`apps/wubuview/`)

Opens **any format `wubudoc` ingests** (txt/md/html/csv/json/svg/xml,
docx/xlsx/pptx, odt/ods/odp, doc/xls/ppt, fonts, zip) in a scrollable,
word-wrapped, paginated terminal window.

Pipeline: `wubudoc ingest → normalized JSON model → docflat → text → wubutui`.

- `docflat.{h,c}` — **pure** model→text flattener. Unwraps the `{"model":{...}}`
  envelope `doc_json()` produces, then renders document blocks (paragraphs, with
  headings underlined — detected by `kind` OR `style` e.g. `Heading1`), sheet
  rows (tab-separated), and text wrappers. Unrecognized models fall back to
  compact JSON so nothing is ever hidden or fabricated.
- Keys: `j`/`k` or ↑/↓ scroll · `Space`/`b` or PgDn/PgUp page · `g`/`G` or
  Home/End jump to ends · `q`/`Esc` quit. Header shows the filename; footer shows
  position (`line-line/total (pct%)`).

- **Mouse (old-school xterm reporting, SGR 1006 + legacy X10):**
  - **wheel** scrolls by 3 lines; 
  - **scrollbar** on the right edge: click to jump, **drag the thumb** to scrub;
  - **footer buttons** `[Top] [PgUp] [PgDn] [Bot] [Quit]` are clickable.
  `term.c` enables `1000`+`1002`+`1006` on enter and disables on leave.

- `--dump` prints the flattened text and exits — scriptable and testable without
  a TTY. Non-TTY interactive launch fails gracefully pointing at `--dump`.

The interactive event→state mapping lives in a **pure controller**
(`apps/wubuview/controller.{h,c}`, no TTY/globals) so the real glue is
unit-tested directly — `main.c` is a thin driver that only reads bytes and
paints. Hit-box math (footer buttons, scrollbar) is shared by draw + tests.

Verified end-to-end: `notes.md → (wubudoc create) → notes.docx → wubuview`
round-trips through the binary format and renders as clean text with underlined
headings. Controller event simulation confirms scrollbar-click jumps to mid, wheel
adjusts, and footer Bot/Top buttons drive scroll to end/top.

## Tests & gates

- `test_wubutui` — screen put/get/dump, box, word-wrap (space/hard/newline),
  scrolled draw, **full mouse-decode matrix** (SGR 1006 press/wheel/release/
  drag/shift, legacy X10, incomplete→re-feed), scrollbar thumb math, button
  hit-test, ANSI render + diff.
- `test_wubuview` — docflat over doc model, `model`-wrapped envelope, style-based
  headings, sheet rows, object cells, unknown-model JSON fallback, non-JSON echo,
  NULL safety.
- `test_wubuview_ctrl` — the **real** event→state controller: keyboard scroll,
  wheel clamping, scrollbar click-jump + drag-scrub, footer button clicks
  (Top/PgUp/PgDn/Bot/Quit), quit, and resize-clamp.
- Gates: **34/34 normal + 34/34 sanitizer (ASan+UBSan), 0 warnings/leaks/UB.**

Design follows the suite standard: opaque structs, minimal includes, C11 only,
self-contained modules, reuse-never-duplicate (JSON via `wubujson`; ingestion via
the `wubudoc` facade — the viewer implements no format parsing of its own).
