# WuBuOffice — Modularization Plan

Two wastes we are fixing:
1. **Re-running the whole suite** (80 tests + a ~10-min OCR/CNN battery) on every
   change, even when only one module moved.
2. **Duplicated utilities** copy-pasted across modules (crc32, utf8, xml-escape,
   PNG writers, dynamic string buffers) — each is a maintenance tax and a place
   for silent divergence (we just hit this: `wubuwordview` reimplemented crc32
   and fed FreeType raw UTF-8 bytes → mojibake).

---

## A. Build / smoke-test modularization (DONE)

Every test is now tagged with a module **LABEL** (see `tests/CMakeLists.txt`
"MODULAR TEST LABELS" block and `tools/CMakeLists.txt`). The slow OCR/CNN
tests are all labelled `ocr` so they are excluded from default smoke.

Run only what you touched:
```
./tools/smoke.sh                 # fast: all modules EXCEPT ocr
./tools/smoke.sh spell           # one module
./tools/smoke.sh chart math draw # several modules
./tools/smoke.sh ocr             # the full (slow) OCR/CNN battery
./tools/smoke.sh all             # literally everything (long)
cmake --build build --target smoke   # equivalent to ./tools/smoke.sh
```
Under the hood these call `ctest -L <re>` / `ctest -LE ocr`. Add a label to any
new test with `set_property(TEST <name> PROPERTY LABELS <module>)`.

**Rule of thumb:** on a normal change run `./tools/smoke.sh <module>`; run
`./tools/smoke.sh` (fast) before commit; run `./tools/smoke.sh all` only in CI
or before a release tag. Never hand-run the whole suite to check a one-line fix.

---

## B. Code-reuse survey → internal shared deps (TODO, high value)

A quick grep of the tree found the same utilities reinvented in 3–6 places each.
Plan: extract each into a small, dependency-free, opaque-struct lib under
`src/wububase/` (or its own `src/wubu*` dir), then have the current copies
#include it. This breaks the monolith-lite habit and gives one tested copy.

| Utility | Found in | Proposed shared lib |
|---------|----------|---------------------|
| `crc32` | `wubuzip/crc.c`, `wubuwordview/main.c`, `wubuocr/png*.c` | `wububase` (crc32) |
| UTF-8 decode | `wubuwordview` (utf8_next), `wubuocr/lexicon.c`, `ocr_render.h`, `wubulegacy` | `wububase` (utf8_next) |
| dynamic string buffer (`Buf`/`buf_append`/`s_catf`) | `wubuchart`, `wubudraw`, `wubumath`, `wubuepub`, `wubuodf`, `wubuwordview` | `wububase` (Buf) |
| `xml_esc` / HTML escape | `wubuepub/epub.c`, `wubucell`, `wubushow`, `wubuxml` | `wububase` (xml_esc) |
| PNG writer | `wubuwordview/main.c`, `wubuocr/png_encode.c` | `wubupng` |
| base64 | `wubudoc/b64.c` (+maybe elsewhere) | `wububase` (b64) |

Extraction order (lowest risk first):
1. **`wububase`** = `utf8_next` + `crc32` + `Buf` + `xml_esc` + `b64`. These are
   tiny, pure, and used almost everywhere. Land it, then point
   `wubuwordview`/`wubuchart`/`wubuepub` at it (deletes ~200 duplicated lines
   and the mojibake class of bug).
2. **`wubupng`** = one correct PNG encoder (zlib-backed, chunk-framing correct —
   the `wubuwordview` one had a chunk-length bug we already fixed; promote that
   fixed version). `wubuocr/png_encode.c` reuses it.

Each extraction is its own commit with a test (e.g. `test_base` round-trips
utf8/crc/buf). No behavior change to callers beyond deleting their private copy.

---

## C. Breaking up monolith-ish modules (TODO)

- `apps/wubuword/`, `apps/wubucell/`, `apps/wubushow/` share `wubuoxml` +
  `*_app` OBJECT libs; that layering is good. Keep it, but route shared helpers
  (xml_esc, buf) through `wububase` instead of per-app copies.
- `src/wubuocr` is large (convnet3, crnn, image ops, pdfua, docfmt). The OCR
  *tests* are already isolated under the `ocr` label; consider splitting the
  heavy CNN training smoke (`test_convnet3`) into its own `ctest` exclude group
  so it only runs in CI, not local smoke.

---

### Immediate next step
Run `./tools/smoke.sh <module>` for the module you just edited. Stop running the
whole suite by hand. Then land `wububase` (item B.1) to kill the crc32/utf8/buf
duplication — that alone removes the most likely source of silent divergence
bugs.
