/* WuBuOffice -- wubuformula/functions/logic
 * Logical functions: IF, IFERROR, AND, OR, NOT, IFS, SWITCH.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "registry.h"
#include "value_util.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static void f_if(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    if (wubu_to_bool(&a[0])) { if (na >= 2) wubuval_copy(out, &a[1]); else wubuval_set_bool(out, 1); }
    else { if (na >= 3) wubuval_copy(out, &a[2]); else wubuval_set_bool(out, 0); }
}

static void f_iferror(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    if (a[0].kind == WV_ERR) wubuval_copy(out, &a[1]);
    else wubuval_copy(out, &a[0]);
}

static void f_and(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)ranges;
    int r = 1;
    for (int i = 0; i < fn; i++) if (!wubu_to_bool(&flat[i])) { r = 0; break; }
    wubuval_set_bool(out, r);
}

static void f_or(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)ranges;
    int r = 0;
    for (int i = 0; i < fn; i++) if (wubu_to_bool(&flat[i])) { r = 1; break; }
    wubuval_set_bool(out, r);
}

static void f_not(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)na; (void)flat; (void)fn; (void)ranges;
    wubuval_set_bool(out, !wubu_to_bool(&a[0]));
}

/* IFS(pair1, pair2, ...) — returns the value of the first TRUE condition's
 * result. Odd arg count; each pair is (condition, value). #NA if none true. */
static void f_ifs(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2 || (na % 2) != 0) { wubuval_set_err(out, WERR_VALUE); return; }
    for (int i = 0; i + 1 < na; i += 2) {
        if (wubu_to_bool(&a[i])) { wubuval_copy(out, &a[i + 1]); return; }
    }
    wubuval_set_err(out, WERR_NA);
}

/* SWITCH(expr, value1, result1, [value2, result2, ...], [default]) */
static void f_switch(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubuval *e = &a[0];
    int i = 1;
    while (i + 1 < na) { /* value, result */
        const wubuval *v = &a[i], *res = &a[i + 1];
        int eq = 0;
        if (e->kind == WV_NUM && v->kind == WV_NUM) eq = (e->num == v->num);
        else if (e->kind == WV_STR && v->kind == WV_STR)
            eq = (wubu_strcasecmp(e->str ? e->str : "", v->str ? v->str : "") == 0);
        else if (e->kind == WV_BOOL && v->kind == WV_BOOL) eq = (e->boolean == v->boolean);
        if (eq) { wubuval_copy(out, res); return; }
        i += 2;
    }
    if (i < na) wubuval_copy(out, &a[i]); /* trailing default */
    else wubuval_set_err(out, WERR_NA);
}

void wubu_register_logic(wubu_func_registrar reg) {
    reg("IF", f_if);
    reg("IFERROR", f_iferror);
    reg("AND", f_and);
    reg("OR", f_or);
    reg("NOT", f_not);
    reg("IFS", f_ifs);
    reg("SWITCH", f_switch);
}
