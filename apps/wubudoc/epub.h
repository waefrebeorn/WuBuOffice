/* epub.h -- EPUB (.epub) writer over the WuBuOffice document model.
 *
 * EPUB is a ZIP container (like ODF: "mimetype" stored first, uncompressed)
 * carrying XHTML content + an OPF package manifest + navigation. We reuse:
 *   - wubuzip           for the container,
 *   - wubudoc's HTML body renderer (wubudoc_render_html_body) for the content,
 *   - the dm_doc model  as the source (so any docx/md/odt/etc. -> epub).
 *
 * The document is split into chapters at each Heading1/Title so the spine and
 * table of contents are meaningful. Clean-room, from-scratch (SLERM). */

#ifndef WUBUDOC_EPUB_H
#define WUBUDOC_EPUB_H

#include "../wubuedit/docmodel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Write `d` as a valid EPUB 3 (with EPUB2 NCX fallback) at `path`.
 * Returns 0 on success, non-zero on error. */
int wubudoc_write_epub(const dm_doc *d, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WUBUDOC_EPUB_H */
