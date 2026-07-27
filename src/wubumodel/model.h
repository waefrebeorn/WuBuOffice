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
    WUBUMODEL_LINK,
    WUBUMODEL_FOOTNOTE,   /* inline ref marker + collected note text */
    WUBUMODEL_ENDNOTE
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

/* ---- footnotes / endnotes (DOC-55) ----
 * A footnote node carries a reference marker (its RUN text, e.g. "1") and a
 * note body (set via wubumodel_node_set_note). The layout emits an inline
 * marker; the collected notes are available via wubumodel_doc_notes(). */
int  wubumodel_node_set_note(wubumodel_node *n, const char *body); /* 0 ok */
const char *wubumodel_node_note(const wubumodel_node *n);

/* Collect all footnote/endnote bodies in document order (caller frees the
 * returned array of pointers; each string is owned by its node). Returns count. */
int  wubumodel_doc_notes(const wubumodel_doc *doc, const char ***out);

/* ---- hyperlink target (DOC-60) ----
 * A LINK node's RUN children are the visible text; its target is set via
 * wubumodel_node_set_link and read via wubumodel_node_link. */
int  wubumodel_node_set_link(wubumodel_node *n, const char *target); /* 0 ok */
const char *wubumodel_node_link(const wubumodel_node *n);
wubumodel_node *wubumodel_node_parent(const wubumodel_node *n); /* RUN->owning block */
typedef void (*wubumodel_change_cb)(wubumodel_doc *doc, wubumodel_id node,
                                    void *user);
int wubumodel_on_change(wubumodel_doc *doc, wubumodel_change_cb cb, void *user);
void wubumodel_emit_change(wubumodel_doc *doc, wubumodel_id node);

/* ---- tree containment (parent/child) ---- */
/* Append child to parent's child list (parent owns the link). Both must belong
 * to the same doc. Returns 0 ok, -1 on error. */
int wubumodel_node_append(wubumodel_doc *doc, wubumodel_node *parent,
                          wubumodel_node *child);
wubumodel_node *wubumodel_node_first_child(const wubumodel_node *parent);
wubumodel_node *wubumodel_node_next_sibling(const wubumodel_node *node);

/* Return the shared style attached to `n` (may be NULL). */
wubumodel_style *wubumodel_node_style(const wubumodel_node *n);

/* First top-level node of `doc` (a SECTION/BLOCK/etc.), or NULL if empty.
 * Walk the tree from here with node_first_child / node_next_sibling. */
wubumodel_node *wubumodel_doc_root(const wubumodel_doc *doc);

/* ---- OOXML I/O (minimal, from-scratch) ---- */
/* Serialize the model to a .docx package (WordprocessingML). `path` is the
 * output file. Returns 0 ok, -1 on error. Emits one SECTION as one logical
 * section; BLOCK/PARAGRAPH/RUN map to w:p/w:r/w:t. */
int wubumodel_write_docx(const wubumodel_doc *doc, const char *path);

/* Load a .docx package into the model (v1: reconstructs text into a
 * SECTION->PARAGRAPH->RUN tree). Returns 0 ok (ownership of *out passes
 * to the caller) or -1 on error (*out is NULL). No external deps:
 * from-scratch ZIP read + our own DEFLATE (ws07#1338). */
int wubumodel_load_docx(const char *path, wubumodel_doc **out);

#ifdef __cplusplus
}
#endif
#endif /* WUBUMODEL_MODEL_H */
