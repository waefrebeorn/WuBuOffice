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
int wubumodel_node_set_style(wubumodel_node *n, wubumodel_style *s) {
    if (!n) return -1;
    if (n->style) wubumodel_style_destroy(n->style);
    n->style = s;
    if (s) s->refcount++;
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
