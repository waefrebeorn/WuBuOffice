#ifndef WUBUFORMULA_VALUE_H
#define WUBUFORMULA_VALUE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A formula cell evaluates to one of these. Errors use the Excel spelling so
 * they round-trip into <v> as a shared-string error literal. */
typedef enum {
    WV_NUM,      /* numeric (double) */
    WV_STR,      /* text */
    WV_BOOL,     /* TRUE/FALSE */
    WV_EMPTY,    /* blank cell reference */
    WV_ERR       /* error: wv_errcode holds the spelling */
} wubuval_kind;

typedef enum {
    WERR_NONE = 0,
    WERR_DIV0,     /* #DIV/0! */
    WERR_NAME,     /* #NAME?  */
    WERR_REF,      /* #REF!   */
    WERR_VALUE,    /* #VALUE! */
    WERR_NA,       /* #N/A    */
    WERR_NUM,      /* #NUM!   */
    WERR_CYCLE,    /* #CYCLE! (WuBuOffice extension: circular reference) */
    WERR_PARSE     /* #PARSE! (WuBuOffice extension: syntax error) */
} wubuval_err;

typedef struct {
    wubuval_kind kind;
    double num;        /* valid when kind == WV_NUM */
    int    boolean;    /* valid when kind == WV_BOOL */
    char  *str;        /* owned; valid when kind == WV_STR */
    wubuval_err err;   /* valid when kind == WV_ERR */
} wubuval;

/* Error value constructors take a spelling-free code; the evaluator fills the
 * canonical text. Empty/blank cells are WV_EMPTY. */
void wubuval_set_num(wubuval *v, double x);
void wubuval_set_str(wubuval *v, const char *s);
void wubuval_set_bool(wubuval *v, int b);
void wubuval_set_empty(wubuval *v);
void wubuval_set_err(wubuval *v, wubuval_err e);
void wubuval_free(wubuval *v);
void wubuval_copy(wubuval *dst, const wubuval *src);

/* Canonical error text e.g. "#DIV/0!". */
const char *wubuval_err_text(wubuval_err e);

/* A cell reference: 1-based col/row; sheet == -1 means "current sheet". */
typedef struct {
    int sheet;   /* -1 = current; -2 = qualified by sheet_name */
    int col;     /* 1-based */
    int row;     /* 1-based */
    int abs_sheet, abs_col, abs_row; /* $ anchored */
    char sheet_name[40]; /* set when sheet == -2 (qualified ref like Sheet1!A1) */
} wubucell_ref;

/* A range reference (inclusive). */
typedef struct {
    wubucell_ref a, b;
} wubucell_range;

/* Resolver: given a (possibly sheet-qualified) cell ref, fill *out with the
 * current value of that cell (which may itself trigger recursive evaluation if
 * the target is a formula). Must return 0 on success, -1 if the ref is invalid
 * (#REF!). The evaluator owns *out's str allocation. */
typedef int (*wubuformula_resolver)(void *ctx, const wubucell_ref *ref, wubuval *out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUFORMULA_VALUE_H */
