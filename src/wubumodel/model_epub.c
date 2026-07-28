/* model_epub.c -- EPUB3 (.epub) import for the unified model
 * (EXP-82 import half; the writer half ships in src/wubuepub/epub.c).
 *
 * An .epub is a ZIP containing:
 *   mimetype                 (stored, uncompressed, "application/epub+zip")
 *   META-INF/container.xml  (points at the OPF package document)
 *   OEBPS/content.opf       (manifest + spine)
 *   OEBPS/chap_N.xhtml       (the OEBPS content; one or more chapters)
 *
 * The XHTML is the part we model. Mapping (mirrors src/wubuepub/epub.c's
 * emit_block so the round-trip preserves structure):
 *   <h1>..<h6>  -> new SECTION + PARAGRAPH with named style HeadingN
 *   <p> <li>     -> PARAGRAPH (under the current section)
 *   <table><tr><td>/<th> -> TABLE -> CELL(row) -> CELL(cell) -> RUN(s)
 *   <a href>     -> LINK (target via set_link) wrapping a RUN (visible text)
 *
 * Self-contained: from-scratch ZIP read (wubuzip) + our own wubuxml SAX.
 * No third-party deps. */

#include "model.h"
#include "model_internal.h"
#include "../wubuzip/reader.h"
#include "../wubuxml/parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------- content (XHTML) parser ---------- */

typedef struct {
    wubumodel_doc *doc;
    wubumodel_node *stack[64];
    int sp;
    int in_text;
    char *textbuf;
    size_t tcap, tlen;
    wubumodel_node *cur_par;   /* active paragraph (NULL inside a cell/link) */
    int in_heading;             /* current section was opened by a heading */
    int in_link;                /* text should land inside the top LINK */
    wubumodel_node *link;      /* the open LINK node (== stack top when in_link) */
} epub_ctx_t;

static wubumodel_node *epub_top(epub_ctx_t *c) {
    return c->sp > 0 ? c->stack[c->sp - 1] : NULL;
}
static void epub_push(epub_ctx_t *c, wubumodel_node *n) {
    if (c->sp < 64) c->stack[c->sp++] = n;
}
static void epub_pop(epub_ctx_t *c) { if (c->sp > 0) c->sp--; }

/* topmost SECTION on the stack (or NULL) */
static wubumodel_node *epub_top_section(epub_ctx_t *c) {
    for (int i = c->sp - 1; i >= 0; i--)
        if (wubumodel_node_kind(c->stack[i]) == WUBUMODEL_SECTION)
            return c->stack[i];
    return NULL;
}

/* container that the next run of text should be appended to */
static wubumodel_node *epub_text_target(epub_ctx_t *c) {
    if (c->in_link && c->link) return c->link;
    if (c->cur_par) return c->cur_par;
    wubumodel_node *t = epub_top(c);
    if (t && wubumodel_node_kind(t) == WUBUMODEL_CELL) return t; /* cell -> RUN direct */
    return NULL;
}

static void epub_flush(epub_ctx_t *c) {
    if (c->tlen == 0) return;
    wubumodel_node *cont = epub_text_target(c);
    if (!cont) { c->tlen = 0; return; }
    wubumodel_node *run = wubumodel_node_create(c->doc, WUBUMODEL_RUN);
    if (!run) { c->tlen = 0; return; }
    c->textbuf[c->tlen] = 0;
    wubumodel_run_set_text(run, c->textbuf);
    wubumodel_node_append(c->doc, cont, run);
    c->tlen = 0;
}

/* ensure there is at least one SECTION to hang content on */
static wubumodel_node *epub_ensure_section(epub_ctx_t *c) {
    wubumodel_node *s = epub_top_section(c);
    if (!s) {
        s = wubumodel_node_create(c->doc, WUBUMODEL_SECTION);
        epub_push(c, s);
    }
    return s;
}

static wubumodel_node *epub_new_paragraph(epub_ctx_t *c, wubumodel_node *parent) {
    wubumodel_node *par = wubumodel_node_create(c->doc, WUBUMODEL_PARAGRAPH);
    if (!par) return NULL;
    wubumodel_node_append(c->doc, parent, par);
    c->cur_par = par;
    return par;
}

static int epub_on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    epub_ctx_t *c = user;

    if (evt == WUBUXML_EVT_TEXT) {
        /* XHTML carries character data directly inside <p>/<hN>/<td>/<a>
         * (no dedicated wrapper element), so we buffer text whenever the
         * current context has a real target to drop it into; epub_flush()
         * discards buffered text when there is no target (e.g. inside
         * <head>/<style>). */
        if (info->text_len && epub_text_target(c)) {
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

    if (evt == WUBUXML_EVT_START) {
        if (strcmp(nm, "h1") == 0 || strcmp(nm, "h2") == 0 ||
            strcmp(nm, "h3") == 0 || strcmp(nm, "h4") == 0 ||
            strcmp(nm, "h5") == 0 || strcmp(nm, "h6") == 0) {
            epub_flush(c);
            if (c->in_heading) epub_pop(c);   /* close previous heading's section */
            wubumodel_node *sec = wubumodel_node_create(c->doc, WUBUMODEL_SECTION);
            epub_push(c, sec);
            wubumodel_node *par = wubumodel_node_create(c->doc, WUBUMODEL_PARAGRAPH);
            wubumodel_node_append(c->doc, sec, par);
            c->cur_par = par;
            int lvl = nm[1] - '0';
            char style[16];
            snprintf(style, sizeof style, "Heading%d", lvl);
            wubumodel_node_apply_named_style(par, style);
            c->in_heading = 1;
            return 0;
        }
        if (strcmp(nm, "p") == 0 || strcmp(nm, "li") == 0) {
            epub_flush(c);
            wubumodel_node *sec = epub_ensure_section(c);
            epub_new_paragraph(c, sec);
            return 0;
        }
        if (strcmp(nm, "table") == 0) {
            epub_flush(c);
            wubumodel_node *sec = epub_ensure_section(c);
            wubumodel_node *tbl = wubumodel_node_create(c->doc, WUBUMODEL_TABLE);
            wubumodel_node_append(c->doc, sec, tbl);
            epub_push(c, tbl);
            return 0;
        }
        if (strcmp(nm, "tr") == 0) {
            epub_flush(c);
            wubumodel_node *tbl = epub_top(c);
            wubumodel_node *row = wubumodel_node_create(c->doc, WUBUMODEL_CELL);
            if (tbl) wubumodel_node_append(c->doc, tbl, row);
            epub_push(c, row);
            return 0;
        }
        if (strcmp(nm, "td") == 0 || strcmp(nm, "th") == 0) {
            epub_flush(c);
            wubumodel_node *row = epub_top(c);
            wubumodel_node *cell = wubumodel_node_create(c->doc, WUBUMODEL_CELL);
            if (row) wubumodel_node_append(c->doc, row, cell);
            epub_push(c, cell);   /* runs land directly in the cell */
            return 0;
        }
        if (strcmp(nm, "a") == 0) {
            epub_flush(c);   /* any text before the link stays in the paragraph */
            (void)epub_ensure_section(c);  /* make sure there is a host section */
            wubumodel_node *link = wubumodel_node_create(c->doc, WUBUMODEL_LINK);
            if (!link) return -1;
            const char *href = NULL;
            for (int i = 0; i < info->attr_count; i++)
                if (strcmp(info->attr_name[i], "href") == 0) { href = info->attr_val[i]; break; }
            if (href) wubumodel_node_set_link(link, href);
            epub_push(c, link);
            c->link = link;
            c->in_link = 1;
            return 0;
        }
        /* other elements (body, div, section, span, em, strong, br, img, ...):
         * treated as transparent grouping; their character data still flows to
         * the active paragraph/cell/link via epub_flush above. */
        return 0;
    }

    /* EVT_END */
    if (strcmp(nm, "h1") == 0 || strcmp(nm, "h2") == 0 ||
        strcmp(nm, "h3") == 0 || strcmp(nm, "h4") == 0 ||
        strcmp(nm, "h5") == 0 || strcmp(nm, "h6") == 0) {
        epub_flush(c);
        if (c->in_heading) epub_pop(c);
        c->in_heading = 0;
        c->cur_par = NULL;
        return 0;
    }
    if (strcmp(nm, "p") == 0 || strcmp(nm, "li") == 0) {
        epub_flush(c);
        c->cur_par = NULL;
        return 0;
    }
    if (strcmp(nm, "td") == 0 || strcmp(nm, "th") == 0 ||
        strcmp(nm, "tr") == 0 || strcmp(nm, "table") == 0) {
        epub_flush(c);
        epub_pop(c);
        return 0;
    }
    if (strcmp(nm, "a") == 0) {
        epub_flush(c);
        wubumodel_node *link = c->link;
        epub_pop(c);
        c->in_link = 0;
        c->link = NULL;
        /* hang the link under the current paragraph (or section) */
        wubumodel_node *host = c->cur_par ? c->cur_par : epub_top_section(c);
        if (link && host) wubumodel_node_append(c->doc, host, link);
        return 0;
    }
    return 0;
}

/* ---------- container.xml / OPF readers ---------- */

typedef struct {
    char rootfile[512];
    int has_root;
} cont_ctx_t;

static int cont_on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    cont_ctx_t *c = user;
    if (evt == WUBUXML_EVT_START && strcmp(info->name, "rootfile") == 0) {
        for (int i = 0; i < info->attr_count; i++)
            if (strcmp(info->attr_name[i], "full-path") == 0) {
                strncpy(c->rootfile, info->attr_val[i], sizeof c->rootfile - 1);
                c->rootfile[sizeof c->rootfile - 1] = 0;
                c->has_root = 1;
            }
    }
    return 0;
}

#define MAX_ITEMS 256
typedef struct {
    char id[MAX_ITEMS][64];
    char href[MAX_ITEMS][512];
    int  is_xhtml[MAX_ITEMS];
    int  n;
    char spine_idref[256][64];
    int  ns;
} opf_ctx_t;

static int opf_on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    opf_ctx_t *c = user;
    if (evt != WUBUXML_EVT_START) return 0;
    if (strcmp(info->name, "item") == 0) {
        const char *id = NULL, *href = NULL, *mt = NULL;
        for (int i = 0; i < info->attr_count; i++) {
            if (strcmp(info->attr_name[i], "id") == 0) id = info->attr_val[i];
            else if (strcmp(info->attr_name[i], "href") == 0) href = info->attr_val[i];
            else if (strcmp(info->attr_name[i], "media-type") == 0) mt = info->attr_val[i];
        }
        if (href && c->n < MAX_ITEMS) {
            strncpy(c->href[c->n], href, sizeof c->href[0] - 1);
            if (id) strncpy(c->id[c->n], id, sizeof c->id[0] - 1);
            c->is_xhtml[c->n] = (mt && strcmp(mt, "application/xhtml+xml") == 0);
            c->n++;
        }
    } else if (strcmp(info->name, "itemref") == 0) {
        for (int i = 0; i < info->attr_count; i++)
            if (strcmp(info->attr_name[i], "idref") == 0 && c->ns < 256) {
                strncpy(c->spine_idref[c->ns], info->attr_val[i], sizeof c->spine_idref[0] - 1);
                c->ns++;
            }
    }
    return 0;
}

/* pick the first content XHTML document to import (spine order preferred),
 * returning its FULL zip path (OPF dir + relative href). `opffile` is the
 * container rootfile (e.g. "OEBPS/content.opf"); the href is relative to
 * its directory. Writes the joined path into `out_full` (caller buffer,
 * >=512) and returns it, or NULL if no xhtml found. */
static const char *epub_pick_xhtml(opf_ctx_t *o, const char *opffile,
                                  char *out_full, size_t cap) {
    const char *href = NULL;
    for (int i = 0; i < o->ns; i++)
        for (int j = 0; j < o->n; j++)
            if (strcmp(o->id[j], o->spine_idref[i]) == 0 && o->is_xhtml[j])
                { href = o->href[j]; break; }
    if (!href)
        for (int j = 0; j < o->n; j++)
            if (o->is_xhtml[j]) { href = o->href[j]; break; }
    if (!href) return NULL;

    /* derive OPF directory ("OEBPS/" from "OEBPS/content.opf") */
    const char *slash = strrchr(opffile, '/');
    size_t dlen = slash ? (size_t)(slash - opffile) + 1 : 0;
    if (dlen) {
        if (dlen + strlen(href) + 1 > cap) return NULL;
        memcpy(out_full, opffile, dlen);
        strcpy(out_full + dlen, href);
    } else {
        if (strlen(href) + 1 > cap) return NULL;
        strcpy(out_full, href);
    }
    return out_full;
}

int wubumodel_load_epub(const char *path, wubumodel_doc **out) {
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

    /* 1) container.xml -> OPF path */
    size_t ci = wubuzip_find(&z, "META-INF/container.xml");
    if (ci == (size_t)-1) { wubuzip_close(&z); free(data); return -1; }
    uint8_t *cont = NULL; size_t cont_len = 0;
    if (wubuzip_extract(&z, ci, &cont, &cont_len) != 0) { wubuzip_close(&z); free(data); return -1; }
    cont_ctx_t cc; memset(&cc, 0, sizeof cc);
    wubuxml_parse(cont, cont_len, cont_on_event, &cc);
    free(cont);
    if (!cc.has_root) { wubuzip_close(&z); free(data); return -1; }

    /* 2) OPF -> pick content document */
    size_t oi = wubuzip_find(&z, cc.rootfile);
    if (oi == (size_t)-1) { wubuzip_close(&z); free(data); return -1; }
    uint8_t *opf = NULL; size_t opf_len = 0;
    if (wubuzip_extract(&z, oi, &opf, &opf_len) != 0) { wubuzip_close(&z); free(data); return -1; }
    opf_ctx_t oc; memset(&oc, 0, sizeof oc);
    wubuxml_parse(opf, opf_len, opf_on_event, &oc);
    free(opf);
    char xfull[512];
    const char *xhtml = epub_pick_xhtml(&oc, cc.rootfile, xfull, sizeof xfull);
    if (!xhtml) { wubuzip_close(&z); free(data); return -1; }

    /* 3) extract + parse the chosen XHTML content */
    size_t xi = wubuzip_find(&z, xhtml);
    if (xi == (size_t)-1) { wubuzip_close(&z); free(data); return -1; }
    uint8_t *xh = NULL; size_t xh_len = 0;
    if (wubuzip_extract(&z, xi, &xh, &xh_len) != 0) { wubuzip_close(&z); free(data); return -1; }

    wubumodel_doc *d = wubumodel_doc_create();
    if (!d) { free(xh); wubuzip_close(&z); free(data); return -1; }
    epub_ctx_t c; memset(&c, 0, sizeof c); c.doc = d;
    int rc = wubuxml_parse(xh, xh_len, epub_on_event, &c);

    free(c.textbuf);
    free(xh);
    wubuzip_close(&z);
    free(data);

    if (rc != 0) { wubumodel_doc_destroy(d); return -1; }
    if (!wubumodel_doc_root(d)) { wubumodel_doc_destroy(d); return -1; }
    *out = d;
    return 0;
}
