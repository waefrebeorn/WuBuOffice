/* doc_md.h -- Markdown & HTML serializers for the WuBuOffice document model.
 *
 * Markdown import builds a wubuword_doc (which can then assemble a .docx or
 * feed any doc serializer). Markdown/HTML export walk a parsed dm_doc (from
 * word/document.xml) so any .docx can be down-converted to text formats.
 *
 * Supported Markdown constructs (import + export, symmetric):
 *   # / ## / ###   -> Heading1..3      (Title via a leading # kept as Heading1)
 *   **bold**       -> bold paragraph flag (paragraph-level, matching the model)
 *   - / *          -> bullet list item paragraphs
 *   | a | b |      -> tables (leading/trailing pipes optional; --- separator)
 *   blank line     -> paragraph break
 *
 * Clean-room, from-scratch (SLERM): no third-party Markdown/HTML code. */

#ifndef WUBUDOC_DOC_MD_H
#define WUBUDOC_DOC_MD_H

#include <stddef.h>
#include "../wubuword/word.h"
#include "../wubuedit/docmodel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parse a Markdown file into a fresh wubuword_doc. Caller wubuword_free()s it.
 * Returns 0 on success. */
int wubudoc_read_md(const char *path, wubuword_doc **out);

/* Export a parsed document model to Markdown at `path`. Returns 0 on success. */
int wubudoc_write_md(const dm_doc *d, const char *path);

/* Export a parsed document model to a standalone HTML file. Returns 0 on ok. */
int wubudoc_write_html(const dm_doc *d, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WUBUDOC_DOC_MD_H */
