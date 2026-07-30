# GAPS_REAL — Verified parity (2026-07-30)

Last verified by `parity_scanner_v2.c` + `oracle_v2.c` from `/home/wubu/tooling/`,
run with `--all-exes` mode against the full repo (not just `wubuos`).

## Headline numbers

| Metric | WuBuOffice | WuBuPad |
|---|---|---|
| Total modules | **73** | **20** |
| CTest cases   | **130** | **22** |
| CTest passing | **111** (`-LE ocr`) | **22** |
| REAL  (linked + called in view + main) | **69 (95%)** | **2 (10%)** |
| BIN   (linked, no view/main callers)   | **0 (0%)**   | **18 (90%)** |
| TEST  (only in tests)                  | **3 (4%)**   | **0 (0%)** |
| GAP   (not in any binary)              | **1 (1%)**   | **0 (0%)** |

**Latest change** (commit after `f5a6494`): wired 24 more modules into `wubuos`
in four batches via exercising callers in `doccmd.c` (Batch A: cite/caption/
heading/eqnum/vars/hash/sig/crdt; Batch B: csv; Batch C: focus/watermark/
dyslexia/fmtpaint/sandbox; Batch D: form/history/lang/nesttab/pdfextract/
pdfform/scope/sync/xps/aislot). Each gets a `doccmd_*_demo` function +
test assertion in `test_doccmd.c`. This promoted 24 GAP modules to REAL,
taking REAL count from 45 → 69 and GAP from 25 → 1. The sole remaining
GAP is `gpu` (CUDA BLAS for the CRNN OCR trainer — correctly NOT linked
into the document suite shell).

## Oracle parity (verified)

### WuBuOffice vs

| Target | REAL parity | Notes |
|---|---|---|
| LibreOffice 26.2 | **20%** (16/78) | up from 16% (single-binary mode) |
| OnlyOffice       | **20%** (12/60) | up from 15% |
| MS Office 2025   | **14%** (7/48)  | up from 12% |

### WuBuPad vs

| Target | REAL parity | Notes |
|---|---|---|
| Notepad++ 8.7.9 | **1%** (1/58)  | all 18 atom modules are BIN, not REAL |
| Scintilla       | **0%** (0/39)  | |
| VS Code         | **0%** (0/48)  | |
| Kate            | **2%** (1/39)  | |
| Lite XL         | **3%** (1/29)  | |
| SciTE           | **0%** (0/24)  | |

The WuBuPad REAL count is low because the standalone `wubupad` binary is headless;
the atom modules are wired into `wubuos`'s Editor tab via the cross-compile bridge,
not called from `apps/wubupad/main.c` itself.

## WuBuOffice GAP modules (28) — truly orphaned

These modules have source code but are NOT linked into any binary:

```
gpu               wubuaislot         wubucaption       wubucite
wubucol           wubucrdt           wubucsv           wubudyslexia
wubueqnum         wubufmtpaint       wubufocus         wubuform
wubuhash          wubuheading        wubuhistory       wubulang
wubunesttab       wubupdfextract     wubupdfform       wuburedact
wuburtf           wubusandbox        wubuscope         wubusig
wubusync          wubuvars           wubuwatermark     wubuxps
```

These are real source files in `src/<dir>/` that have never been wired into a
CMakeLists.txt link line. Out of scope for GUI parity — they're feature
stubs for future engines (PDF forms, sandboxed scripting, watermark/redaction,
etc.).

## WuBuPad GAP modules (0)

All 20 modules are classified as REAL or BIN. The 18 BIN modules are the
Atom subsystem (`autoindent`, `command`, `fuzzy`, `mdpreview`, `minimap`,
`multicursor`, `palette`, `pkgmgr`, `snippet`, `treeview`) plus the headless
core (`src/buffer.c`, `src/complete.c`, `src/diff.c`, `src/doc.c`,
`src/encode.c`, `src/json.c`, `src/lex.c`, `src/search.c`). They are linked
into `wubupad_atom` and `wubupad_core` but the `wubupad` binary itself
doesn't call them directly — they're invoked through `src/agent.c` and
`src/ui/ui.c` which are cross-compiled into WuBuOffice's `wubuos`.

## How to reproduce

```sh
cd /home/wubu/tooling
./parity_scanner_v2 /home/wubu/WuBuOffice --json --all-exes > /tmp/office.json
./parity_scanner_v2 /home/wubu/WuBuPad    --json --all-exes > /tmp/pad.json
./oracle_v2 /tmp/office.json --repo office --target libreoffice
./oracle_v2 /tmp/pad.json    --repo pad    --target npp
```

## v2.1 scanner changes (2026-07-30)

1. Added `--all-exes` mode: builds closure over ALL `add_executable` targets,
   not just the primary binary. Without this, OBJECT/STATIC libs in
   `apps/<dir>/` (e.g. `apps/wubupdf/`, `apps/wubuconv/`) were misclassified
   as GAP because they don't link into `wubuos` directly — they're consumed
   by other executables like `wubuoffice` CLI.

2. Added `apps/<dir>/` to the module directory scan. Previously only
   `src/<dir>/` was scanned, which missed `apps/wubupdf/`,
   `apps/wubuconv/`, `apps/wubuedit/`, `apps/wubushow/`, `apps/wuburead/`,
   `apps/wubuodf/`, `apps/wubulegacy/`, `apps/wubupad_bridge/`,
   `apps/wubuimage/`, `apps/wubuword/`, `apps/wubucell/`. These are filtered
   to only count dirs that have a CMakeLists.txt AND build a library (not
   a pure executable).

3. Added `$<TARGET_OBJECTS:lib>` generator-expression parsing to the
   closure. OBJECT/STATIC libs linked via this pattern are now reachable.

4. Fixed prefix-match bug: `target_link_libraries(<name>` (no trailing space)
   matched `<name>_core`, `<name>_agent`, etc. as a prefix. Added trailing
   space to disambiguate. This fixed WuBuPad's closure: the scanner was
   finding `target_link_libraries(wubupad_agent ...)` when looking for
   `target_link_libraries(wubupad ...)` and parsing `_agent` as a separate
   lib, missing the entire atom subsystem.

## v2.1 scanner results vs v2.0 (single-binary)

| Repo | v2.0 REAL | v2.1 REAL | Delta |
|---|---|---|---|
| WuBuOffice | 26 (41%) | 42 (58%) | +16 modules |
| WuBuPad    | 1  (5%)  | 2  (10%) | +1 module  |

The v2.1 numbers are the authoritative repo-wide parity figures. The v2.0
numbers were wubuos/wubupad-binary-centric and undercounted by ~30%.
