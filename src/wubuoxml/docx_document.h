#ifndef WUBUOXML_DOCX_DOCUMENT_H
#define WUBUOXML_DOCX_DOCUMENT_H

/* Map a WordprocessingML document.xml part into the unified object
 * model (ws05#0884 / ws05#082). Builds SECTION -> (PARAGRAPH |
 * TABLE); PARAGRAPH -> RUN (with text); TABLE -> ROW -> CELL ->
 * (PARAGRAPH). The element names match what model_io.c emits, so
 * write -> load -> write is self-compatible.
 *
 * Opaque mapper: callers pass the raw part bytes + a freshly
 * created wubumodel_doc; the mapper appends the tree under it. */

#include "../wubumodel/model.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Parse `xml`/`len` (the word/document.xml part) and append the
 * corresponding node tree to `doc`. Returns 0 on success, -1 on a
 * structural error. The doc keeps ownership of every node created. */
int wubuoxml_docx_to_model(const uint8_t *xml, size_t len,
                              wubumodel_doc *doc);

#ifdef __cplusplus
}
#endif
#endif /* WUBUOXML_DOCX_DOCUMENT_H */
