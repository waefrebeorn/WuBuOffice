/* WuBuOffice -- wubuformula/value
 * Formula value model + cell reference types.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "value.h"

#include <stdlib.h>
#include <string.h>

void wubuval_set_num(wubuval *v, double x) {
    v->kind = WV_NUM; v->num = x; v->str = NULL; v->err = WERR_NONE;
}
void wubuval_set_str(wubuval *v, const char *s) {
    v->kind = WV_STR; v->num = 0; v->str = s ? strdup(s) : strdup(""); v->boolean = 0; v->err = WERR_NONE;
}
void wubuval_set_bool(wubuval *v, int b) {
    v->kind = WV_BOOL; v->boolean = b ? 1 : 0; v->str = NULL; v->err = WERR_NONE;
}
void wubuval_set_empty(wubuval *v) {
    v->kind = WV_EMPTY; v->num = 0; v->str = NULL; v->err = WERR_NONE;
}
void wubuval_set_err(wubuval *v, wubuval_err e) {
    v->kind = WV_ERR; v->err = e; v->str = NULL; v->num = 0;
}
void wubuval_free(wubuval *v) {
    if (v && v->kind == WV_STR) { free(v->str); v->str = NULL; }
    v->kind = WV_EMPTY;
}
void wubuval_copy(wubuval *dst, const wubuval *src) {
    wubuval_free(dst);
    *dst = *src;
    if (src->kind == WV_STR && src->str) dst->str = strdup(src->str);
}

const char *wubuval_err_text(wubuval_err e) {
    switch (e) {
        case WERR_DIV0:   return "#DIV/0!";
        case WERR_NAME:   return "#NAME?";
        case WERR_REF:    return "#REF!";
        case WERR_VALUE:  return "#VALUE!";
        case WERR_NA:     return "#N/A";
        case WERR_NUM:    return "#NUM!";
        case WERR_CYCLE:  return "#CYCLE!";
        case WERR_PARSE:  return "#PARSE!";
        default:          return "";
    }
}
