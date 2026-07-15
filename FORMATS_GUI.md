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
- `--dump` prints the flattened text and exits — scriptable and testable without
  a TTY. Non-TTY interactive launch fails gracefully pointing at `--dump`.

Verified end-to-end: `notes.md → (wubudoc create) → notes.docx → wubuview`
round-trips through the binary format and renders as clean text with underlined
headings.

## Tests & gates

- `test_wubutui` — screen put/get/dump, box, word-wrap (space/hard/newline),
  scrolled draw, full key-decode matrix, ANSI render + diff.
- `test_wubuview` — docflat over doc model, `model`-wrapped envelope, style-based
  headings, sheet rows, object cells, unknown-model JSON fallback, non-JSON echo,
  NULL safety.
- Gates: **33/33 normal + 33/33 sanitizer (ASan+UBSan), 0 warnings/leaks/UB.**

Design follows the suite standard: opaque structs, minimal includes, C11 only,
self-contained modules, reuse-never-duplicate (JSON via `wubujson`; ingestion via
the `wubudoc` facade — the viewer implements no format parsing of its own).
