/* wubusvg.c -- clean-room SVG ingest + regurgitate. Native C11 + POSIX
 * open_memstream. Reuses the wubuxml SAX parser and XML writer. */
#include "wubusvg.h"
#include "parser.h"   /* wubuxml_parse (SAX) */
#include "xml.h"      /* wubuxml writer */

#include <stdlib.h>
#include <string.h>

/* ---------- tree ---------- */
struct SvgNode {
    char   *name;
    char  **akey;
    char  **aval;
    size_t  na, acap;
    SvgNode **kids;
    size_t  nk, kcap;
    char   *text;      /* accumulated direct text (may be NULL) */
    size_t  tlen, tcap;
    SvgNode *parent;
};

struct SvgDoc {
    SvgNode *root;
};

static void *xmalloc(size_t n) { void *p = malloc(n ? n : 1); if (!p) abort(); return p; }
static void *xrealloc(void *p, size_t n) { void *r = realloc(p, n ? n : 1); if (!r) abort(); return r; }
static char *xstrdup(const char *s) { size_t n = strlen(s) + 1; char *p = xmalloc(n); memcpy(p, s, n); return p; }

static SvgNode *node_new(const char *name, SvgNode *parent) {
    SvgNode *n = xmalloc(sizeof *n);
    memset(n, 0, sizeof *n);
    n->name = xstrdup(name);
    n->parent = parent;
    return n;
}

static void node_add_attr(SvgNode *n, const char *k, const char *v) {
    if (n->na == n->acap) {
        n->acap = n->acap ? n->acap * 2 : 4;
        n->akey = xrealloc(n->akey, n->acap * sizeof *n->akey);
        n->aval = xrealloc(n->aval, n->acap * sizeof *n->aval);
    }
    n->akey[n->na] = xstrdup(k);
    n->aval[n->na] = xstrdup(v ? v : "");
    n->na++;
}

static void node_add_kid(SvgNode *n, SvgNode *kid) {
    if (n->nk == n->kcap) {
        n->kcap = n->kcap ? n->kcap * 2 : 4;
        n->kids = xrealloc(n->kids, n->kcap * sizeof *n->kids);
    }
    n->kids[n->nk++] = kid;
}

static void node_add_text(SvgNode *n, const char *t, size_t len) {
    if (n->tlen + len + 1 > n->tcap) {
        n->tcap = (n->tlen + len + 1) * 2;
        n->text = xrealloc(n->text, n->tcap);
    }
    memcpy(n->text + n->tlen, t, len);
    n->tlen += len;
    n->text[n->tlen] = '\0';
}

static void node_free(SvgNode *n) {
    if (!n) return;
    for (size_t i = 0; i < n->na; i++) { free(n->akey[i]); free(n->aval[i]); }
    free(n->akey); free(n->aval);
    for (size_t i = 0; i < n->nk; i++) node_free(n->kids[i]);
    free(n->kids);
    free(n->name); free(n->text);
    free(n);
}

/* ---------- SAX build ---------- */
typedef struct { SvgDoc *doc; SvgNode *cur; int error; } Build;

static int on_evt(wubuxml_event evt, const wubuxml_info *info, void *user) {
    Build *b = user;
    switch (evt) {
    case WUBUXML_EVT_START: {
        SvgNode *n = node_new(info->name, b->cur);
        for (int i = 0; i < info->attr_count; i++)
            node_add_attr(n, info->attr_name[i], info->attr_val[i]);
        if (b->cur) node_add_kid(b->cur, n);
        else if (!b->doc->root) b->doc->root = n;
        else { node_free(n); b->error = 1; return 1; }  /* two roots */
        b->cur = n;
        return 0;
    }
    case WUBUXML_EVT_END:
        if (b->cur) b->cur = b->cur->parent;
        return 0;
    case WUBUXML_EVT_TEXT:
        if (b->cur && info->text && info->text_len)
            node_add_text(b->cur, info->text, info->text_len);
        return 0;
    }
    return 0;
}

SvgDoc *svg_parse(const char *data, size_t len) {
    if (!data) return NULL;
    SvgDoc *doc = xmalloc(sizeof *doc);
    doc->root = NULL;
    Build b = { doc, NULL, 0 };
    int rc = wubuxml_parse((const uint8_t *)data, len, on_evt, &b);
    if (rc != 0 || b.error || !doc->root) { svg_free(doc); return NULL; }
    return doc;
}

void svg_free(SvgDoc *doc) {
    if (!doc) return;
    node_free(doc->root);
    free(doc);
}

/* ---------- accessors ---------- */
SvgNode *svg_root(const SvgDoc *doc) { return doc ? doc->root : NULL; }
const char *svg_node_name(const SvgNode *n) { return n ? n->name : NULL; }
size_t svg_child_count(const SvgNode *n) { return n ? n->nk : 0; }
SvgNode *svg_child(const SvgNode *n, size_t i) { return (n && i < n->nk) ? n->kids[i] : NULL; }
const char *svg_node_text(const SvgNode *n) { return (n && n->text) ? n->text : ""; }
size_t svg_attr_count(const SvgNode *n) { return n ? n->na : 0; }
const char *svg_attr_key(const SvgNode *n, size_t i) { return (n && i < n->na) ? n->akey[i] : NULL; }
const char *svg_attr_val(const SvgNode *n, size_t i) { return (n && i < n->na) ? n->aval[i] : NULL; }

const char *svg_attr(const SvgNode *n, const char *key) {
    if (!n || !key) return NULL;
    for (size_t i = 0; i < n->na; i++)
        if (strcmp(n->akey[i], key) == 0) return n->aval[i];
    return NULL;
}

size_t svg_count_tag(const SvgNode *n, const char *tag) {
    if (!n || !tag) return 0;
    size_t c = (strcmp(n->name, tag) == 0) ? 1 : 0;
    for (size_t i = 0; i < n->nk; i++) c += svg_count_tag(n->kids[i], tag);
    return c;
}

/* ---------- regurgitate ---------- */
static void emit_node(wubuxml_writer *w, const SvgNode *n) {
    wubuxml_open(w, n->name);
    for (size_t i = 0; i < n->na; i++)
        wubuxml_set_attr(w, n->akey[i], n->aval[i]);
    if (n->text && n->tlen) wubuxml_text(w, n->text);
    for (size_t i = 0; i < n->nk; i++) emit_node(w, n->kids[i]);
    wubuxml_close(w);
}

char *svg_regurgitate(const SvgDoc *doc) {
    if (!doc || !doc->root) return NULL;
    char *buf = NULL; size_t bsz = 0;
    FILE *ms = open_memstream(&buf, &bsz);
    if (!ms) return NULL;
    wubuxml_writer *w = wubuxml_create(ms);
    wubuxml_declaration(w);
    emit_node(w, doc->root);
    wubuxml_destroy(w);
    fclose(ms);
    return buf;  /* malloc'd by open_memstream; caller frees */
}
