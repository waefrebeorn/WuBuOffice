#ifndef WUBUWORD_ASSEMBLE_H
#define WUBUWORD_ASSEMBLE_H

#include "../wubuoxml/package.h"
#include "word.h"

/* Assemble a complete .docx package from a pre-rendered word/document.xml
 * payload. Writes the file at `outpath`. Returns 0 on success. */
int wubuword_assemble(const char *outpath, const void *doc_xml, size_t doc_len);

/* Assemble from a wubuword_doc directly. When the document used lists, also
 * emits word/numbering.xml (with bullet + decimal definitions) and wires the
 * content-type override, relationship, and document.xml reference. Returns 0. */
int wubuword_assemble_doc(const char *outpath, wubuword_doc *doc);

/* Returns the numbering definition XML (bullet + decimal) as a heap buffer the
 * caller owns (or NULL on OOM). Used when a document contains lists. */
char *wubuword_numbering_xml(size_t *out_len);

#endif /* WUBUWORD_ASSEMBLE_H */
