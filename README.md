# WuBuOffice

A **ground-up C11 reimplementation** of the OOXML office document formats
(`.docx` / `.xlsx` / `.pptx`), built from the published ECMA-376 specifications
and behavioural observation of real Office/LibreOffice files.

> **SLERM** (verb): take someone's full work and build your own version from
> scratch. Not a fork — a clean-room, dependency-free reimplementation.

Every byte is written by us in C11. **No forks, no vendored upstream code, zero
runtime dependencies** beyond POSIX. (The only external link is `zlib`, used
*only* inside the inflate test as an independent oracle.)

## Sister projects

- **[WuBuPad](https://github.com/waefrebeorn/WuBuPad)** — the clean C11
  **code editor** built the same way: a ground-up, fork-free alternative to
  Notepad++. It shares WuBuOffice's engineering standard (opaque structs, no
  god headers, reuse-never-duplicate, sanitizer-gated). Its headless core
  (piece-table buffer, C/JSON lexers, document model with undo/redo +
  cursor/selection) is the editor engine WuBuOffice's in-app editing will
  reuse. Reference clone of Notepad++ lives in `ref/` for feature parity only;
  no code is copied.

## What works

| Layer | Status | Description |
|-------|--------|-------------|
| `wubuzip` | ✅ working | ZIP writer (store) **+ full reader** + IEEE-802.3 CRC-32 |
| `wubuzip` inflate | ✅ working | RFC 1951 DEFLATE: store / fixed / dynamic Huffman |
| `wubuxml` | ✅ working | Streaming, well-formed XML writer with correct escaping |
| `wubuoxml` | ✅ working | OPC writer **+ reader** — `[Content_Types].xml`, `.rels` graphs, text extraction |
| `wuburead` | ✅ working | **Full OOXML read-back**: docx paragraph/run text, xlsx cells (shared-string + inline-string resolution, formula cached values) as TSV, pptx slide text. Dispatches by detected part type. |
|| `wubuword` | ✅ working | WordprocessingML: headings, bold, tables, `.docx` assembly |
| `wubucell` | ✅ working | SpreadsheetML: multi-sheet, shared strings, numbers, **formulas**, styles |
| `wubushow` | ✅ working | PresentationML: multi-slide, title + multi-paragraph body, theme/master/layout |
| `wubuedit` | ✅ working | **Structure-preserving** round-trip: parses word/document.xml into a model (paragraph style, bold runs, tables) and re-emits it — structure survives the reader+writer loop |
| `wubudoc` | ✅ working | Markdown (read+write), HTML + RTF export, lossless JSON of all three models |
| `wubuodf` | ✅ working | **OpenDocument** `.odt` / `.ods` / `.odp` + flat `.fodt` / `.fods` / `.fodp` read+write (validated by odfpy / stdlib XML) |
| `wubupdf` | ✅ working | **PDF 1.7** writer over the doc model (pypdf + pdfminer validated) |
| `wubuzip` | ✅ working | **ZIP container** + DEFLATE (BIT/huffman/fixed/block/inflate), also powers EPUB |
| `wubudoc` | ✅ working | **EPUB 3** writer over the doc model (EbookLib validated) — chapters split at H1/Title, OPF + nav + NCX |
| `wubulegacy` | ✅ working | **Legacy binary** readers: `.xls` (BIFF8, validated by xlwt/xlrd), `.doc` (FIB + piece table), `.ppt` (record atoms) |
| `wubuconv` | ✅ working | **Unified conversion**: any supported format → any other (docx/xlsx/pptx/csv/tsv/md/html/rtf/odt/ods/odp/fodt/fods/fodp/doc/xls/ppt/pdf/epub/json) |

All three writers emit files that open in Microsoft Word / Excel /
PowerPoint and LibreOffice. The reader decodes **real deflate-compressed
(method 8)** parts — not just the store-mode files the writer emits. ODF files
are read and written by the independent **odfpy** library in CI.

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
./build/wubuoffice convert in.xlsx out.odt # any format -> any other
./build/wubuoffice convert legacy.xls out.csv  # legacy binary .xls/.doc/.ppt in
./build/wubuoffice convert report.docx out.pdf # PDF out (fixed layout)

# standalone binaries also exist: wubuword, wubucell, wubushow, wuburead, wubuedit
```

Supported by `convert` — in: `docx xlsx pptx csv tsv md odt ods odp fodt fods fodp doc xls ppt`;
out: `docx xlsx pptx csv tsv md html rtf odt ods odp fodt fods fodp pdf epub json`.
Cross-family bridges: sheet→doc (as a table), show→doc (title + bullet body),
text→sheet (flatten), text→show (one slide per heading). JSON dumps any model.

## Layout

```
src/wubuzip/        ZIP container + DEFLATE (bit, huffman, fixed, block, inflate, io_le)
src/wubuxml/        streaming XML writer
src/wubuoxml/       OPC writer + reader (package, rels_path, docx_text)
src/wubucfb/        MS-CFB / OLE2 compound-file container reader (legacy substrate)
apps/wubuword/      WordprocessingML builder + .docx assembler
apps/wubucell/      SpreadsheetML builder + .xlsx assembler
apps/wubushow/      PresentationML builder + .pptx assembler
apps/wuburead/      OPC reader CLI
apps/wubuedit/      round-trip re-writer
apps/wubudoc/       Markdown + HTML + RTF + JSON serializers over all models; EPUB 3 writer
apps/wubuodf/       OpenDocument (.odt/.ods/.odp) + flat (.fodt/.fods/.fodp) read+write; odf_body shared emitters/SAX
apps/wubulegacy/    legacy binary readers (.xls/.doc/.ppt) over MS-CFB
apps/wubupdf/       PDF/1.7 writer over the doc model
apps/wubuconv/      unified format conversion across the full matrix
apps/wubuoffice/    unified CLI dispatch (word/cell/show/read/edit/convert)
docs/               ARCHITECTURE.md, SLERM.md, FORMATS.md
tests/              ctest suite (crc, inflate, reader, edit, csv, md, rtf/json, odf, convert, foreign, legacy_pdf, flat_epub)
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
