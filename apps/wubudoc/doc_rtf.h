/* doc_rtf.h -- Rich Text Format (RTF 1.x) writer over the document model.
 *
 * RTF is WordprocessingML's ancestor: a plain-text control-word format every
 * word processor on earth reads. We emit a minimal but valid RTF from a parsed
 * dm_doc (headings as bold+larger font, bold runs, tables via \trowd/\cell).
 *
 * Clean-room, from-scratch (SLERM): no third-party RTF code. */

#ifndef WUBUDOC_DOC_RTF_H
#define WUBUDOC_DOC_RTF_H

#include "../wubuedit/docmodel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Export a parsed document model to an RTF file. Returns 0 on success. */
int wubudoc_write_rtf(const dm_doc *d, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WUBUDOC_DOC_RTF_H */
