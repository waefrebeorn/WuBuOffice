#ifndef WUBUOXML_DOCX_TEXT_H
#define WUBUOXML_DOCX_TEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Plain-text extraction from OOXML document parts, via the shared SAX
 * handler in extract.c. Every entry point heap-allocates a NUL-terminated,
 * caller-owned buffer (may be empty, never NULL on success) and returns 0,
 * or -1 on alloc failure. */

/* WordprocessingML: word/document.xml. <w:t> runs concatenated, <w:p> ends a
 * line. Entity decoding handled by the shared wubuxml parser. */
int wubuoxml_docx_text(const uint8_t *xml, size_t len, char **out);

/* PresentationML: a single ppt/slides/slideN.xml. <a:t> runs concatenated
 * within a paragraph; <a:p> ends a line. */
int wubuoxml_pptx_text(const uint8_t *xml, size_t len, char **out);

/* SpreadsheetML: a whole workbook. `shared` is xl/sharedStrings.xml (may be
 * NULL), `sheets` is one entry per xl/worksheets/sheetN.xml. Produces a
 * per-sheet TSV dump (sheet header line + one tab-separated row per <row>,
 * shared-string and inline-string cells both resolved). */
typedef struct { const char *name; const uint8_t *bytes; size_t len; } wubuoxml_sheet;
int wubuoxml_xlsx_text(const uint8_t *shared, size_t shared_len,
                       const wubuoxml_sheet *sheets, size_t nsheets, char **out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOXML_DOCX_TEXT_H */
