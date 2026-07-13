#ifndef WUBUWORD_ASSEMBLE_H
#define WUBUWORD_ASSEMBLE_H

#include "../wubuoxml/package.h"

/* Assemble a complete .docx package from a pre-rendered word/document.xml
 * payload. Writes the file at `outpath`. Returns 0 on success. */
int wubuword_assemble(const char *outpath, const void *doc_xml, size_t doc_len);

#endif /* WUBUWORD_ASSEMBLE_H */
