/* wubupdf -- minimal PDF/1.7 writer over the TEXT model (dm_doc).
 *
 * Emits a standards-valid single-file PDF: catalog, pages tree, one or more
 * page objects, a shared Helvetica/Helvetica-Bold font, and a content stream
 * per page with text laid out top-to-bottom. Headings render larger, bold runs
 * use Helvetica-Bold, tables render as tab-separated text rows. Long lines wrap
 * and content paginates automatically.
 *
 * Clean-room C11, zero dependencies. Write-only (PDF is a fixed-layout target,
 * not part of the round-trip model set). */

#ifndef WUBUPDF_PDF_H
#define WUBUPDF_PDF_H

#include "../wubuedit/docmodel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Render `doc` to a PDF at `outpath`. Returns 0 on success, non-zero on
 * I/O/alloc failure. */
int wubupdf_write(const dm_doc *doc, const char *outpath);

#ifdef __cplusplus
}
#endif

#endif /* WUBUPDF_PDF_H */
