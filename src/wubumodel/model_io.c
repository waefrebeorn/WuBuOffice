/* WS11 model -> OOXML writer (minimal, from-scratch).
 * Emits a valid .docx (WordprocessingML) from the unified model. One SECTION
 * maps to one logical section; BLOCK->w:p, PARAGRAPH->w:p, RUN->w:r/w:t.
 * The model core (model.c) stays I/O-free; all serialization lives here, per
 * the WS11 design principle "I/O is separate from the canonical model". */
#define _POSIX_C_SOURCE 200809L
#include "model_internal.h"
#include "../wubuoxml/package.h"
#include "../wubuoxml/reader.h"
#include "../wubuoxml/docx_text.h"
#include "../wubuoxml/docx_document.h"
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
 * SECTION/BLOCK are pure grouping (no element); PARAGRAPH -> <w:p>;
 * RUN -> <w:r>/<w:t>; TABLE -> <w:tbl>, its CELL children ->
 * <w:tr> (rows), those CELL children -> <w:tc> (cells) holding
 * a nested <w:p>. `tbl` tracks table nesting depth so a CELL is
 * emitted as the right element. */
static void serialize_node(wubumodel_node *n, char **dst, size_t *cap,
                           size_t *len, int tbl) {
    if (!n) return;
    switch (n->kind) {
        case WUBUMODEL_PARAGRAPH: {
            if (*len + 8 > *cap) { *cap = (*len + 512); *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, "<w:p>", 5); *len += 5;
            for (wubumodel_node *c = n->first_child; c; c = c->next_sibling)
                serialize_node(c, dst, cap, len, tbl);
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
        case WUBUMODEL_TABLE: {
            if (*len + 9 > *cap) { *cap = (*len + 512); *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, "<w:tbl>", 7); *len += 7;
            for (wubumodel_node *c = n->first_child; c; c = c->next_sibling)
                serialize_node(c, dst, cap, len, 1);   /* children = rows */
            if (*len + 10 > *cap) { *cap = (*len + 512); *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, "</w:tbl>", 8); *len += 8;
            break;
        }
        case WUBUMODEL_CELL: {
            if (tbl == 1) {   /* a row: emit <w:tr> */
                if (*len + 7 > *cap) { *cap = (*len + 512); *dst = realloc(*dst, *cap); }
                memcpy(*dst + *len, "<w:tr>", 6); *len += 6;
                for (wubumodel_node *c = n->first_child; c; c = c->next_sibling)
                    serialize_node(c, dst, cap, len, 2);   /* children = cells */
                if (*len + 8 > *cap) { *cap = (*len + 512); *dst = realloc(*dst, *cap); }
                memcpy(*dst + *len, "</w:tr>", 7); *len += 7;
            } else if (tbl == 2) {   /* a cell: emit <w:tc> */
                if (*len + 7 > *cap) { *cap = (*len + 512); *dst = realloc(*dst, *cap); }
                memcpy(*dst + *len, "<w:tc>", 6); *len += 6;
                for (wubumodel_node *c = n->first_child; c; c = c->next_sibling)
                    serialize_node(c, dst, cap, len, 2);   /* nested para/run */
                if (*len + 8 > *cap) { *cap = (*len + 512); *dst = realloc(*dst, *cap); }
                memcpy(*dst + *len, "</w:tc>", 7); *len += 7;
            } else {   /* a CELL used outside a table: treat as grouping */
                for (wubumodel_node *c = n->first_child; c; c = c->next_sibling)
                    serialize_node(c, dst, cap, len, tbl);
            }
            break;
        }
        default:
            /* DOC/SECTION/BLOCK/SHAPE/CHART/FIELD/LINK: grouping or
             * not yet represented in v1 docx output; recurse children. */
            for (wubumodel_node *c = n->first_child; c; c = c->next_sibling)
                serialize_node(c, dst, cap, len, tbl);
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
    /* serialize top-level nodes in CREATION order (by id), not bucket
     * order, so output is deterministic + matches authoring order. */
    wubumodel_node **top = NULL;
    size_t ntop = 0, tcap = 0;
    for (size_t b = 0; b < WUBUMODEL_BUCKETS; b++)
        for (wubumodel_node *n = doc->nodes[b]; n; n = n->next)
            if (!n->parent) {
                if (ntop == tcap) {
                    tcap = tcap ? tcap * 2 : 16;
                    wubumodel_node **nt = realloc(top, tcap * sizeof *nt);
                    if (!nt) { free(top); free(body); return -1; }
                    top = nt;
                }
                top[ntop++] = n;
            }
    /* insertion sort by id (small N) */
    for (size_t a = 1; a < ntop; a++) {
        wubumodel_node *key = top[a];
        size_t j = a;
        while (j > 0 && top[j - 1]->id > key->id) { top[j] = top[j - 1]; j--; }
        top[j] = key;
    }
    for (size_t a = 0; a < ntop; a++)
        serialize_node(top[a], &body, &bcap, &blen, 0);
    free(top);
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
    /* wubuoxml_finalize closes the underlying FILE; we do NOT fclose. */
    return rc;
}

/* ---- OOXML load (ws05#0885 round-trip; feeds doc_cache + load_async) ---- */

static uint8_t *read_file(const char *path, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return NULL; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    uint8_t *buf = malloc((size_t)sz ? (size_t)sz : 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (got != (size_t)sz) { free(buf); return NULL; }
    *out_len = (size_t)sz;
    return buf;
}

int wubumodel_load_docx(const char *path, wubumodel_doc **out) {
    if (!path || !out) return -1;
    *out = NULL;

    size_t len = 0;
    uint8_t *data = read_file(path, &len);
    if (!data) return -1;

    wubuoxml_package pkg;
    if (wubuoxml_read(data, len, &pkg) != 0) {
        free(data);
        return -1;
    }

    const wubuoxml_part *doc = wubuoxml_part_find(&pkg, "word/document.xml");
    if (!doc) {
        free(data);
        wubuoxml_free(&pkg);
        return -1;   /* no document part -> not a docx we understand */
    }

    wubumodel_doc *d = wubumodel_doc_create();
    if (!d) {
        free(data);
        wubuoxml_free(&pkg);
        return -1;
    }

    /* Structural map: document.xml -> SECTION/(PARAGRAPH|TABLE)/... */
    if (wubuoxml_docx_to_model(doc->bytes, doc->len, d) != 0) {
        wubumodel_doc_destroy(d);
        free(data);
        wubuoxml_free(&pkg);
        return -1;
    }

    free(data);
    wubuoxml_free(&pkg);
    *out = d;
    return 0;
}
