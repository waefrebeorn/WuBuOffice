#include "assemble.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wubuword_assemble(const char *outpath, const void *doc_xml, size_t doc_len) {
    FILE *out = fopen(outpath, "wb");
    if (!out) { perror("fopen"); return -1; }
    wubuoxml_package *pkg = wubuoxml_create(out);
    wubuoxml_add_default_type(pkg, "rels", "application/vnd.openxmlformats-package.relationships+xml");
    wubuoxml_add_default_type(pkg, "xml", "application/xml");
    wubuoxml_add_override(pkg, "/word/document.xml",
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml");
    wubuoxml_add_relationship(pkg, "",
        "word/document.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");
    wubuoxml_add_part(pkg, "word/document.xml", doc_xml, doc_len);
    int rc = wubuoxml_finalize(pkg);
    fclose(out);
    return rc;
}
