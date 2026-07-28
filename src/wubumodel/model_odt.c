/* model_odt.c -- OpenDocument Text (.odt) import/export for the unified model
 * (DOC-78: ODF/ODT import-export parity, sibling to the DOCX round-trip).
 *
 * An .odt is a ZIP containing:
 *   mimetype                 (stored, uncompressed, exactly the ODF text mime)
 *   META-INF/manifest.xml    (lists the parts)
 *   content.xml              (office:document-content with the body text)
 *
 * content.xml maps to the model as:
 *   office:body/office:text -> SECTION (grouping; not emitted as an element)
 *   text:p                  -> WUBUMODEL_PARAGRAPH
 *   text:span / character data inside text:p -> WUBUMODEL_RUN (text)
 *   table:table / table:table-row / table:table-cell -> TABLE/CELL (best effort)
 *
 * Self-contained: uses wubuzip (from-scratch ZIP) for packaging, wubuxml SAX
 * for parsing. No third-party deps. */

#include "model.h"
#include "model_internal.h"
#include "../wubuzip/zip.h"
#include "../wubuzip/reader.h"
#include "../wubuxml/parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------- shared XML text escaping ---------- */

static void xml_escape(const char *s, char **dst, size_t *cap, size_t *len) {
    if (!s) return;
    for (; *s; s++) {
        const char *ent = NULL;
        switch (*s) {
            case '&': ent = "&amp;"; break;
            case '<': ent = "&lt;";  break;
            case '>': ent = "&gt;";  break;
            case '"': ent = "&quot;"; break;
            default:  break;
        }
        size_t need = ent ? strlen(ent) : 1;
        if (*len + need + 1 > *cap) {
            *cap = (*len + need + 256) * 2;
            char *nb = realloc(*dst, *cap);
            if (!nb) return;
            *dst = nb;
        }
        if (ent) { memcpy(*dst + *len, ent, need); *len += need; }
        else     { (*dst)[(*len)++] = *s; }
    }
    (*dst)[*len] = 0;
}

/* ---------- writer ---------- */

/* Serialize a node + its children into content.xml body. Returns 0. */
static void serialize_odt(wubumodel_node *n, char **dst, size_t *cap, size_t *len) {
    if (!n) return;
    switch (wubumodel_node_kind(n)) {
        case WUBUMODEL_SECTION:
        case WUBUMODEL_BLOCK:
            /* grouping only */
            for (wubumodel_node *c = wubumodel_node_first_child(n); c;
                 c = wubumodel_node_next_sibling(c))
                serialize_odt(c, dst, cap, len);
            break;
        case WUBUMODEL_PARAGRAPH: {
            const char *open = "<text:p>";
            size_t ol = strlen(open);
            if (*len + ol + 1 > *cap) { *cap = *len + ol + 256; *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, open, ol); *len += ol;
            for (wubumodel_node *c = wubumodel_node_first_child(n); c;
                 c = wubumodel_node_next_sibling(c))
                serialize_odt(c, dst, cap, len);
            const char *close = "</text:p>";
            size_t cl = strlen(close);
            if (*len + cl + 1 > *cap) { *cap = *len + cl + 256; *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, close, cl); *len += cl;
            break;
        }
        case WUBUMODEL_RUN: {
            const char *t = wubumodel_run_text(n);
            const char *open = "<text:span>";
            size_t ol = strlen(open);
            if (*len + ol + 1 > *cap) { *cap = *len + ol + 256; *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, open, ol); *len += ol;
            xml_escape(t, dst, cap, len);
            const char *close = "</text:span>";
            size_t cl = strlen(close);
            if (*len + cl + 1 > *cap) { *cap = *len + cl + 256; *dst = realloc(*dst, *cap); }
            memcpy(*dst + *len, close, cl); *len += cl;
            break;
        }
        default:
            /* other kinds (SHAPE/CHART/etc.) are not represented in v1 ODT;
             * serialize any run children they may carry. */
            for (wubumodel_node *c = wubumodel_node_first_child(n); c;
                 c = wubumodel_node_next_sibling(c))
                serialize_odt(c, dst, cap, len);
            break;
    }
}

int wubumodel_write_odt(const wubumodel_doc *doc, const char *path) {
    if (!doc || !path) return -1;
    char *body = calloc(1, 256);
    size_t bcap = 256, blen = 0;
    if (!body) return -1;

    static const char *hdr =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<office:document-content xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
        "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\">\n"
        "<office:body><office:text>";
    size_t hlen = strlen(hdr);
    memcpy(body, hdr, hlen); blen = hlen;

    /* top-level nodes in creation order */
    wubumodel_node **top = NULL; size_t ntop = 0, tcap = 0;
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
    for (size_t a = 1; a < ntop; a++) {
        wubumodel_node *key = top[a]; size_t j = a;
        while (j > 0 && top[j - 1]->id > key->id) { top[j] = top[j - 1]; j--; }
        top[j] = key;
    }
    for (size_t a = 0; a < ntop; a++)
        serialize_odt(top[a], &body, &bcap, &blen);
    free(top);

    static const char *ftr[] = {
        "</office:text></office:body></office:document-content>",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\">\n"
        "<manifest:file-entry manifest:media-type=\"application/vnd.oasis.opendocument.text\" manifest:full-path=\"/\"/>\n"
        "<manifest:file-entry manifest:media-type=\"text/xml\" manifest:full-path=\"content.xml\"/>\n"
        "</manifest:manifest>",
        "application/vnd.oasis.opendocument.text"
    };
    size_t flen = strlen(ftr[0]);
    if (blen + flen + 1 > bcap) { bcap = blen + flen + 16; body = realloc(body, bcap); }
    memcpy(body + blen, ftr[0], flen); blen += flen; body[blen] = 0;

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(body); return -1; }
    wubuzip_writer *z = wubuzip_create(fp);
    if (!z) { fclose(fp); free(body); return -1; }
    int rc = 0;
    /* mimetype MUST be stored (uncompressed) and first */
    if (wubuzip_add(z, "mimetype", ftr[2], (uint32_t)strlen(ftr[2])) != 0) rc = -1;
    if (rc == 0 && wubuzip_add_deflated(z, "META-INF/manifest.xml", ftr[1], (uint32_t)strlen(ftr[1])) != 0) rc = -1;
    if (rc == 0 && wubuzip_add_deflated(z, "content.xml", body, (uint32_t)blen) != 0) rc = -1;
    if (rc == 0 && wubuzip_finalize(z) != 0) rc = -1;
    free(body);
    /* wubuzip_finalize closes fp */
    return rc;
}

/* ---------- reader ---------- */

typedef struct {
    wubumodel_doc *doc;
    wubumodel_node *stack[64];
    int sp;
    int in_text;
    char *textbuf;
    size_t tcap, tlen;
} odt_ctx_t;

static wubumodel_node *odt_top(odt_ctx_t *c) {
    return c->sp > 0 ? c->stack[c->sp - 1] : NULL;
}
static void odt_push(odt_ctx_t *c, wubumodel_node *n) {
    if (c->sp < 64) c->stack[c->sp++] = n;
}
static void odt_pop(odt_ctx_t *c) { if (c->sp > 0) c->sp--; }

static void odt_flush(odt_ctx_t *c) {
    if (c->tlen == 0) return;
    wubumodel_node *par = odt_top(c);
    if (!par || wubumodel_node_kind(par) != WUBUMODEL_PARAGRAPH) { c->tlen = 0; return; }
    wubumodel_node *run = wubumodel_node_create(c->doc, WUBUMODEL_RUN);
    c->textbuf[c->tlen] = 0;
    wubumodel_run_set_text(run, c->textbuf);
    wubumodel_node_append(c->doc, par, run);
    c->tlen = 0;
}

static int odt_on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    odt_ctx_t *c = user;
    if (evt == WUBUXML_EVT_TEXT) {
        if (c->in_text && info->text_len) {
            if (c->tlen + info->text_len + 1 > c->tcap) {
                c->tcap = (c->tlen + info->text_len) * 2 + 64;
                char *nb = realloc(c->textbuf, c->tcap);
                if (!nb) return -1;
                c->textbuf = nb;
            }
            memcpy(c->textbuf + c->tlen, info->text, info->text_len);
            c->tlen += info->text_len;
        }
        return 0;
    }
    const char *nm = info->name;
    int is_text = (strncmp(nm, "text:", 5) == 0);
    const char *local = is_text ? nm + 5 : nm;
    if (evt == WUBUXML_EVT_START) {
        if (is_text && strcmp(local, "p") == 0) {
            odt_flush(c);
            wubumodel_node *par = wubumodel_node_create(c->doc, WUBUMODEL_PARAGRAPH);
            wubumodel_node *parent = odt_top(c);
            if (!parent) { /* under office:text (no section yet) -> make one */
                parent = wubumodel_node_create(c->doc, WUBUMODEL_SECTION);
                wubumodel_node_append(c->doc, parent, par); /* SECTION owns PARAGRAPH chain */
                odt_push(c, parent); /* section becomes the active container */
                odt_push(c, par);
            } else {
                wubumodel_node_append(c->doc, parent, par);
                odt_push(c, par);
            }
        } else if (is_text && strcmp(local, "span") == 0) {
            c->in_text = 1; c->tlen = 0;
        }
        return 0;
    }
    /* EVT_END */
    if (is_text && strcmp(local, "span") == 0) {
        c->in_text = 0; odt_flush(c);
    } else if (is_text && strcmp(local, "p") == 0) {
        odt_flush(c);
        odt_pop(c); /* paragraph */
    }
    return 0;
}

int wubumodel_load_odt(const char *path, wubumodel_doc **out) {
    if (!path || !out) return -1;
    *out = NULL;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return -1; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return -1; }
    uint8_t *data = malloc((size_t)sz ? (size_t)sz : 1);
    if (!data) { fclose(fp); return -1; }
    if (fread(data, 1, (size_t)sz, fp) != (size_t)sz) { fclose(fp); free(data); return -1; }
    fclose(fp);

    wubuzip_archive z;
    if (wubuzip_open(data, (size_t)sz, &z) != 0) { free(data); return -1; }
    size_t ci = wubuzip_find(&z, "content.xml");
    if (ci == (size_t)-1) { wubuzip_close(&z); free(data); return -1; }
    uint8_t *cx = NULL; size_t cxlen = 0;
    if (wubuzip_extract(&z, ci, &cx, &cxlen) != 0) { wubuzip_close(&z); free(data); return -1; }

    wubumodel_doc *d = wubumodel_doc_create();
    if (!d) { free(cx); wubuzip_close(&z); free(data); return -1; }
    odt_ctx_t c; memset(&c, 0, sizeof c); c.doc = d;
    int rc = wubuxml_parse(cx, cxlen, odt_on_event, &c);
    free(c.textbuf);
    free(cx);
    wubuzip_close(&z);
    free(data);
    if (rc != 0) { wubumodel_doc_destroy(d); return -1; }
    *out = d;
    return 0;
}
