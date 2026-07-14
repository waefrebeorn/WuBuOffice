/* wuburead readers: build a dm_doc from non-OOXML text formats.
 *
 *   wuburead_rtf  -> dm_doc   (RTF token stream)
 *   wuburead_html -> dm_doc   (tag-based, reuses wubuxml SAX)
 *   wuburead_epub -> dm_doc   (ZIP of XHTML, fed through the HTML reader)
 *
 * All build dm_doc exclusively via wubuedit_docmodel_* (single owner of the
 * block-array layout), so there is no second copy of model construction.
 *
 * Clean-room C11. */

#ifndef WUBUREAD_READERS_H
#define WUBUREAD_READERS_H

#include <stddef.h>
#include <stdint.h>
#include "../wubuedit/docmodel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parse an RTF file into a dm_doc. Returns 0 on success, -1 on I/O or OOM. */
int wuburead_rtf(const char *path, dm_doc *out);

/* Parse an HTML file into a dm_doc. Returns 0 on success, -1 on I/O or OOM. */
int wuburead_html(const char *path, dm_doc *out);

/* Parse an EPUB file into a dm_doc (all XHTML content documents in spine
 * order). Returns 0 on success, -1 on I/O, malformed ZIP, or OOM. */
int wuburead_epub(const char *path, dm_doc *out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUREAD_READERS_H */
