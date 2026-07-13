# WuBuOffice

A **ground-up C11 reimplementation** of the OOXML office document formats
(`.docx` / `.xlsx` / `.pptx`), built from the published ECMA-376 specifications
and behavioural observation of real Office/LibreOffice files.

> **SLERM** (verb): take someone's full work and build your own version from
> scratch. Not a fork — a clean-room, dependency-free reimplementation.

Every byte is written by us in C11. **No forks, no vendored upstream code, zero
runtime dependencies** beyond POSIX. (The only external link is `zlib`, used
*only* inside the inflate test as an independent oracle.)

## What works

| Layer | Status | Description |
|-------|--------|-------------|
| `wubuzip` | ✅ working | ZIP writer (store) **+ full reader** + IEEE-802.3 CRC-32 |
| `wubuzip` inflate | ✅ working | RFC 1951 DEFLATE: store / fixed / dynamic Huffman |
| `wubuxml` | ✅ working | Streaming, well-formed XML writer with correct escaping |
| `wubuoxml` | ✅ working | OPC writer **+ reader** — `[Content_Types].xml`, `.rels` graphs, text extraction |
| `wubuword` | ✅ working | WordprocessingML: headings, bold, tables, `.docx` assembly |
| `wubucell` | ✅ working | SpreadsheetML: multi-sheet, shared strings, numbers, **formulas**, styles |
| `wubushow` | ✅ working | PresentationML: multi-slide, title + multi-paragraph body, theme/master/layout |
| `wubuedit` | ✅ working | Round-trip re-writer: docx in → docx out, text preserved |

All three writers emit files that open in Microsoft Word / Excel /
PowerPoint and LibreOffice. The reader decodes **real deflate-compressed
(method 8)** parts — not just the store-mode files the writer emits.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cd build && ctest --output-on-failure
```

Requires a C11 compiler (tested: gcc 13.3, clang). POSIX `open_memstream` /
`strdup` are used via `_POSIX_C_SOURCE=200809L` (no GNU extensions).

## Use

```sh
# unified CLI
./build/wubuoffice word  out.docx          # rich .docx
./build/wubuoffice cell  out.xlsx          # workbook (shared strings + formula)
./build/wubuoffice show  out.pptx          # slide deck
./build/wubuoffice read  file.docx         # dump parts + extract text
./build/wubuoffice edit  in.docx out.docx  # round-trip re-write

# standalone binaries also exist: wubuword, wubucell, wubushow, wuburead, wubuedit
```

`wubuoffice cell` demonstrates the deepened spreadsheet: a **shared-string
table** (the real Excel default), a numeric cell, and a **formula** cell
(`=B2+B3` with a cached value). `wubuoffice show` emits multi-paragraph bullet
slides.

## Layout

```
src/wubuzip/        ZIP container + DEFLATE (bit, huffman, fixed, block, inflate, io_le)
src/wubuxml/        streaming XML writer
src/wubuoxml/       OPC writer + reader (package, rels_path, docx_text)
apps/wubuword/      WordprocessingML builder + .docx assembler
apps/wubucell/      SpreadsheetML builder + .xlsx assembler
apps/wubushow/      PresentationML builder + .pptx assembler
apps/wuburead/      OPC reader CLI
apps/wubuedit/      round-trip re-writer
apps/wubuoffice/    unified CLI dispatch
docs/               ARCHITECTURE.md, SLERM.md
tests/              ctest suite (crc, inflate, reader_roundtrip, edit_roundtrip)
```

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the module boundaries and
the no-double-code rules, and [`docs/SLERM.md`](docs/SLERM.md) for the SLERM
approach and what the reference repositories were (and were not) used for.

## Why "C11 or bust"

Office formats are byte-level container formats. A from-scratch C11 core keeps
the runtime at zero dependencies, is trivially embeddable (games, firmware,
headless servers), and is the smallest honest surface to reimplement a format
on. No VM, no runtime, no telemetry.

## References consulted (not copied)

- ECMA-376 / ISO 29500 (Office Open XML)
- `dotnet/Open-XML-SDK` — format truth only (read, never compiled in)
- OPC spec (ISO 29500-2 packaging)

WuBuOffice's code is original. No file from any upstream repo is compiled,
linked, or vendored here.
