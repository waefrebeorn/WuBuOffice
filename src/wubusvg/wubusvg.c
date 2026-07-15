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

/* ---------- editing ---------- */
int svg_set_attr(SvgNode *n, const char *key, const char *val) {
    if (!n || !key) return -1;
    for (size_t i = 0; i < n->na; i++) {
        if (strcmp(n->akey[i], key) == 0) {
            char *nv = xstrdup(val ? val : "");
            free(n->aval[i]);
            n->aval[i] = nv;
            return 0;
        }
    }
    node_add_attr(n, key, val);
    return 0;
}

int svg_remove_attr(SvgNode *n, const char *key) {
    if (!n || !key) return 0;
    for (size_t i = 0; i < n->na; i++) {
        if (strcmp(n->akey[i], key) == 0) {
            free(n->akey[i]); free(n->aval[i]);
            for (size_t j = i + 1; j < n->na; j++) {
                n->akey[j-1] = n->akey[j];
                n->aval[j-1] = n->aval[j];
            }
            n->na--;
            return 1;
        }
    }
    return 0;
}

SvgNode *svg_new_node(const char *name) {
    if (!name) return NULL;
    return node_new(name, NULL);
}

void svg_free_node(SvgNode *n) { node_free(n); }

int svg_append_child(SvgNode *parent, SvgNode *kid) {
    if (!parent || !kid) return -1;
    kid->parent = parent;
    node_add_kid(parent, kid);
    return 0;
}

int svg_insert_child(SvgNode *parent, size_t i, SvgNode *kid) {
    if (!parent || !kid) return -1;
    if (i > parent->nk) i = parent->nk;
    /* grow, then shift right to open a slot at i */
    if (parent->nk == parent->kcap) {
        parent->kcap = parent->kcap ? parent->kcap * 2 : 4;
        parent->kids = xrealloc(parent->kids, parent->kcap * sizeof *parent->kids);
    }
    for (size_t j = parent->nk; j > i; j--) parent->kids[j] = parent->kids[j-1];
    parent->kids[i] = kid;
    parent->nk++;
    kid->parent = parent;
    return 0;
}

int svg_remove_child(SvgNode *parent, size_t i) {
    if (!parent || i >= parent->nk) return 0;
    node_free(parent->kids[i]);
    for (size_t j = i + 1; j < parent->nk; j++) parent->kids[j-1] = parent->kids[j];
    parent->nk--;
    return 1;
}

int svg_set_text(SvgNode *n, const char *text) {
    if (!n) return -1;
    free(n->text);
    n->text = NULL;
    n->tlen = n->tcap = 0;
    if (text && *text) node_add_text(n, text, strlen(text));
    return 0;
}

/* ---------- query + edit-by-query ---------- */
/* Depth-first descendant-chain match: the first node whose ancestor chain
 * matches segs[0]<-segs[1]<-...<-segs[nseg-1] (segs[0] nearest the root). The
 * root's own tag is irrelevant (so "g/rect" and "svg/g/rect" both work). */
static SvgNode *walk_path(const SvgNode *cur, char **segs, size_t nseg) {
    if (!cur || nseg == 0) return NULL;
    if (strcmp(cur->name, segs[0]) == 0) {
        if (nseg == 1) return (SvgNode *)cur;
        for (size_t i = 0; i < cur->nk; i++) {
            SvgNode *r = walk_path(cur->kids[i], segs + 1, nseg - 1);
            if (r) return r;
        }
        return NULL;
    }
    for (size_t i = 0; i < cur->nk; i++) {
        SvgNode *r = walk_path(cur->kids[i], segs, nseg);
        if (r) return r;
    }
    return NULL;
}

static size_t walk_path_all(const SvgNode *cur, char **segs, size_t nseg,
                            SvgNode **out, size_t maxout, size_t got) {
    if (!cur || nseg == 0) return got;
    if (strcmp(cur->name, segs[0]) == 0) {
        if (nseg == 1) {
            if (got < maxout) out[got] = (SvgNode *)cur;
            return got + 1;
        }
        for (size_t i = 0; i < cur->nk; i++)
            got = walk_path_all(cur->kids[i], segs + 1, nseg - 1, out, maxout, got);
        return got;
    }
    for (size_t i = 0; i < cur->nk; i++)
        got = walk_path_all(cur->kids[i], segs, nseg, out, maxout, got);
    return got;
}

/* Split `path` into up to 8 tag segments stored in a static arena; ignores a
 * leading segment equal to the root tag (so "svg/g/rect" == "g/rect"). Returns
 * the segment count and fills `segs` with NUL-terminated pointers. */
static size_t split_path(const char *root_name, const char *path, char *segs[8]) {
    static char arena[8][32];
    size_t cnt = 0;
    char buf[256];
    size_t l = strlen(path);
    if (l >= sizeof buf) l = sizeof buf - 1;
    memcpy(buf, path, l); buf[l] = '\0';
    char *save = NULL;
    for (char *tok = strtok_r(buf, "/", &save); tok && cnt < 8; tok = strtok_r(NULL, "/", &save)) {
        if (cnt == 0 && strcmp(tok, root_name) == 0) continue;  /* skip root echo */
        strncpy(arena[cnt], tok, 31); arena[cnt][31] = '\0';
        segs[cnt] = arena[cnt];
        cnt++;
    }
    return cnt;
}

SvgNode *svg_find(const SvgNode *root, const char *path) {
    if (!root || !path) return NULL;
    char *segs[8];
    size_t nseg = split_path(root->name, path, segs);
    if (nseg == 0) return NULL;
    return walk_path(root, segs, nseg);
}

size_t svg_find_all(const SvgNode *root, const char *path, SvgNode **out, size_t maxout) {
    if (!root || !path || !out) return 0;
    char *segs[8];
    size_t nseg = split_path(root->name, path, segs);
    if (nseg == 0) return 0;
    return walk_path_all(root, segs, nseg, out, maxout, 0);
}

int svg_set_attr_path(SvgNode *root, const char *path, const char *key, const char *val) {
    SvgNode *n = svg_find(root, path);
    if (!n) return -1;
    return svg_set_attr(n, key, val);
}

int svg_remove_path(SvgNode *root, const char *path) {
    if (!root || !path) return -1;
    char *segs[8];
    size_t nseg = split_path(root->name, path, segs);
    if (nseg == 0) return -1;

    /* Locate the target node and its parent by descending the chain; segs[0]
     * matches anywhere under root, each subsequent segment matches among that
     * node's children. */
    SvgNode *parent = NULL;
    size_t idx = 0;
    SvgNode *node = walk_path(root, segs, nseg);   /* first matching target */
    if (!node) return -1;
    /* find node's parent + index */
    if (node != root) {
        /* BFS-free: scan from root for the parent that has `node` as a child */
        int found = 0;
        /* small helper via recursion-less scan using a stack-like array */
        SvgNode *stack[64]; size_t sp = 0;
        stack[sp++] = (SvgNode *)root;
        while (sp) {
            SvgNode *p = stack[--sp];
            for (size_t i = 0; i < p->nk; i++) {
                if (p->kids[i] == node) { parent = p; idx = i; found = 1; break; }
                if (sp < 64) stack[sp++] = p->kids[i];
            }
            if (found) break;
        }
    }
    if (!parent) return -1;  /* cannot remove the root */
    int rc = svg_remove_child(parent, idx);
    return rc ? 1 : -1;
}
