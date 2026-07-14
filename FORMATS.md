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
| HTML | `.html` | ✅ (export) | ✅ | Semantic HTML5 from the doc model (h1-3, p, strong, table). |
| Rich Text Format | `.rtf` | ⬜ | 🔨 | WordprocessingML's ancestor; write first. |
| JSON | `.json` | ⬜ | 🔨 | Lossless dump of each model for pipelines. |

## Tier 2 — OpenDocument (ODF / ISO 26300)

| Format | Ext | Read | Write | Notes |
|--------|-----|:----:|:-----:|-------|
| OpenDocument Text | `.odt` | ⬜ | ⬜ | ZIP + `content.xml`; `text:p`, `text:h`, tables. |
| OpenDocument Spreadsheet | `.ods` | ⬜ | ⬜ | `table:table` / `table:table-row` / `table:table-cell`. |
| OpenDocument Presentation | `.odp` | ⬜ | ⬜ | `draw:page` + `draw:frame`/`text:p`. |
| OpenDocument Flat XML | `.fodt/.fods/.fodp` | ⬜ | ⬜ | Single-file XML variants. |

## Tier 3 — Legacy binary (hard; MS-CFB / BIFF)

| Format | Ext | Read | Write | Notes |
|--------|-----|:----:|:-----:|-------|
| Legacy Word | `.doc` | ⬜ | ⭕ | MS-CFB compound file + Word binary stream. Read-only aim. |
| Legacy Excel | `.xls` | ⬜ | ⭕ | BIFF8 records inside CFB. Read-only aim. |
| Legacy PowerPoint | `.ppt` | ⬜ | ⭕ | PPT binary inside CFB. Read-only aim. |

## Tier 4 — Fixed-layout & adjacent

| Format | Ext | Read | Write | Notes |
|--------|-----|:----:|:-----:|-------|
| PDF | `.pdf` | ⭕ | ⬜ | Write a minimal PDF from the doc model (text runs). |
| EPUB | `.epub` | ⬜ | ⬜ | ZIP + XHTML; reuses HTML export + OPF packaging. |
| Plain text | `.txt` | ✅* | ⬜ | *extraction exists via wuburead; add clean writer. |

## Engine reuse map

- `wubuzip` — ZIP container for OOXML, ODF, EPUB.
- `wubuxml` — SAX + writer for all XML-based formats.
- `wubucell_book` — the spreadsheet model behind xlsx / ods / csv.
- doc model (`wubuword`/`docmodel`) — behind docx / odt / md / rtf / html.
- `wubushow_pres` — behind pptx / odp.

One model per document class, many serializers. That's how we get supremacy
without N² code.
