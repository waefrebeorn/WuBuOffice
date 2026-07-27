/* exp.h -- exporters: layout -> document formats (EXP cluster).
 * These are THIN consumers of wubulayout. The whole point of the central
 * pipeline: export is just "walk the laid-out pages and serialize". No
 * per-format text-wrapping reimplemented. C11, no third-party deps.
 *
 * Formats: Markdown, HTML, LaTeX, RTF, PDF (from-scratch minimal writer). */
#ifndef WUBUEXP_EXP_H
#define WUBUEXP_EXP_H

#include "ublayout.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* All return 0 ok, -1 on error. `out` is a file path. */

int wubuexp_markdown(const wubulayout_doc *L, const char *out);
int wubuexp_html(   const wubulayout_doc *L, const char *out);
int wubuexp_latex(  const wubulayout_doc *L, const char *out);
int wubuexp_rtf(    const wubulayout_doc *L, const char *out);
/* PDF: one page per layout page, Helvetica, logical text (no embedding of
 * fonts beyond the base-14). Good enough for archive/preview. */
int wubuexp_pdf(    const wubulayout_doc *L, const char *out);

#ifdef __cplusplus
}
#endif
#endif
