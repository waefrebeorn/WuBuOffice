/* view_doc_internal.h -- shared layout of the document view's private state.
 * Internal to apps/wubuos doc modules. */
#ifndef VIEW_DOC_INTERNAL_H
#define VIEW_DOC_INTERNAL_H

#include "wuos.h"
#include "wuburender.h"
#include "model.h"
#include "a11y.h"
#include "toc.h"
#include "settings.h"

#define DOC_MAX_OBJS 16

typedef struct { Wurender *r; wubumodel_doc *doc; char *path;
                 char *text;          /* recognized/raw text (for find) */
                 char *find_q; int find_hit;
                 /* hop 22: match navigation + icase */
                 int find_ix;      /* current match index (0-based) */
                 int find_total;   /* total matches for the query */
                 int find_icase;   /* case-insensitive mode */
                 /* inserted objects (chart/draw/math) rasterized to RGBA (INT-1,3) */
                 unsigned char *obj[ DOC_MAX_OBJS ]; int objw[ DOC_MAX_OBJS ]; int objh[ DOC_MAX_OBJS ];
                 int nobj;
                 char *epub_msg;      /* last EPUB export result */
                 a11y_report a11y;    /* last a11y check */
                 int a11y_done;
                 int toc_dirty;       /* TOC needs rebuild */
                 Toc *toc;            /* DOC-54 side pane */
                 int jump_page;        /* pending TOC jump (set by on_key) */
                 /* DOC-58: current paragraph index for style application */
                 int cur_para;
                 /* DOC-60: recorded link boxes (for click hit-testing) */
                 struct { int x, y, w, h; const char *target; } linkbox[32];
                 int nlink;
} DocV;

/* Shared across doc modules. */
int  doc_chrome_fs(DocV *e, int base);
wubumodel_node *doc_nth_paragraph(DocV *e, int idx);
void doc_apply_named_style(DocV *e, const char *name);
void doc_toggle_run_prop(DocV *e, const char *prop);   /* N3: bold/italic */
void doc_move_para(DocV *e, int dx);

#endif /* VIEW_DOC_INTERNAL_H */
