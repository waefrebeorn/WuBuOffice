# WuBuOffice — Architecture

A ground-up, from-scratch C11 implementation of the OOXML document formats
(`.docx` / `.xlsx` / `.pptx`). **No forks, no vendored upstream code, zero
runtime dependencies** beyond POSIX. The only external thing linked anywhere
is `zlib` — and only inside the *test* binary, where it serves as an
independent ground-truth oracle for the inflate decoder.

## Design rules (enforced)

1. **C11 only.** `_POSIX_C_SOURCE=200809L` is defined globally so pointer-
   returning POSIX calls (`strdup`, `open_memstream`) are not implicitly
   truncated to `int` under `-std=c11`.
2. **Opaque structs.** Every module's `struct foo` is defined in the `.c`
   file; the public header only forward-declares `typedef struct foo foo;`.
   Callers cannot reach in.
3. **No god headers.** A header exposes exactly one module's API plus its
   minimal includes. No header pulls in the whole tree.
4. **Self-contained modules.** Each `.c` includes its own `.h`, the system
   headers it needs, and its direct dependencies — nothing else.
5. **No double-coding.** Shared helpers live in exactly one place:
   - `src/wubuzip/io_le.h` — little-endian `rd16`/`rd32` (static inline) used
     by both the ZIP reader and the OPC reader.
   - `src/wubuoxml/rels_path.c` — the `.rels` path computation, shared by the
     OPC **writer** and **reader** so both agree on the relationship graph.
   - `src/wubuxml/xml.c` — the one XML writer, reused by every document
     builder.
6. **Minimal IWYU.** Headers include only what their own declarations need.

## Module map

```
src/wubuzip/        from-scratch ZIP container + DEFLATE (reader AND writer)
  crc.c/.h           IEEE 802.3 CRC-32 (zlib-compatible)
  zip.c              ZIP writer (store / method 0) — local + central dir + EOCD
  reader.c           ZIP reader: locate EOCD, walk central dir, extract parts
  bit.c/.h           LSB-first bit reader (DEFLATE)
  huffman.c/.h       canonical Huffman build + decode (puff.c algorithm)
  fixed.c/.h         RFC 1951 fixed Huffman tables, GENERATED at runtime
  block.c/.h         literal/length + distance block decode (fixed + dynamic)
  inflate.c          top-level DEFLATE driver (store / fixed / dynamic)
  io_le.h            shared little-endian readers (inline)

src/wubuxml/        streaming, well-formed XML writer
  xml.c              element stack, attribute/text escaping, auto-closing

src/wubuoxml/       Open Packaging Conventions (the OPC layer over ZIP)
  package.c          OPC writer: [Content_Types].xml + .rels graphs
  rels_path.c/.h     .rels path computation (shared writer+reader)
  reader.c           OPC reader: enumerate parts, parse .rels, inflate
  docx_text.c/.h     WordprocessingML plain-text extraction (<w:t>)

apps/wubuword/      WordprocessingML builder + .docx assembler
  word.c/.h          paragraphs (heading/bold), tables
  assemble.c/.h      package the document.xml into a valid .docx
  main.c/entry.c     CLI + standalone driver

apps/wubucell/      SpreadsheetML builder + .xlsx assembler
  cell.c/.h          sheets, inline/shared strings, numbers, formulas, styles
  main.c/entry.c

apps/wubushow/      PresentationML builder + .pptx assembler
  show.c/.h          slides with title + multi-paragraph body
  main.c/entry.c

apps/wuburead/      OPC reader CLI (dump parts + extract text)
apps/wubuedit/      round-trip re-writer (docx in -> docx out, text preserved)
apps/wubuoffice/    unified CLI dispatch (word/cell/show/read/edit)
```

## Why `fixed.c` generates its tables

The deflate *fixed* Huffman code lengths are 288 constants. A hand-written
literal array of that size is exactly the kind of thing that gets truncated or
mis-edited (and it did, once — see `docs/SLERM.md`). `wubuzip_fixed_litlen()`
and `wubuzip_fixed_dist()` build the arrays programmatically from four simple
ranges, so the bug class is gone: there is no 288-entry literal to corrupt.

## Build & test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cd build && ctest --output-on-failure
```

Tests (all dependency-free except the inflate oracle):
- `wubuzip_crc` — CRC32 matches the known `CRC32("WuBu")`.
- `inflate` — round-trips zlib-compressed buffers (fixed **and** dynamic blocks),
  cross-checked against zlib.
- `reader_roundtrip` — writes a `.docx`, reads it back, extracts text.
- `edit_roundtrip` — writes a `.docx`, round-trips through `wubuedit`, reads
  the result back and confirms the text survived.
