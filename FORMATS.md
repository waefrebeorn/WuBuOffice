# WuBuOffice — Format Supremacy Roadmap

The goal: **read, write, and round-trip every office document format that
matters**, clean-room in C11, zero runtime dependencies. This file is the
scoreboard.

Legend: ✅ done · 🔨 in progress · ⬜ planned · ⭕ out of scope (for now)

## Tier 0 — Core OOXML (the crown jewels)

| Format | Ext | Read | Write | Round-trip | Foreign-file robust |
|--------|-----|:----:|:-----:|:----------:|:-------------------:|
| WordprocessingML | `.docx` | ✅ | ✅ | ✅ | ✅ |
| SpreadsheetML | `.xlsx` | ✅ | ✅ | ✅ | ✅ (openpyxl) |
| PresentationML | `.pptx` | ✅ | ✅ | ✅ | ✅ (python-pptx) |

## Tier 1 — Plain-text & interchange (fast, high-value)

| Format | Ext | Read | Write | Notes |
|--------|-----|:----:|:-----:|-------|
| Comma-separated values | `.csv` | ✅ | ✅ | RFC 4180: quoting, embedded commas/newlines/quotes. Maps to `wubucell_book`. |
| Tab-separated values | `.tsv` | ✅ | ✅ | Same engine as CSV, tab delimiter. |
| Markdown | `.md` | ✅ | ✅ | Headings, bold, bullet lists, tables ↔ doc model. |
| HTML | `.html` | ⬜ | ✅ | Semantic HTML5 from the doc model (h1-3, p, strong, table). |
| Rich Text Format | `.rtf` | ⬜ | ✅ | RTF 1.x from doc model: headings, bold, tables, UTF-8 via \uN escapes. |
| JSON | `.json` | ⬜ | ✅ | Lossless JSON of all three models (doc/workbook/presentation). |
| EPUB (source) | `.epub` | ⬜ | — | Write-only for now; readers reuse the doc model + XHTML. |

## Tier 2 — OpenDocument (ODF / ISO 26300)

| Format | Ext | Read | Write | Notes |
|--------|-----|:----:|:-----:|-------|
| OpenDocument Text | `.odt` | ✅ | ✅ | ZIP + `content.xml`; `text:p`, `text:h`, tables. Validated by odfpy. |
| OpenDocument Spreadsheet | `.ods` | ✅ | ✅ | `table:table` / `table:table-row` / `table:table-cell`. Validated by odfpy. |
| OpenDocument Presentation | `.odp` | ✅ | ✅ | `draw:page` + `draw:frame`/`text:p`. Validated by odfpy. |
| OpenDocument Flat XML | `.fodt/.fods/.fodp` | ✅ | ✅ | Single-file `<office:document>` XML: same body emitters + SAX handlers as the packaged formats (zero duplicated logic). Validated as well-formed `office:document` with correct inner root + text. |

## Tier 3 — Legacy binary (hard; MS-CFB / BIFF)

Container: `src/wubucfb` — clean-room MS-CFB / OLE2 reader (header, DIFAT, FAT,
directory, MiniFAT + mini-stream). Byte-verified against `olefile` on genuine
Microsoft binaries. All three legacy readers sit on it.

| Format | Ext | Read | Write | Notes |
|--------|-----|:----:|:-----:|-------|
| Legacy Excel | `.xls` | ✅ | ⭕ | BIFF8: SST (+CONTINUE splits), BOUNDSHEET, LABELSST/LABEL, NUMBER, RK, MULRK, FORMULA. Validated by xlwt/xlrd. → `wubucell_book`. |
| Legacy Word | `.doc` | ✅ | ⭕ | FIB + complex piece table (CLX/PlcPcd), CP1252/UTF-16 pieces, CR paragraph split. Verified on real MS `.doc`. → `dm_doc`. |
| Legacy PowerPoint | `.ppt` | ✅ | ⭕ | Recursive record tree; TextChars/TextBytes atoms per Slide container (master-text fallback). Verified on real MS `.ppt`. → `wubushow_pres`. |

## Tier 4 — Fixed-layout & adjacent

| Format | Ext | Read | Write | Notes |
|--------|-----|:----:|:-----:|-------|
| PDF | `.pdf` | ⭕ | ✅ | `src/wubupdf`-style writer over `dm_doc`: PDF/1.7, Helvetica/Helvetica-Bold with AFM metrics, WinAnsi encoding, word-wrap + auto-pagination, headings/bold/tables. Validated by pypdf + pdfminer. |
| EPUB | `.epub` | ⭕ | ✅ | EPUB 3 (+ EPUB2 NCX) over `dm_doc`: ZIP container (`mimetype` stored first), XHTML chapters split at H1/Title, OPF package + nav + toc.ncx. Reuses `wubuzip` + the doc-model HTML body renderer. Validated by EbookLib. |
| Plain text | `.txt` | ✅* | ⬜ | *extraction exists via wuburead; add clean writer. |

## Unified conversion

`wubuoffice convert <in> <out>` bridges **any** supported format to **any**
other. Three canonical models are the interchange pivot:

- **TEXT** (`dm_doc`) ← docx / md / html / rtf / odt / fodt / **doc** · → docx / md / html / rtf / odt / fodt / **pdf** / **epub**
- **SHEET** (`wubucell_book`) ← xlsx / csv / tsv / ods / fods / **xls** · → xlsx / csv / tsv / ods / fods
- **SHOW** (`wubushow_pres`) ← pptx / odp / fodp / **ppt** · → pptx / odp / fodp

Cross-family bridges: sheet→text (table), show→text (title + bullet body),
text→sheet (flatten), text→show (heading per slide). JSON dumps any model.

## Engine reuse map

- `wubuzip` — ZIP container for OOXML, ODF, EPUB.
- `wubucfb` — MS-CFB / OLE2 container behind legacy `.doc` / `.xls` / `.ppt`.
- `wubuxml` — SAX + writer for all XML-based formats.
- `wubucell_book` — the spreadsheet model behind xlsx / ods / csv / tsv / xls / fods.
- doc model (`wubuword`/`docmodel`) — behind docx / odt / fodt / md / rtf / html / doc / pdf / epub.
- `wubushow_pres` — behind pptx / odp / fodp / ppt.
- `wubuodf`/`odf_body` — shared body emitters + SAX handlers for packaged AND flat ODF (one source of truth per doc class).
- `wubupdf` — fixed-layout PDF serializer over the doc model.
- `wubuconv` — the format-agnostic conversion engine (matrix dispatch).

One model per document class, many serializers. That's how we get supremacy
without N² code.
