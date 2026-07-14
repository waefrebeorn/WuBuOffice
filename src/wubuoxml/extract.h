#ifndef WUBUOXML_EXTRACT_H
#define WUBUOXML_EXTRACT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SAX-based plain-text extraction for WordprocessingML / SpreadsheetML /
 * PresentationML. All three reuse the one streaming handler in extract.c, so
 * output is consistent across document types (one line per paragraph /
 * slide, runs concatenated, entities decoded by the shared wubuxml parser).
 *
 * The docx entry point preserves the historical wubuoxml_docx_text() symbol
 * (Apps/wuburead depend on it); the xlsx/pptx entry points are new.
 *
 * Returns 0 on success and stores a caller-owned, NUL-terminated heap buffer
 * in *out (may be empty string, never NULL on success). Returns -1 on alloc
 * failure (*out is left untouched). */

/* WordprocessingML: text of word/document.xml, paragraph breaks as '\n'. */
int wubuoxml_docx_text(const uint8_t *xml, size_t len, char **out);

/* PresentationML: text of a single ppt/slides/slideN.xml. Each <a:p> becomes
 * one line; slide-level <a:t> runs are concatenated within their paragraph. */
int wubuoxml_pptx_text(const uint8_t *xml, size_t len, char **out);

/* SpreadsheetML: a whole workbook's readable text. `shared` points at the
 * xl/sharedStrings.xml bytes (may be NULL if the workbook inlines strings),
 * `sheets` is an array of {name, bytes, len} for each xl/worksheets/sheetN.xml
 * (n >= 1). Builds a TSV-ish dump: each sheet prefixed by its path, each row
 * on one line, cells separated by '\t'. Inline strings (<c t="inlineStr">) and
 * shared-string references (<c t="s"><v>N</v>) are both resolved. */
typedef struct { const char *name; const uint8_t *bytes; size_t len; } wubuoxml_sheet;
int wubuoxml_xlsx_text(const uint8_t *shared, size_t shared_len,
                       const wubuoxml_sheet *sheets, size_t nsheets, char **out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOXML_EXTRACT_H */
