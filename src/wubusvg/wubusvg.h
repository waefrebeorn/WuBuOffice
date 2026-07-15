/* wubusvg.h -- clean-room SVG document model: ingest + regurgitate.
 *
 * SVG is XML (W3C SVG 1.1). This module ingests an SVG byte stream into a
 * structured element tree (reusing the dependency-free wubuxml SAX parser) and
 * regurgitates it back to well-formed SVG (reusing the wubuxml writer). It is
 * the document ingestion/regurgitation contract for WuBuOS: bytes in ->
 * structured tree the agent can inspect/edit -> bytes out.
 *
 * Not a renderer and not a full SVG DOM: it preserves element nesting,
 * attributes (in source order), and text content, which is what an ingestion
 * engine needs to round-trip and inspect a document.
 */
#ifndef WUBUSVG_H
#define WUBUSVG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SvgDoc  SvgDoc;
typedef struct SvgNode SvgNode;

/* Ingest an SVG (XML) byte stream into a document tree. Returns NULL on a
 * structural parse error or if there is no root element. Caller frees with
 * svg_free(). The input buffer need NOT outlive the doc (strings are copied). */
SvgDoc *svg_parse(const char *data, size_t len);
void    svg_free(SvgDoc *doc);

/* Root element (e.g. the <svg> node), or NULL for an empty doc. */
SvgNode *svg_root(const SvgDoc *doc);

/* Element tag name (namespaced name kept verbatim, e.g. "svg", "font-face"). */
const char *svg_node_name(const SvgNode *n);

/* Number of direct child elements. */
size_t svg_child_count(const SvgNode *n);
/* i-th direct child element (0-based), or NULL if out of range. */
SvgNode *svg_child(const SvgNode *n, size_t i);

/* Concatenated text content directly under this node (may be ""). */
const char *svg_node_text(const SvgNode *n);

/* Number of attributes on this node, and the i-th key/value. */
size_t svg_attr_count(const SvgNode *n);
const char *svg_attr_key(const SvgNode *n, size_t i);
const char *svg_attr_val(const SvgNode *n, size_t i);
/* Value for a given attribute key, or NULL if absent. */
const char *svg_attr(const SvgNode *n, const char *key);

/* Count elements with the given tag name anywhere in the subtree rooted at n
 * (inclusive). Useful to inspect an ingested font-SVG: e.g. how many <glyph>. */
size_t svg_count_tag(const SvgNode *n, const char *tag);

/* Regurgitate the document as well-formed SVG. Returns a malloc'd NUL-terminated
 * string (caller frees), or NULL on failure. Emits an XML declaration. */
char *svg_regurgitate(const SvgDoc *doc);

/* ---------- editing (the "creation" half of the engine) ----------
 * All mutators respect the opaque tree; the AGI edits via these, never by
 * reaching into node internals. */

/* Set (create or overwrite) an attribute on a node. Returns 0 on success. */
int svg_set_attr(SvgNode *n, const char *key, const char *val);
/* Remove an attribute by key. Returns 1 if removed, 0 if it was absent. */
int svg_remove_attr(SvgNode *n, const char *key);

/* Create a new detached element node with the given tag name. It is owned by
 * the caller until attached with svg_append_child/svg_insert_child; if never
 * attached it must be released with svg_free_node(). */
SvgNode *svg_new_node(const char *name);
/* Free a DETACHED node (and its subtree). Do NOT call on an attached node. */
void     svg_free_node(SvgNode *n);

/* Append `kid` as the last child of `parent`. Ownership transfers to the tree.
 * Returns 0 on success. */
int svg_append_child(SvgNode *parent, SvgNode *kid);
/* Insert `kid` at index `i` (clamped to [0, child_count]). Ownership transfers.
 * Returns 0 on success. */
int svg_insert_child(SvgNode *parent, size_t i, SvgNode *kid);
/* Detach and free the i-th child of `parent`. Returns 1 if removed. */
int svg_remove_child(SvgNode *parent, size_t i);

/* Replace a node's direct text content (entity-escaped on regurgitate). */
int svg_set_text(SvgNode *n, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* WUBUSVG_H */
