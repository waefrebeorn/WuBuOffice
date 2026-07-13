#ifndef WUBUMODEL_MODEL_H
#define WUBUMODEL_MODEL_H

/* Public, opaque API for the unified object model (WS11).
 * App and extension code must NOT see the struct layouts — include only this
 * header. See model_internal.h for definitions (internal use only). */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Node kinds in the taxonomy. Values stable across versions. */
typedef enum {
    WUBUMODEL_DOC = 0,
    WUBUMODEL_SECTION,
    WUBUMODEL_BLOCK,
    WUBUMODEL_PARAGRAPH,
    WUBUMODEL_RUN,
    WUBUMODEL_CELL,
    WUBUMODEL_SHAPE,
    WUBUMODEL_CHART,
    WUBUMODEL_TABLE,
    WUBUMODEL_FIELD,
    WUBUMODEL_LINK
} wubumodel_kind;

typedef uint64_t wubumodel_id;

typedef struct wubumodel_doc wubumodel_doc;
typedef struct wubumodel_node wubumodel_node;
typedef struct wubumodel_style wubumodel_style;

/* ---- document lifecycle ---- */
wubumodel_doc *wubumodel_doc_create(void);
void wubumodel_doc_destroy(wubumodel_doc *doc);

/* ---- node CRUD (returns NULL on OOM) ---- */
wubumodel_node *wubumodel_node_create(wubumodel_doc *doc, wubumodel_kind kind);
wubumodel_id    wubumodel_node_id(const wubumodel_node *n);
wubumodel_kind  wubumodel_node_kind(const wubumodel_node *n);
void            wubumodel_node_destroy(wubumodel_doc *doc, wubumodel_node *n);
wubumodel_node *wubumodel_node_find(wubumodel_doc *doc, wubumodel_id id);

/* ---- text (RUN) ---- */
int  wubumodel_run_set_text(wubumodel_node *run, const char *utf8); /* 0 ok, -1 err */
const char *wubumodel_run_text(const wubumodel_node *run);

/* ---- style (shared, copy-on-write) ---- */
wubumodel_style *wubumodel_style_create(void);
void wubumodel_style_destroy(wubumodel_style *s);
int  wubumodel_style_set_prop(wubumodel_style *s, const char *name,
                              const char *value); /* 0 ok */
const char *wubumodel_style_get_prop(const wubumodel_style *s, const char *name);
/* attach (shares pointer; COW performed by setter on internal mutation) */
int wubumodel_node_set_style(wubumodel_node *n, wubumodel_style *s);

/* ---- command / undo ---- */
typedef enum {
    WUBUMODEL_CMD_SET_TEXT = 0
} wubumodel_cmd_kind;
/* apply a set-text command, recording the inverse for undo; returns 0 ok */
int wubumodel_cmd_set_text(wubumodel_doc *doc, wubumodel_node *run,
                           const char *new_text);
int wubumodel_doc_undo(wubumodel_doc *doc);   /* returns 0 ok, -1 nothing-to-undo */

/* ---- observers (reactive) ---- */
typedef void (*wubumodel_change_cb)(wubumodel_doc *doc, wubumodel_id node,
                                    void *user);
int wubumodel_on_change(wubumodel_doc *doc, wubumodel_change_cb cb, void *user);
void wubumodel_emit_change(wubumodel_doc *doc, wubumodel_id node);

#ifdef __cplusplus
}
#endif
#endif /* WUBUMODEL_MODEL_H */
