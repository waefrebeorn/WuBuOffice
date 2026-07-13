/* WuBuOffice -- apps/wubucell/cell_internal
 * Internal (non-public) definitions for the xlsx builder. These structs are
 * not part of the public API in cell.h; only cell_*.c translation units
 * include this header, keeping the public surface minimal and the model
 * opaque to callers.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#ifndef WUBUCELL_CELL_INTERNAL_H
#define WUBUCELL_CELL_INTERNAL_H

#include "cell.h"
#include "style.h"
#include "value.h"
#include "eval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

typedef enum { C_STR, C_NUM, C_FORM } cell_kind;

typedef struct {
    int col, row;
    cell_kind kind;
    double num;
    double cached;      /* numeric result of a formula */
    char *text;         /* for C_STR: the value; for C_FORM: the evaluated result */
    char *formula;      /* for C_FORM: the original formula (without '=') */
    int style;
    int cached_err;     /* wubuval_err code when a C_FORM evaluated to an error
                           (0 = none). Lets dependents propagate the error
                           instead of coercing the error string to a number. */
} cell_t;

typedef struct {
    char *name;
    cell_t *cells; size_t n, cap;
} sheet_t;

typedef struct {
    int sheet;
    char *title;
    char *cats;
    char *vals;
} chart_t;

struct wubucell_book {
    sheet_t *sheets; size_t n, cap;
    int use_sst;
    struct wubucell_style *styles;
    chart_t *charts; size_t ncharts, capc;
};

/* formula engine integration: the book is the resolver context. */
typedef struct {
    wubucell_book *book;
    int cur_sheet;   /* 0-based sheet the current formula lives in */
    int *visit;      /* per-global-formula-index state: 0/1/2 */
} book_resolver;

/* sst (shared string table) */
typedef struct { char *s; int idx; } sst_ent;
typedef struct { sst_ent *e; size_t n, cap; } sst_t;
int cell_sst_add(sst_t *t, const char *s);

/* model.c */
sheet_t *cell_book_sheet(wubucell_book *b, int idx);
void cell_col_letter(int col, char *out);
char *cell_render_sheet(const wubucell_book *b, const sheet_t *s, size_t sheet_idx, sst_t *sst);
char *cell_render_chart(const chart_t *c, size_t idx);
void cell_xml_escape(FILE *m, const char *t);

/* eval.c */
int cell_br_resolve(void *ctx, const wubucell_ref *ref, wubuval *out);
void cell_eval_all(wubucell_book *b);

#endif /* WUBUCELL_CELL_INTERNAL_H */
