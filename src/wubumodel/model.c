/* WS11 unified object model — core implementation (green baseline).
 * v1 scope: doc lifecycle, node CRUD with stable ids, RUN text, shared style
 * (COW-on-attach is deferred; we share pointer + refcount), set-text command
 * with undo, and observers. OOXML I/O, typed payloads, and the CRDT seam are
 * layered in later PRs. */
#define _POSIX_C_SOURCE 200809L
#include "model_internal.h"
#include <stdlib.h>
#include <string.h>

static wubumodel_node *node_lookup(wubumodel_doc *doc, wubumodel_id id) {
    if (!doc) return NULL;
    size_t b = id % WUBUMODEL_BUCKETS;
    for (wubumodel_node *n = doc->nodes[b]; n; n = n->next)
        if (n->id == id) return n;
    return NULL;
}

wubumodel_doc *wubumodel_doc_create(void) {
    wubumodel_doc *d = calloc(1, sizeof(*d));
    if (d) d->next_id = 1;
    return d;
}

void wubumodel_doc_destroy(wubumodel_doc *doc) {
    if (!doc) return;
    for (size_t b = 0; b < WUBUMODEL_BUCKETS; b++) {
        wubumodel_node *n = doc->nodes[b];
        while (n) {
            wubumodel_node *nx = n->next;
            if (n->style) wubumodel_style_destroy(n->style);
            free(n->text);
            free(n->note);
            free(n->link);
            free(n->img);
            free(n->author);
            free(n->field);
            free(n);
            n = nx;
        }
    }
    while (doc->undo_top) {
        cmd_inv *c = doc->undo_top; doc->undo_top = c->prev;
        free(c->before); free(c);
    }
    while (doc->observers) {
        obs *o = doc->observers; doc->observers = o->next;
        free(o);
    }
    free(doc);
}

wubumodel_node *wubumodel_node_create(wubumodel_doc *doc, wubumodel_kind kind) {
    if (!doc) return NULL;
    wubumodel_node *n = calloc(1, sizeof(*n));
    if (!n) return NULL;
    n->id = doc->next_id++;
    n->kind = kind;
    size_t b = n->id % WUBUMODEL_BUCKETS;
    n->next = doc->nodes[b];
    doc->nodes[b] = n;
    return n;
}

wubumodel_id wubumodel_node_id(const wubumodel_node *n) {
    return n ? n->id : 0;
}
wubumodel_kind wubumodel_node_kind(const wubumodel_node *n) {
    return n ? n->kind : WUBUMODEL_DOC;
}
void wubumodel_node_destroy(wubumodel_doc *doc, wubumodel_node *n) {
    if (!doc || !n) return;
    size_t b = n->id % WUBUMODEL_BUCKETS;
    wubumodel_node **pp = &doc->nodes[b];
    while (*pp) {
        if (*pp == n) { *pp = n->next; break; }
        pp = &(*pp)->next;
    }
    if (n->style) wubumodel_style_destroy(n->style);
    free(n->text);
    free(n->note);
    free(n->link);
    free(n->img);
    free(n->author);
    free(n->field);
    free(n);
}
wubumodel_node *wubumodel_node_find(wubumodel_doc *doc, wubumodel_id id) {
    return node_lookup(doc, id);
}

static int set_text_raw(wubumodel_node *run, const char *utf8) {
    char *cp = utf8 ? strdup(utf8) : NULL;
    if (utf8 && !cp) return -1;
    free(run->text);
    run->text = cp;
    return 0;
}

int wubumodel_run_set_text(wubumodel_node *run, const char *utf8) {
    if (!run) return -1;
    return set_text_raw(run, utf8);
}
const char *wubumodel_run_text(const wubumodel_node *run) {
    return run ? run->text : NULL;
}
int wubumodel_node_set_text(wubumodel_node *n, const char *utf8) {
    if (!n) return -1;
    return set_text_raw(n, utf8);
}
const char *wubumodel_node_text(const wubumodel_node *n) {
    return n ? n->text : NULL;
}

/* ---- footnotes / endnotes (DOC-55) ---- */
int wubumodel_node_set_note(wubumodel_node *n, const char *body) {
    if (!n) return -1;
    char *cp = body ? strdup(body) : NULL;
    if (body && !cp) return -1;
    free(n->note); n->note = cp;
    return 0;
}
const char *wubumodel_node_note(const wubumodel_node *n) {
    return n ? n->note : NULL;
}

/* iterate every footnote/endnote body in document order */
int wubumodel_doc_notes(const wubumodel_doc *doc, const char ***out) {
    if (!doc || !out) return -1;
    /* worst-case sizing: every node could be a note */
    const char **arr = calloc(1, sizeof(*arr) * 2048);
    if (!arr) return -1;
    int n = 0;
    for (size_t b = 0; b < WUBUMODEL_BUCKETS; b++)
        for (wubumodel_node *p = doc->nodes[b]; p; p = p->next)
            if ((p->kind == WUBUMODEL_FOOTNOTE ||
                  p->kind == WUBUMODEL_ENDNOTE) && p->note)
                if (n < 2048) arr[n++] = p->note;
    *out = arr;
    return n;
}

/* ---- hyperlink target (DOC-60) ----
 * A LINK node carries an inline text (its RUN children) and a target URL set
 * via wubumodel_node_set_link(). The layout reserves an object box and the
 * views render it blue + underlined and make it clickable. */
int wubumodel_node_set_link(wubumodel_node *n, const char *target) {
    if (!n) return -1;
    char *cp = target ? strdup(target) : NULL;
    if (target && !cp) return -1;
    free(n->link); n->link = cp;
    return 0;
}
const char *wubumodel_node_link(const wubumodel_node *n) {
    return n ? n->link : NULL;
}
wubumodel_node *wubumodel_node_parent(const wubumodel_node *n) {
    return n ? n->parent : NULL;
}

/* ---- embedded image (DOC-61) ----
 * Stores a copy of an RGBA plane (w*h*4 bytes) on the node. The layout treats
 * an IMAGE node as an object box; the view blits the raster into it. */
int wubumodel_node_set_image(wubumodel_node *n, const uint8_t *rgba,
                             int w, int h){
    if (!n || w<=0 || h<=0 || !rgba) return -1;
    uint8_t *cp = malloc((size_t)w*h*4);
    if (!cp) return -1;
    memcpy(cp, rgba, (size_t)w*h*4);
    free(n->img);
    n->img = cp; n->img_w = w; n->img_h = h;
    return 0;
}
const uint8_t *wubumodel_node_image(const wubumodel_node *n, int *w, int *h){
    if (!n || !n->img) return NULL;
    if (w) *w = n->img_w;
    if (h) *h = n->img_h;
    return n->img;
}

/* ---- review/field/break metadata (DOC-56..65) ---- */
int  wubumodel_node_set_author(wubumodel_node *n, const char *a){
    if (!n) return -1;
    char *c = a?strdup(a):NULL;
    free(n->author);
    n->author=c;
    return 0;
}
const char *wubumodel_node_author(const wubumodel_node *n){ return n?n->author:NULL; }
int  wubumodel_node_set_field(wubumodel_node *n, const char *f){
    if (!n) return -1;
    char *c = f?strdup(f):NULL;
    free(n->field);
    n->field=c;
    return 0;
}
const char *wubumodel_node_field(const wubumodel_node *n){ return n?n->field:NULL; }

int  wubumodel_node_set_foreign(wubumodel_node *n, const char *name,
                                const char *raw){
    if (!n) return -1;
    n->kind = WUBUMODEL_FOREIGN;
    char *c = name ? strdup(name) : NULL;
    free(n->field); n->field = c;
    char *r = raw ? strdup(raw) : NULL;
    free(n->text);  n->text = r;      /* raw XML rides the node text slot */
    return 0;
}
const char *wubumodel_node_foreign_name(const wubumodel_node *n){
    return (n && n->kind == WUBUMODEL_FOREIGN) ? n->field : NULL;
}
const char *wubumodel_node_foreign_raw(const wubumodel_node *n){
    return (n && n->kind == WUBUMODEL_FOREIGN) ? n->text : NULL;
}

int  wubumodel_node_set_float(wubumodel_node *n, int side, int wrap,
                              int w, int h){
    if (!n) return -1;
    n->float_side = side; n->float_wrap = wrap;
    if (w > 0) n->img_w = w;
    if (h > 0) n->img_h = h;
    return 0;
}
int wubumodel_node_float_side(const wubumodel_node *n){ return n?n->float_side:0; }
int wubumodel_node_float_wrap(const wubumodel_node *n){ return n?n->float_wrap:0; }
int wubumodel_node_float_w(const wubumodel_node *n){ return n?n->img_w:0; }
int wubumodel_node_float_h(const wubumodel_node *n){ return n?n->img_h:0; }

int wubumodel_node_set_span(wubumodel_node *n, int col_span, int vmerge){
    if (!n) return -1;
    n->col_span = col_span > 0 ? col_span : 1;
    n->vmerge = vmerge;
    return 0;
}
int wubumodel_node_col_span(const wubumodel_node *n){ return (n && n->col_span > 0) ? n->col_span : 1; }
int wubumodel_node_vmerge(const wubumodel_node *n){ return n?n->vmerge:0; }
int  wubumodel_node_set_tc(wubumodel_node *n, int t){ if(!n)return -1; n->tc=t; return 0; }
int  wubumodel_node_tc(const wubumodel_node *n){ return n?(n->tc):0; }
int  wubumodel_node_set_break(wubumodel_node *n, int b){ if(!n)return -1; n->brk=b; return 0; }
int  wubumodel_node_break(const wubumodel_node *n){ return n?(n->brk):0; }

wubumodel_style *wubumodel_style_create(void) {
    wubumodel_style *s = calloc(1, sizeof(*s));
    if (s) s->refcount = 1;
    return s;
}
void wubumodel_style_destroy(wubumodel_style *s) {
    if (!s) return;
    if (--s->refcount > 0) return;
    style_prop *p = s->props;
    while (p) { style_prop *nx = p->next; free(p->name); free(p->value); free(p); p = nx; }
    free(s);
}
int wubumodel_style_set_prop(wubumodel_style *s, const char *name, const char *value) {
    if (!s || !name) return -1;
    for (style_prop *p = s->props; p; p = p->next)
        if (strcmp(p->name, name) == 0) {
            char *v = value ? strdup(value) : NULL;
            if (value && !v) return -1;
            free(p->value); p->value = v;
            return 0;
        }
    style_prop *p = calloc(1, sizeof(*p));
    if (!p) return -1;
    p->name = strdup(name);
    p->value = value ? strdup(value) : NULL;
    if (!p->name) { free(p); return -1; }
    p->next = s->props;
    s->props = p;
    return 0;
}
const char *wubumodel_style_get_prop(const wubumodel_style *s, const char *name) {
    if (!s || !name) return NULL;
    for (style_prop *p = s->props; p; p = p->next)
        if (strcmp(p->name, name) == 0) return p->value;
    return NULL;
}
int wubumodel_style_prop_at(const wubumodel_style *s, int i,
                            const char **name, const char **value) {
    if (!s || i < 0) return 0;
    int k = 0;
    for (style_prop *p = s->props; p; p = p->next, k++)
        if (k == i) {
            if (name)  *name  = p->name;
            if (value) *value = p->value;
            return 1;
        }
    return 0;
}
int wubumodel_node_set_style(wubumodel_node *n, wubumodel_style *s) {
    if (!n) return -1;
    if (n->style) wubumodel_style_destroy(n->style);
    n->style = s;
    if (s) s->refcount++;
    return 0;
}
/* DOC-58: named style presets. Returns a freshly-allocated style the caller
 * attaches via wubumodel_node_set_style (it takes ownership of the refcount).
 * Recognized names: Heading1/2/3, Body, Quote, Code. Unknown -> Body. */
wubumodel_style *wubumodel_style_named(const char *name){
    wubumodel_style *s = wubumodel_style_create();
    if (!s) return NULL;
    if (!name || !*name) name = "Body";
    if (strcmp(name,"Heading1")==0 || strcmp(name,"H1")==0){
        wubumodel_style_set_prop(s,"heading","1");
        wubumodel_style_set_prop(s,"size","26"); wubumodel_style_set_prop(s,"bold","1");
    } else if (strcmp(name,"Heading2")==0 || strcmp(name,"H2")==0){
        wubumodel_style_set_prop(s,"heading","2");
        wubumodel_style_set_prop(s,"size","20"); wubumodel_style_set_prop(s,"bold","1");
    } else if (strcmp(name,"Heading3")==0 || strcmp(name,"H3")==0){
        wubumodel_style_set_prop(s,"heading","3");
        wubumodel_style_set_prop(s,"size","16"); wubumodel_style_set_prop(s,"bold","1");
    } else if (strcmp(name,"Quote")==0){
        wubumodel_style_set_prop(s,"italic","1"); wubumodel_style_set_prop(s,"size","13");
    } else if (strcmp(name,"Code")==0){
        wubumodel_style_set_prop(s,"size","12"); wubumodel_style_set_prop(s,"mono","1");
    } else { /* Body */
        wubumodel_style_set_prop(s,"size","12");
    }
    return s;
}
int wubumodel_node_apply_named_style(wubumodel_node *n, const char *name){
    if (!n) return -1;
    wubumodel_style *s = wubumodel_style_named(name);
    if (!s) return -1;
    wubumodel_node_set_style(n, s);   /* node takes a ref (refcount -> 2) */
    wubumodel_style_destroy(s);       /* drop our caller ref (refcount -> 1, node-owned) */
    return 0;
}

int wubumodel_cmd_set_text(wubumodel_doc *doc, wubumodel_node *run,
                           const char *new_text) {
    if (!doc || !run) return -1;
    cmd_inv *c = calloc(1, sizeof(*c));
    if (!c) return -1;
    c->node = run->id;
    c->kind = WUBUMODEL_CMD_SET_TEXT;
    c->before = run->text ? strdup(run->text) : NULL;
    if (set_text_raw(run, new_text) != 0) { free(c->before); free(c); return -1; }
    c->prev = doc->undo_top;
    doc->undo_top = c;
    wubumodel_emit_change(doc, run->id);
    return 0;
}

int wubumodel_doc_undo(wubumodel_doc *doc) {
    if (!doc || !doc->undo_top) return -1;
    cmd_inv *c = doc->undo_top; doc->undo_top = c->prev;
    wubumodel_node *n = node_lookup(doc, c->node);
    if (n) set_text_raw(n, c->before);
    free(c->before); free(c);
    return 0;
}

int wubumodel_on_change(wubumodel_doc *doc, wubumodel_change_cb cb, void *user) {
    if (!doc || !cb) return -1;
    obs *o = calloc(1, sizeof(*o));
    if (!o) return -1;
    o->cb = cb; o->user = user;
    o->next = doc->observers;
    doc->observers = o;
    return 0;
}
void wubumodel_emit_change(wubumodel_doc *doc, wubumodel_id node) {
    if (!doc) return;
    for (obs *o = doc->observers; o; o = o->next)
        o->cb(doc, node, o->user);
}

/* ---- tree containment ---- */
int wubumodel_node_append(wubumodel_doc *doc, wubumodel_node *parent,
                          wubumodel_node *child) {
    if (!doc || !parent || !child) return -1;
    /* child must already be registered in this doc (created via node_create) */
    if (!node_lookup(doc, child->id)) return -1;
    if (child->parent) return -1; /* already attached elsewhere */
    child->parent = parent;
    /* append at the TAIL so iteration (first_child -> next_sibling)
     * preserves creation / authoring order (not reversed). */
    if (parent->first_child == NULL) {
        parent->first_child = child;
    } else {
        wubumodel_node *last = parent->first_child;
        while (last->next_sibling) last = last->next_sibling;
        last->next_sibling = child;
    }
    child->next_sibling = NULL;
    return 0;
}
wubumodel_node *wubumodel_node_first_child(const wubumodel_node *parent) {
    return parent ? parent->first_child : NULL;
}
wubumodel_node *wubumodel_node_next_sibling(const wubumodel_node *node) {
    return node ? node->next_sibling : NULL;
}
wubumodel_style *wubumodel_node_style(const wubumodel_node *n) {
    return n ? n->style : NULL;
}

/* Return the first top-level node of the document (a SECTION/BLOCK/etc.),
 * or NULL if the doc is empty. Used by readers/serializers to walk the
 * tree starting from the root. O(1): scans the node buckets. */
wubumodel_node *wubumodel_doc_root(const wubumodel_doc *doc) {
    if (!doc) return NULL;
    for (size_t b = 0; b < WUBUMODEL_BUCKETS; b++)
        for (wubumodel_node *n = doc->nodes[b]; n; n = n->next)
            if (!n->parent) return n;
    return NULL;
}
