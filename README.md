# WuBuOffice — C11 from-scratch office suite

A ground-up, fork-free, dependency-free reimplementation of the
Office Open XML formats (`.docx` / `.xlsx` / `.pptx`) and the full
OpenDocument family, built from the published ECMA-376 / ISO 29500
specifications in strict C11.

**SLERM** (verb): take someone's full work and build your own
version from scratch — not a fork, not a vendored copy, not a
thin wrapper. Every byte is written by us, zero third-party code
compiled or linked.

## Table of contents

1. [Quick start](#quick-start)
2. [Format coverage](#format-coverage)
3. [Architecture](#architecture)
4. [Build](#build)
5. [Usage](#usage)
6. [Design rules](#design-rules)
7. [What's in the docs](#whats-in-the-docs)
8. [Project status](#project-status)
9. [License](#license)

## Quick start

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

The `wubuoffice` unified CLI dispatches to every engine:

```sh
wubuoffice word   out.docx              # rich .docx
wubuoffice cell   out.xlsx              # workbook (shared strings + formula)
wubuoffice show   out.pptx              # slide deck
wubuoffice read   file.docx             # dump parts + extract text
wubuoffice edit   in.docx out.docx      # round-trip re-write
wubuoffice convert in.xlsx out.odt      # any format → any other
wubuoffice convert legacy.xls out.csv   # legacy binary → CSV
wubuoffice convert report.docx out.pdf  # PDF out (fixed layout)
```

## Format coverage

### Tier 0 — OOXML (the crown jewels)
| Format | Read | Write | Round-trip | Validation |
|--------|:----:|:-----:|:----------:|------------|
| WordprocessingML (`.docx`) | ✅ | ✅ | ✅ | pypdf + python-docx |
| SpreadsheetML (`.xlsx`) | ✅ | ✅ | ✅ | openpyxl |
| PresentationML (`.pptx`) | ✅ | ✅ | ✅ | python-pptx |

### Tier 1 — Plain-text & interchange
| Format | Read | Write | Notes |
|--------|:----:|:-----:|-------|
| CSV / TSV | ✅ | ✅ | RFC 4180: quoting, embedded commas/newlines |
| Markdown | ✅ | ✅ | Headings, bold, tables → doc model |
| HTML | ⬜ | ✅ | Semantic HTML5 from model |
| RTF | ⬜ | ✅ | RTF 1.x from model |
| JSON | ⬜ | ✅ | Lossless dump of all three models |
| EPUB | ⬜ | ✅ | EPUB 3 from doc model; OEBPS + NCX |

### Tier 2 — OpenDocument (ISO/IEC 26300)
| Format | Read | Write | Validation |
|--------|:----:|:-----:|------------|
| ODT / FODT | ✅ | ✅ | odfpy / stdlib XML |
| ODS / FODS | ✅ | ✅ | odfpy |
| ODP / FODP | ✅ | ✅ | odfpy |

### Tier 3 — Legacy binary (MS-CFB / BIFF)
| Format | Read | Notes |
|--------|:----:|-------|
| `.doc` (BIFF8 Word) | ✅ | FIB + piece table, validated by xlrd |
| `.xls` (BIFF8 Excel) | ✅ | BIFF8 parser, validated by xlrd/xlwt |
| `.ppt` (PPT97) | ✅ | Record atoms + atom tree |

### Other
| Format | Description |
|--------|-------------|
| PDF 1.7 | Writer over the doc model (pypdf + pdfminer validated) |
| QR codec | Byte-mode QR ECC (GF(256) Reed-Solomon, Berlekamp-Massey + Chien + Forney) |

All three office writers emit files that open in **Microsoft Word/Excel/PowerPoint**
and **LibreOffice**. Readers decode real deflate-compressed (method 8) parts,
not just store-mode files.

## Architecture

```
src/wubuzip/        ZIP container + DEFLATE (bit/huffman/fixed/block/inflate/reader+writer)
src/wubuxml/        streaming, well-formed XML writer with correct escaping
src/wubuoxml/       Open Packaging Conventions (OPC) — writer + reader
src/wubucfb/        MS-CFB / OLE2 compound-file container reader (legacy substrate)
src/wubuword/       WordprocessingML model + .docx assembler
src/wubucell/       SpreadsheetML model + .xlsx assembler (formulas + styles)
src/wubushow/       PresentationML model + .pptx assembler
src/wuburead/       OPC reader — dispatches by part type, extracts text/structure
src/wubuedit/       Structure-preserving round-trip (parses → model → re-emits)
src/wubudoc/        Markdown (read+write), HTML/RTF/JSON export, EPUB 3 writer
src/wubuodf/        OpenDocument read+write (.odt/.ods/.odp + flat .fodt/.fods/.fodp)
src/wubupdf/        PDF 1.7 writer over the doc model
src/wubuconv/       Unified conversion across all formats
src/wubulegacy/     Legacy binary readers (.xls/.doc/.ppt) over MS-CFB
src/wubuqr/         QR codec (ECC-M, v1..7) — Reed-Solomon GF(256)
src/wubusvg/        SVG ingest/regurgitate + rasterizer
src/wububase/       Shared utilities (utf8_next, crc32, Buf, xml_escape, b64)
src/wubuimage/      Raster image pipeline (PNG, JPEG via ffmpeg, Netpbm, EXIF)
src/wubuocr/        OCR pipeline (CRNN+CTC, QR code, fontbank recognizer)
src/wubuformula/    Formula engine (shared by wubucell + wubuword tables)
apps/wubuoffice/    Unified CLI dispatch (word/cell/show/read/edit/convert)
apps/wubuword/      WordprocessingML CLI + standalone .docx assembler
apps/wubucell/      SpreadsheetML CLI + standalone .xlsx assembler
apps/wubushow/      PresentationML CLI + standalone .pptx assembler
apps/wuburead/      OPC reader CLI (dump parts + extract text)
apps/wubuedit/      round-trip re-writer CLI
apps/wubudoc/       Markdown + HTML + RTF + JSON serializers
apps/wubuodf/       OpenDocument read+write CLI
apps/wubuconv/      Unified format conversion CLI
apps/wubupdf/       PDF output CLI
apps/wubuqr/        QR encode/decode CLI
apps/wubuos/        SDL2 GUI shell (Editor + 5 engine views)
docs/               ARCHITECTURE.md, SLERM.md, FEATURES_100.md
tests/              ctest suite (27/27 green)
```

### Design rules (enforced)

1. **C11 only.** `_POSIX_C_SOURCE=200809L` globally; no GNU extensions.
2. **Opaque structs.** Every module's `struct foo` is in the `.c`
   file; the header forward-declares only. Callers cannot reach in.
3. **No god headers.** Each header exposes exactly one module's API
   plus its minimal includes. No header pulls in the whole tree.
4. **Self-contained modules.** Each `.c` includes its own `.h`,
   system headers it needs, and its direct dependencies — nothing else.
5. **No double-coding.** Shared helpers live in exactly one place
   (`src/wububase/`, `src/wubuxml/`).
6. **Clean-room only.** No upstream source file is present. Reference
   repos are read-only and cited in docs.

## Build

```sh
# fast release build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure

# fast smoke test (exclude slow OCR/CNN battery)
./tools/smoke.sh
./tools/smoke.sh ocr           # slow OCR only
./tools/smoke.sh all           # literally everything

# ASan+UBSan build (0 leaks/0 UB)
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DWITH_SANITIZER=ON
cmake --build build-asan -j$(nproc)
ctest --test-dir build-asan --output-on-failure

# build with all warnings as errors
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release
cmake --build build-perf -j$(nproc)
```

Requires a C11 compiler (tested: gcc 13.3). POSIX `open_memstream` /
`strdup` are used via `_POSIX_C_SOURCE=200809L` (no GNU extensions).
The only external link is `zlib`, and only inside the inflate test as
an independent oracle for the DEFLATE decoder.

## Usage examples

### Unified CLI (`wubuoffice`)

```sh
# Convert anything to anything else
wubuoffice convert report.docx slides.pptx
wubuoffice convert data.xlsx formatted.csv

# Round-trip with fidelity
wubuoffice edit draft.docx final.docx

# Read a document's raw parts
wubuoffice read template.docx

# Legacy format support
wubuoffice convert legacy.xls data.csv
wubuoffice convert old.pptx slides.pdf
```

### Standalone binaries

```sh
wubuword   document.docx       # build a rich .docx from CLI
wubucell   spreadsheet.xlsx    # build a workbook (formulas + charts)
wubushow   deck.pptx           # build a slide deck
wuburead   file.docx           # dump parts + extract text
wubuedit   in.docx out.docx    # round-trip re-write
wubuconv   in.xlsx out.odt     # unified conversion
wubupdf    doc.docx out.pdf    # fixed-layout PDF
wubuqr     "hello" qr.png      # QR code encode
```

### GUI Shell (`wubuos`)

```sh
# Launch the SDL2 GUI shell (Editor + all engine views)
wubuos

# Open a file in the editor
wubuos file.docx

# Compare two files (diff view)
wubuos compare a.txt b.txt
```

## Design rules

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full module
boundary map and the no-double-code rules.

### The SLERM posture

WuBuOffice is a **SLERM** (clean-room reimplementation), not a fork or
vendor:

| Approach | Copy code? | Compile upstream? | From scratch? |
|----------|:----------:|:-----------------:|:-------------:|
| Fork | yes | yes | no |
| Vendor | yes | yes | no |
| **SLERM** | **no** | **no** | **yes** |

We read upstream repos only to learn format truth (ECMA-376 / ISO 29500
specs, element names, content types). Then we write our own parsing and
serialization in C11. No `.cs`, `.java`, or upstream source file is
present in this repo.

## What's in the docs

| Doc | Contents |
|-----|----------|
| [README.md](README.md) | This file — quick start + format coverage + usage |
| [GUI_PARITY_PLAN.md](GUI_PARITY_PLAN.md) | SDL2 GUI steamroller plan: every view, every feature, every keybinding |
| [GAPS_REAL.md](GAPS_REAL.md) | **VERIFIED gap list** (source audit + build + ctest) — use this for planning |
| [FORMATS.md](FORMATS.md) | Format roadmap with tier breakdown (OOXML → ODF → legacy) |
| [FORMATS_GUI.md](FORMATS_GUI.md) | Native TUI (`wubutui` + `wubuview`) design — terminal human interface |
| [FORMATS_STD.md](FORMATS_STD.md) | Format coverage + ISO/IEC/W3C standards reference |
| [FORMATS_OCR.md](FORMATS_OCR.md) | Standards track + OCR pipeline spec |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Module map + design rules |
| [docs/SLERM.md](docs/SLERM.md) | SLERM definitions + what reference repos contributed |
| [docs/fork-vs-slerm.md](docs/fork-vs-slerm.md) | Fork vs SLERM vs Vendoring comparison |
| [docs/FEATURES_100.md](docs/FEATURES_100.md) | Feature-by-feature status board |
| [UI_TRIPLE_REVIEW.md](UI_TRIPLE_REVIEW.md) | Vision + architectural + artistic UI critique |
| [RESEARCH_GAPS_100.md](RESEARCH_GAPS_100.md) | 100 tracked gaps (STALE — see GAPS_REAL.md) |
| [RESEARCH_GAPS.md](RESEARCH_GAPS.md) | 20-web-search gap analysis (STALE — see GAPS_REAL.md) |
| [ROADMAP_OCR.md](ROADMAP_OCR.md) | OCR pipeline roadmap (Phase 0 → 3) |
| [steamroller_plan.md](steamroller_plan.md) | GUI steamroller passes (render → paste-plain → polish) |
| [MODULARIZATION.md](MODULARIZATION.md) | Build modularization + code reuse survey (wububase, wubupng, wuburender) |
| [UI_TRIPLE_REVIEW.md](UI_TRIPLE_REVIEW.md) | Triple Devil's Advocate UI review |

## Project status

| Area | Status |
|------|--------|
| Core format engines (zip, xml, oxml, cell, word, show) | ✅ All working |
| Round-trip fidelity | ✅ Verified by foreign-format tests |
| Unified CLI (`wubuoffice`) | ✅ All 6 subcommands wired |
| GUI shell (`wubuos`) | ✅ SDL2/FreeType2, Editor + 5 views, menu bar, clipboard |
| Test suite | ✅ 27/27 green, ASan-clean |
| Documentation | ✅ README + plan + 100-gap inventory + architecture |
| Sanitizer build (ASan+UBSan) | ✅ 0 leaks, 0 UB |

## Project structure

```
WuBuOffice/
├── .github/          # CI workflows
├── apps/             # CLI tools + SDL2 GUI shell
│   ├── wubuoffice/   # unified CLI dispatch
│   ├── wubuword/     # WordprocessingML CLI
│   ├── wubucell/     # SpreadsheetML CLI
│   ├── wubushow/     # PresentationML CLI
│   ├── wuburead/     # OPC reader CLI
│   ├── wubuedit/     # round-trip re-writer CLI
│   ├── wubudoc/      # Markdown/HTML/RTF/JSON serializers
│   ├── wubuodf/      # OpenDocument CLI
│   ├── wubuconv/     # unified format conversion CLI
│   ├── wubupdf/      # PDF output CLI
│   ├── wubuqr/       # QR encode/decode CLI
│   └── wubuos/       # SDL2 GUI shell + views
├── src/              # All library modules
│   ├── wubuzip/      # ZIP + DEFLATE
│   ├── wubuxml/      # XML writer
│   ├── wubuoxml/     # OPC packaging
│   ├── wubucfb/      # MS-CFB / OLE2 reader
│   ├── wubuword/     # WordprocessingML model
│   ├── wubucell/     # SpreadsheetML model
│   ├── wubushow/     # PresentationML model
│   ├── wuburead/     # OPC reader dispatcher
│   ├── wubuedit/     # Round-trip re-writer
│   ├── wubudoc/      # Markdown/HTML/RTF/JSON/EPUB
│   ├── wubuodf/      # OpenDocument
│   ├── wubupdf/      # PDF writer
│   ├── wubuconv/     # Format conversion
│   ├── wubulegacy/   # Legacy binary readers
│   ├── wubuqr/       # QR codec
│   ├── wubusvg/      # SVG ingest + rasterizer
│   ├── wububase/     # Shared utilities (utf8, crc32, Buf, xml_escape)
│   ├── wubuimage/    # Image pipeline (PNG, JPEG, Netpbm)
│   ├── wubuocr/      # OCR pipeline (CRNN+CTC, QR, fontbank)
│   ├── wubuformula/  # Formula engine
│   └── wubumodel/    # Shared document model
├── docs/             # Architecture + SLERM docs
├── tests/            # ctest suite
├── tools/            # Scripts (smoke.sh, etc.)
├── fonts/            # Font study corpus
├── data/             # OCR training data, wordlists
├── CMakeLists.txt    # Top-level build
├── README.md         # This file
├── LICENSE           # Waefrebeorn Umbrella License v3.0
├── FORMATS.md        # Format roadmap
├── GUI_PARITY_PLAN.md# SDL2 GUI steamroller plan
├── GAPS_REAL.md       # **VERIFIED gap list** (source audit + build + ctest)
├── GAPS_100.md       # 100 tracked gaps (STALE — see GAPS_REAL.md)
└── MODULARIZATION.md # Build + reuse strategy
```

## Why "C11 or bust"

Office formats are byte-level container formats. A from-scratch C11 core
keeps the runtime at zero dependencies, is trivially embeddable (games,
firmware, headless servers), and is the smallest honest surface to
reimplement a format on. No VM, no runtime, no telemetry.

## References consulted (not copied)

- ECMA-376 / ISO 29500 (Office Open XML)
- `dotnet/Open-XML-SDK` — format truth only (read, never compiled in)
- OPC spec (ISO 29500-2 packaging)
- `python-docx`, `openpyxl`, `python-pptx` — round-trip validation oracles
- `odfpy` — ODF round-trip validation oracle

WuBuOffice's code is original. No file from any upstream repo is
compiled, linked, or vendored here.

---

## License

This project is licensed under the **Waefrebeorn Umbrella License v3.0**.
See the [LICENSE](LICENSE) file for the full license text.

The Waefrebeorn Umbrella License is a custom source-available license.
It is not OSI-approved and not FSF-approved.
