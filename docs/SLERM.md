# SLERM — what it is, and how WuBuOffice does it

> **SLERM** (verb): to take someone's full work and make your own version
> from scratch. Not a fork — a ground-up reimplementation built only from the
> published format specifications and behavioural observation.

## The stance

Microsoft publishes the OOXML formats as open ECMA/ISO specifications
(ECMA-376). That is the only thing we treat as ground truth. We read the
spec, we read real files produced by Office/LibreOffice to confirm behaviour,
and we write every byte ourselves in C11.

**We do not copy code.** No `git clone` of `dotnet/Open-XML-SDK`, no LibreOffice
translation units, no pasted snippets. The reference repositories exist on this
machine *only* as format oracles — to confirm a part name, a relationship type,
or a deflate block shape. They are never compiled into WuBuOffice.

## What the reference repos were used for (audit only)

- `dotnet/Open-XML-SDK` — confirmed the OPC packaging rules: a document needs
  `[Content_Types].xml` at the root, a `_rels/.rels` graph, and per-part
  `.rels` parts for any relationship rooted at a non-root source.
- `LibreOffice/core` — confirmed the ZIP layout Word/LibreOffice accept, and
  that the writer need not compress parts (store / method 0 is valid and opens
  everywhere). This let us ship a working writer *before* the inflate decoder
  existed.

Neither repo's source is linked, imported, or vendored.

## The one bug that taught the rule about generated tables

The deflate *fixed* Huffman tables are 288 code lengths. The first
implementation hand-wrote them as a 288-entry `uint8_t` literal:

```c
static const uint8_t FIX_LITLEN[288] = { 8,8,8, ... };   /* <-- truncated */
```

The literal was written with only 144 entries; the remaining 144 defaulted to
`0`. That produced `cnt[7]=32` instead of the correct `24`, so every fixed
block decoded to garbage. The fix was architectural, not a one-line patch:
**generate the tables at runtime** (`fixed.c`) from four ranges, so there is no
large literal to truncate. The decoder also got split into `bit` / `huffman` /
`fixed` / `block` / `inflate` so each concern is isolated and testable.

## Verification, not vibes

Every claim about output validity is checked by execution:
- All three writers emit ZIPs that Python's `zipfile` opens, with the correct
  part inventory and relationship graphs.
- The inflate decoder is checked against zlib on both fixed and dynamic blocks.
- The reader decodes **real deflate-compressed (method 8)** parts — not just the
  store-mode files the writer emits — proving the decoder handles authentic
  Office output.

## Scope

| Format | Status | Notes |
|--------|--------|-------|
| `.docx` | working | headings, bold, tables, round-trip re-write |
| `.xlsx` | working | multi-sheet, inline + shared strings, numbers, formulas, styles |
| `.pptx` | working | multi-slide, title + multi-paragraph body, theme/master/layout |
| ZIP | working | store writer, full reader |
| DEFLATE | working | store / fixed / dynamic Huffman inflate |

Roadmap (not yet built): rich cell formatting engines, chart parts, real
theme editing, a streaming writer for very large documents.
