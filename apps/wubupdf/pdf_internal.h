/* pdf_internal.h -- shared types/helpers for the wubupdf writer modules
 * (pdf_text.c: encoding + wrapping; pdf_write.c: object serialization). */

#ifndef WUBUPDF_INTERNAL_H
#define WUBUPDF_INTERNAL_H

#include <stddef.h>

/* Page geometry (US Letter, 1-inch margins), in PDF points (1/72"). */
#define PDF_PAGE_W    612.0
#define PDF_PAGE_H    792.0
#define PDF_MARGIN     72.0
#define PDF_TEXT_W    (PDF_PAGE_W - 2 * PDF_MARGIN)   /* usable text width */
#define PDF_TOP_Y     (PDF_PAGE_H - PDF_MARGIN)
#define PDF_BOT_Y      PDF_MARGIN

/* A single laid-out text line on a page. */
typedef struct {
    char  *text;   /* WinAnsi-encoded, PDF-string-escaped bytes */
    double size;   /* font size in points */
    int    bold;   /* 1 => Helvetica-Bold (F2), else Helvetica (F1) */
    double gap_after; /* extra vertical space after this line (points) */
} pdf_line;

/* A page = an ordered list of lines. */
typedef struct {
    pdf_line *lines;
    size_t    n, cap;
} pdf_page;

/* Growable list of pages. */
typedef struct {
    pdf_page *pages;
    size_t    n, cap;
} pdf_doc;

/* --- pdf_text.c --- */

/* Convert a UTF-8 string to a WinAnsi, PDF-string-escaped, malloc'd buffer
 * (caller frees). Unmappable code points become '?'. */
char *pdf_encode_winansi(const char *utf8);

/* Estimate the rendered width (points) of an already-WinAnsi byte string at
 * `size`, using Helvetica AFM widths (bold flag selects the bold table). */
double pdf_text_width(const char *winansi, size_t len, double size, int bold);

/* Word-wrap a WinAnsi string to `max_w` points, appending each wrapped line to
 * `pg` (paginating into `doc` when the page is full). Advances layout state via
 * *y (current baseline). */
void pdf_emit_wrapped(pdf_doc *doc, pdf_page **pg, double *y,
                      const char *winansi, double size, int bold, double gap_after);

/* Free a pdf_doc's pages/lines. */
void pdf_doc_free(pdf_doc *doc);

/* Start a fresh page appended to `doc`; returns it and resets *y to the top. */
pdf_page *pdf_new_page(pdf_doc *doc, double *y);

#endif /* WUBUPDF_INTERNAL_H */
