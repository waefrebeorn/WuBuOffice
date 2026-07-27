/* layout.h -- central text pipeline: model -> laid-out pages.
 *
 * This is the heaviest plumbing in the suite. Every downstream feature
 * (screen rendering, PDF/RTF/LaTeX/HTML export, a11y reading order,
 * virtualization, RTL, tables, headers/footers, line numbers) consumes the
 * output of THIS module instead of re-implementing text wrapping. That is the
 * whole point: one pagination engine, many thin consumers.
 *
 * The engine is font-agnostic: it asks for glyph/run metrics through a
 * metric callback you supply, so wubulayout stays in the engine layer and
 * never includes an app or renderer header. Opaque by design.
 *
 * C11, no third-party deps. */
#ifndef WUBULAYOUT_LAYOUT_H
#define WUBULAYOUT_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WUBULAYOUT_LTR = 0,
    WUBULAYOUT_RTL = 1
} wubulayout_dir;

/* A laid-out positioned run (one contiguous styled text segment on a line). */
typedef struct {
    const char *text;     /* borrowed from the model (UTF-8) */
    size_t      text_len; /* byte length of the segment on THIS line */
    int         x;        /* pen x (px) */
    int         y;        /* baseline y (px) */
    int         w;        /* advance width (px) */
    int         h;        /* line height (px) */
    int         page;     /* 0-based page index */
    int         line;     /* 0-based line index within the page */
    int         font_size;
    int         bold;
    int         italic;
    int         rtl;      /* 1 if this run was laid out right-to-left */
    wubulayout_dir dir;
    void *user;           /* the source model node/run (for hit-testing) */
} wubulayout_run;

/* A page: a rectangle; runs reference it by index. */
typedef struct {
    int w, h;
    int margin_l, margin_r, margin_t, margin_b;
} wubulayout_page_info;

/* Paragraph/line geometry for consumers that want line boxes. */
typedef struct {
    int x, y, w, h;
    int page, line;
} wubulayout_line;

typedef struct wubulayout_doc wubulayout_doc;

/* Metric callback: given a UTF-8 segment + font size + bold/italic, return
 * the advance width in px. Returns height (line height) via *out_h. The
 * engine calls this for each candidate line break. */
typedef int (*wubulayout_measure_fn)(const char *text, size_t len,
                                     int font_size, int bold, int italic,
                                     int *out_h, void *user);

/* Style query: for a model run/node the engine asks for font size/bold/italic
 * and base direction. Return 0 and fill fields; non-zero -> use defaults. */
typedef int (*wubulayout_style_fn)(void *user, void *run,
                                   int *font_size, int *bold, int *italic,
                                   wubulayout_dir *dir);

/* ---- construction ---- */
/* Build a layout from a model document. `measure` and `style` are required.
 * page_w/page_h are in px (e.g. 794x1123 for A4@96dpi). margins in px. */
wubulayout_doc *wubulayout_create(
    void *model_doc,                 /* opaque model doc pointer */
    void *model_root,                /* opaque root node (or NULL to use doc root) */
    wubulayout_measure_fn measure,
    wubulayout_style_fn   style,
    void *cb_user,
    int page_w, int page_h,
    int margin_l, int margin_r, int margin_t, int margin_b);

void wubulayout_destroy(wubulayout_doc *L);

/* Re-run pagination (after model edits). Cheap enough to call on every idle. */
int wubulayout_rebuild(wubulayout_doc *L);

/* Incremental re-lay (PRF-101): re-lay ONLY from the given block onward.
 * `block` is a model node that received a checkpoint during the last
 * (re)build — paragraphs, tables, and object nodes all qualify. Geometry of
 * everything before it is preserved and the measure callback is NOT called
 * again for those blocks. Unknown nodes fall back to a full rebuild. */
int wubulayout_invalidate(wubulayout_doc *L, void *block);

/* ---- queries ---- */
int   wubulayout_page_count(const wubulayout_doc *L);
const wubulayout_page_info *wubulayout_page(const wubulayout_doc *L, int page);

/* Runs on a page (array valid until next rebuild). */
int   wubulayout_run_count(const wubulayout_doc *L, int page);
const wubulayout_run *wubulayout_run_at(const wubulayout_doc *L, int page, int i);

/* Lines on a page (line boxes). */
int   wubulayout_line_count(const wubulayout_doc *L, int page);
const wubulayout_line *wubulayout_line_at(const wubulayout_doc *L, int page, int i);

/* Object boxes on a page (tables/cells/images/shapes). Returns geometry +
 * the source model node (for border/overlay drawing) without leaking the
 * internal layout struct. Array valid until next rebuild. */
typedef struct {
    int x, y, w, h;       /* box rect in page-content px */
    void *user;           /* source model node (e.g. a CELL row) */
} wubulayout_box;
int   wubulayout_box_count(const wubulayout_doc *L, int page);
const wubulayout_box *wubulayout_box_at(const wubulayout_doc *L, int page, int i);

/* Total laid-out run count across all pages. */
int   wubulayout_total_runs(const wubulayout_doc *L);

/* Hit-test: given page + (x,y) in page content coords, return run index on
 * that page or -1. Also fills *out_line. */
int   wubulayout_hit_test(const wubulayout_doc *L, int page, int x, int y,
                          int *out_line);

/* Reading-order text of a page (for a11y / export). Returns malloc'd string;
 * caller frees. Logical order per paragraph (LTR or RTL per run). */
char *wubulayout_page_text(const wubulayout_doc *L, int page);

/* Full document reading-order text (all pages concatenated). Caller frees. */
char *wubulayout_doc_text(const wubulayout_doc *L);

#ifdef __cplusplus
}
#endif
#endif /* WUBULAYOUT_LAYOUT_H */
