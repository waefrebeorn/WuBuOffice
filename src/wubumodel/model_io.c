/* WS11 model -> OOXML writer (minimal, from-scratch).
 * Emits a valid .docx (WordprocessingML) from the unified model. One SECTION
 * maps to one logical section; BLOCK->w:p, PARAGRAPH->w:p, RUN->w:r/w:t.
 * The model core (model.c) stays I/O-free; all serialization lives here, per
 * the WS11 design principle "I/O is separate from the canonical model". */
#define _POSIX_C_SOURCE 200809L
#include "model_internal.h"
#include "../wubuoxml/package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* append a UTF-8 string, XML-escaping & < > */
static void emit_xml_text(char **dst, size_t *cap, size_t *len, const char *s) {
    for (; s && *s; s++) {
        const char *ent = NULL;
        switch (*s) {
            case '&': ent = "&amp;"; break;
            case '<': ent = "&lt;";  break;
            case '>': ent = "&gt;";  break;
            default: break;
        }
        if (ent) {
            size_t add = strlen(ent);
            if (*len + add + 1 > *cap) { *cap = (*len + add + 1) * 2 + 64; *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, ent, add); *len += add;
        } else {
            if (*len + 2 > *cap) { *cap = (*len + 2) * 2 + 64; *dst = realloc(*dst, *cap); }
            (*dst)[(*len)++] = *s;
        }
    }
}

/* Recursively serialize the node tree into WordprocessingML body.
 * SECTION/BLOCK are pure grouping (no element); PARAGRAPH -> <w:p>; RUN -> <w:r>. */
static void serialize_node(wubumodel_node *n, char **dst, size_t *cap, size_t *len) {
    if (!n) return;
    switch (n->kind) {
        case WUBUMODEL_PARAGRAPH: {
            if (*len + 8 > *cap) { *cap = (*len + 512); *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, "<w:p>", 5); *len += 5;
            for (wubumodel_node *c = n->first_child; c; c = c->next_sibling)
                serialize_node(c, dst, cap, len);
            if (*len + 9 > *cap) { *cap = (*len + 512); *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, "</w:p>", 6); *len += 6;
            break;
        }
        case WUBUMODEL_RUN: {
            const char *open = "<w:r><w:t xml:space=\"preserve\">";
            const char *close = "</w:t></w:r>";
            size_t olen = strlen(open), clen = strlen(close);
            if (*len + olen + 1 > *cap) { *cap = (*len + olen + 512); *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, open, olen); *len += olen;
            emit_xml_text(dst, cap, len, n->text);
            if (*len + clen + 1 > *cap) { *cap = (*len + clen + 512); *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, close, clen); *len += clen;
            break;
        }
        default:
            /* DOC/SECTION/BLOCK/CELL/SHAPE/CHART/TABLE/FIELD/LINK: grouping or
             * not yet represented in v1 docx output; recurse children. */
            for (wubumodel_node *c = n->first_child; c; c = c->next_sibling)
                serialize_node(c, dst, cap, len);
            break;
    }
}

int wubumodel_write_docx(const wubumodel_doc *doc, const char *path) {
    if (!doc || !path) return -1;
    char *body = NULL; size_t bcap = 0, blen = 0;
    /* document.xml header */
    const char *hdr =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>";
    size_t hlen = strlen(hdr);
    body = calloc(1, hlen + 256);
    if (!body) return -1;
    memcpy(body, hdr, hlen); blen = hlen;
    /* serialize top-level nodes (SECTION/BLOCK/PARAGRAPH) */
    for (size_t b = 0; b < WUBUMODEL_BUCKETS; b++)
        for (wubumodel_node *n = doc->nodes[b]; n; n = n->next)
            if (!n->parent) /* top-level nodes */
                serialize_node(n, &body, &bcap, &blen);
    const char *ftr = "</w:body></w:document>";
    size_t flen = strlen(ftr);
    if (blen + flen + 1 > bcap) { bcap = blen + flen + 16; body = realloc(body, bcap); }
    memcpy(body + blen, ftr, flen); blen += flen;
    body[blen] = 0;

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(body); return -1; }
    wubuoxml_package *pkg = wubuoxml_create(fp);
    if (!pkg) { fclose(fp); free(body); return -1; }
    int rc = 0;
    if (wubuoxml_add_default_type(pkg, "xml", "application/xml") != 0) rc = -1;
    if (rc == 0 && wubuoxml_add_override(pkg, "/word/document.xml",
            "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml") != 0) rc = -1;
    if (rc == 0 && wubuoxml_add_relationship(pkg, "",
            "word/document.xml",
            "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument") != 0) rc = -1;
    if (rc == 0 && wubuoxml_add_part(pkg, "word/document.xml", body, blen) != 0) rc = -1;
    if (rc == 0 && wubuoxml_finalize(pkg) != 0) rc = -1;
    free(body);
    /* wubuoxml_finalize closes the underlying FILE? If not, we fclose. */
    fclose(fp);
    return rc;
}
