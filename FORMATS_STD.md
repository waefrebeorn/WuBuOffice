# Formats & Standards — the "9 yards" backbone

WuBuOffice + WuBuPad are the ingestion/creation backbone for wubuOS. This doc
tracks the format coverage, the **ISO/IEC and W3C standards** each is built
against, and what's real (clean-room, tested) vs. referenced.

## Why standards, not vibes
Every reader/writer in this tree targets a published specification so output is
interoperable with independent tools (openpyxl, python-pptx, xlrd/xlwt,
pypdf, EbookLib, and — for the font work — `xml.dom.minidom` as an XML oracle).
Round-trip and foreign-read tests assert we agree with those tools.

## Canonical references (cite these)
| Domain | Standard | Used by |
|---|---|---|
| Office Open XML (DOCX/XLSX/PPTX) | **ISO/IEC 29500** | wubuoxml / wubuword / wubucell / wubushow |
| OpenDocument (ODT/ODS/ODP) | **ISO/IEC 26300** | wubuodf |
| PDF | **ISO/IEC 32000** (PDF 1.7) | wubupdf |
| JPEG 2000 | **ISO/IEC 15444** | (reference; raster path) |
| Open Font Format (sfnt/TTF/OTF) | **ISO/IEC 14496-22** | wubufont |
| WOFF 1.0 (web font container) | **W3C WOFF 1.0** + RFC 1950 (zlib) | wubufont/woff |
| ZIP / DEFLATE container | **ISO/IEC 21320** (ZIP app note) + RFC 1951 | wubuzip |
| Compound File Binary (legacy .doc/.xls/.ppt) | **MS-CFB** (ISO/IEC 29500-2 references) | wubucfb |
| SVG | **W3C SVG 1.1** (and SVG 2 `<font>`/path) | wubufont emit |
| Quantities/units in docs | **ISO 80000** (quantities & units) | doc model metadata |
| Document identifiers | **ISO 2145** (section numbering), **ISO 8601** (dates) | wubudoc / wubucell |

## What's live and tested (clean-room, native C11)
- **SFNT / TrueType → SVG font** (`src/wubufont/`): parses the sfnt directory,
  `head`/`maxp`/`hhea`/`loca`/`glyf`/`cmap`/`name`/`hmtx`; decodes TrueType
  quadratic glyph outlines to SVG path `d` (Y-flipped); emits a standalone
  `<svg><font>` document. Validated against a **real DejaVuSans.ttf** and the
  emitted SVG is confirmed well-formed XML by an **independent** Python
  `xml.dom.minidom` oracle. (`wubufont_cli` + `test_wubufont`.)
  - Scope note: TrueType `glyf` outlines are decoded. CFF/OpenType (`CFF `
    table, Type2 charstrings) is *detected* but not yet decoded — that needs a
    Type2 interpreter (future work, not faked).
- **WOFF 1.0 read + write** (`src/wubufont/woff.c`): `woff_open` decompresses
  every table (RFC 1950 zlib, reusing `wubuzip` DEFLATE + our own Adler-32),
  reconstructs the sfnt in memory, and hands it to the font parser — so a
  `.woff` is transparently a compressed font. `sfnt_to_woff` compresses an sfnt
  back to WOFF (per-table zlib, stores uncompressed when that's smaller).
  Verified by a **byte-exact round-trip** on DejaVuSans.ttf: unitsPerEm, glyph
  count, cmap, and glyph outlines all preserved; the WOFF (431 KB) is 43%
  smaller than the sfnt (760 KB), and the CLI reads the `.woff` straight to a
  well-formed SVG (independent XML oracle).
- Office OOXML / ODF / legacy CFB / PDF / RTF / HTML / EPUB / MD — see
  `GAPS_*.md` and the existing test suite (27/27 green).
- **SVG ingest + regurgitate** (`src/wubusvg/`): ingests an SVG (XML) byte
  stream into a structured element tree (reusing the dependency-free `wubuxml`
  SAX parser) and regurgitates it back to well-formed SVG (reusing the
  `wubuxml` writer). This is the vector-document ingestion/regurgitation
  contract for WuBuOS: bytes in → inspectable/editable tree → bytes out.
  Preserves nesting, attributes (source order), and entity-decoded text;
  rejects malformed/unbalanced markup. Verified by an **idempotent round-trip**
  (re-ingesting the regurgitated output yields a structurally identical tree)
  and by the full cross-module chain
  `TTF → wubufont → SVG → wubusvg (count/regurgitate) → independent XML oracle`.
- **SVG editing (creation half)** added to wubusvg: `svg_set_attr`,
  `svg_remove_attr`, `svg_new_node` + `svg_append_child` / `svg_insert_child`,
  `svg_remove_child`, `svg_set_text`. The AGI transforms a vector document via
  these mutators and re-emits well-formed SVG; edits survive re-ingestion
  (round-trip proof). `wubusvg_cli` exposes `--set-attr` / `--remove-attr` on
  the root for shell/agent-driven transforms.

## Reference fork (NOT modified, NOT redistributed here)
`github.com/waefrebeorn/WuBuContainer` — a fork of the convert.to.it universal
converter (TypeScript/Vite, GPL-2.0). Used **only as a citation map** of ~50
format handlers (fonts, SVG, bson, nbt, qoi, icns, xcursor, pandoc, ffmpeg, …)
to decide what the C11 backbone should grow next. We do **not** copy its code;
everything here is original clean-room C11. The fork is cloned read-only under
`/home/wubu/ref/WuBuContainer`.

## Next on the "9 yards" (candidates, prioritized by reuse)
1. **More outline/raster formats** reusing wubufont + wubuzip: OTF/CFF decode
   (Type2), WOFF/WOFF2 container (zlib/ brotli over sfnt), `icns`/`xcursor`
   (PNG-in-container, reuses wubuzip + a PNG decoder), `bson` (binary JSON),
   `qoi` (lossless image, trivial decoder).
2. **Standards metadata** in the doc model: ISO 8601 dates, ISO 2145 outline
   numbering, ISO 80000 unit annotations on numeric cells.
3. **SVG as a first-class document type** in WuBuPad's ingestion engine
   (parse + regurgitate SVG, round-trip through wubufont's emitter).

## Verification gate
Both repos are sanitizer-gated (ASan + UBSan): 0 leaks, 0 UB, 0 warnings before
any change ships. The font work specifically is cross-checked by an independent
XML parser so a malformed emitter can never ship silent.
