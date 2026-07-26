#include "assemble.h"
#include "word.h"
#include "word_internal.h"
#include "../wubuoxml/package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Static numbering definition: abstractNum #0 = bullet, #1 = decimal; two
 * concrete <w:num> instances (numId 1 -> bullet, numId 2 -> decimal). A real
 * Word file carries far richer formatting, but this minimal, well-formed
 * definition renders bullets and 1. 2. 3. lists in Word + LibreOffice. */
static const char *NUMBERING_XML =
"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
"<w:numbering xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
  "<w:abstractNum w:abstractNumId=\"0\">"
    "<w:multiLevelType w:val=\"hybridMultilevel\"/>"
    "<w:lvl w:ilvl=\"0\">"
      "<w:start w:val=\"1\"/><w:numFmt w:val=\"bullet\"/><w:lvlText w:val=\"•\"/>"
      "<w:lvlJc w:val=\"left\"/>"
      "<w:pPr><w:ind w:left=\"720\" w:hanging=\"360\"/></w:pPr>"
    "</w:lvl>"
  "</w:abstractNum>"
  "<w:abstractNum w:abstractNumId=\"1\">"
    "<w:multiLevelType w:val=\"hybridMultilevel\"/>"
    "<w:lvl w:ilvl=\"0\">"
      "<w:start w:val=\"1\"/><w:numFmt w:val=\"decimal\"/><w:lvlText w:val=\"%1.\"/>"
      "<w:lvlJc w:val=\"left\"/>"
      "<w:pPr><w:ind w:left=\"720\" w:hanging=\"360\"/></w:pPr>"
    "</w:lvl>"
  "</w:abstractNum>"
  "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"0\"/></w:num>"
  "<w:num w:numId=\"2\"><w:abstractNumId w:val=\"1\"/></w:num>"
"</w:numbering>";

char *wubuword_numbering_xml(size_t *out_len) {
    size_t n = strlen(NUMBERING_XML);
    char *buf = malloc(n + 1);
    if (!buf) return NULL;
    memcpy(buf, NUMBERING_XML, n + 1);
    if (out_len) *out_len = n;
    return buf;
}

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

int wubuword_assemble_doc(const char *outpath, wubuword_doc *doc) {
    size_t doc_len = 0;
    char *doc_xml = wubuword_render(doc, &doc_len);
    if (!doc_xml) return -1;

    FILE *out = fopen(outpath, "wb");
    if (!out) { perror("fopen"); free(doc_xml); return -1; }
    wubuoxml_package *pkg = wubuoxml_create(out);
    wubuoxml_add_default_type(pkg, "rels", "application/vnd.openxmlformats-package.relationships+xml");
    wubuoxml_add_default_type(pkg, "xml", "application/xml");
    wubuoxml_add_override(pkg, "/word/document.xml",
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml");

    if (wubuword_used_lists(doc)) {
        wubuoxml_add_override(pkg, "/word/numbering.xml",
            "application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml");
    }

    wubuoxml_add_relationship(pkg, "",
        "word/document.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");

    if (wubuword_used_lists(doc)) {
        wubuoxml_add_relationship(pkg, "word/document.xml", "word/numbering.xml",
            "http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering");
        size_t nl = 0;
        char *nxml = wubuword_numbering_xml(&nl);
        wubuoxml_add_part(pkg, "word/numbering.xml", nxml, nl);
        free(nxml);
    }

    wubuoxml_add_part(pkg, "word/document.xml", doc_xml, doc_len);
    int rc = wubuoxml_finalize(pkg);
    fclose(out);
    free(doc_xml);
    return rc;
}
