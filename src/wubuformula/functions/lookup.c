/* WuBuOffice -- wubuformula/functions/lookup
 * Range-aware lookup and conditional-aggregate functions. These operate on a
 * real 2D grid supplied through the per-argument range geometry (ranges[]),
 * not a flat value list. SUMIF/COUNTIF/AVERAGEIF reuse wubu_match_criteria
 * from value_util (single implementation shared across modules).
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "registry.h"
#include "value_util.h"

#include <stdlib.h>
#include <string.h>

static void f_vlookup(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    int ok; double key = wubu_to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int col = (int)wubu_to_num(&a[2], &(int){0}); if (col < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *t = &ranges[1];
    if (!t->grid || t->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    if (col > t->cols) { wubuval_set_err(out, WERR_REF); return; }
    for (int r = 0; r < t->rows; r++) {
        const wubuval *c0 = &t->grid[r * t->cols];
        int ok2; double d = wubu_to_num(c0, &ok2);
        if (ok2 && d == key) { wubuval_copy(out, &t->grid[r * t->cols + (col - 1)]); return; }
    }
    wubuval_set_err(out, WERR_NA);
}
static void f_hlookup(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    int ok; double key = wubu_to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int row = (int)wubu_to_num(&a[2], &(int){0}); if (row < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *t = &ranges[1];
    if (!t->grid || t->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    if (row > t->rows) { wubuval_set_err(out, WERR_REF); return; }
    for (int c = 0; c < t->cols; c++) {
        const wubuval *c0 = &t->grid[c];
        int ok2; double d = wubu_to_num(c0, &ok2);
        if (ok2 && d == key) { wubuval_copy(out, &t->grid[(row - 1) * t->cols + c]); return; }
    }
    wubuval_set_err(out, WERR_NA);
}
static void f_index(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const wubu_func_range *t = &ranges[0];
    if (!t->grid || t->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    int row = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 1;
    int col = (na >= 3) ? (int)wubu_to_num(&a[2], &(int){0}) : 1;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    if (row < 1 || row > t->rows || col < 1 || col > t->cols) { wubuval_set_err(out, WERR_REF); return; }
    wubuval_copy(out, &t->grid[(row - 1) * t->cols + (col - 1)]);
}
static void f_match(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *t = &ranges[1]; /* second arg is the lookup array */
    if (!t->grid || t->rows * t->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    int n = t->rows * t->cols;
    for (int i = 0; i < n; i++) {
        int ok; double d = wubu_to_num(&t->grid[i], &ok);
        int match = 0;
        if (ok && a[0].kind == WV_NUM) match = (d == wubu_to_num(&a[0], &(int){0}));
        else if (a[0].kind == WV_STR) {
            const char *s = wubu_to_str(&a[0]);
            if (t->grid[i].kind == WV_STR) match = (wubu_strcasecmp(t->grid[i].str ? t->grid[i].str : "", s ? s : "") == 0);
        }
        if (match) { wubuval_set_num(out, (double)(i + 1)); return; }
    }
    wubuval_set_err(out, WERR_NA);
}
static void f_choose(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    int idx = (int)wubu_to_num(&a[0], &(int){0});
    if (idx < 1 || idx > na - 1) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_copy(out, &a[idx]); /* a[0] is index; a[1..] are choices */
}

/* SUMIF/COUNTIF/AVERAGEIF: a[1] is the criteria; sum/criteria ranges are
 * ranges[0] and (optional) ranges[2]. Single shared match engine. */
static void f_sumif(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *cr = &ranges[0];
    if (!cr->grid || cr->rows < 1 || cr->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *sr = (na >= 3) ? &ranges[2] : cr;
    double sum = 0;
    for (int i = 0; i < cr->rows * cr->cols; i++) {
        if (wubu_match_criteria(&cr->grid[i], &a[1])) {
            int ri = i / cr->cols, ci = i % cr->cols;
            const wubuval *sv = (na >= 3 && sr->grid) ? &sr->grid[ri * sr->cols + ci] : &cr->grid[i];
            int ok; double d = wubu_to_num(sv, &ok); if (ok) sum += d;
        }
    }
    wubuval_set_num(out, sum);
}
static void f_countif(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *cr = &ranges[0];
    if (!cr->grid || cr->rows < 1 || cr->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    int c = 0;
    for (int i = 0; i < cr->rows * cr->cols; i++) if (wubu_match_criteria(&cr->grid[i], &a[1])) c++;
    wubuval_set_num(out, (double)c);
}
static void f_averageif(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *cr = &ranges[0];
    if (!cr->grid || cr->rows < 1 || cr->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *sr = (na >= 3) ? &ranges[2] : cr;
    double sum = 0; int c = 0;
    for (int i = 0; i < cr->rows * cr->cols; i++) {
        if (wubu_match_criteria(&cr->grid[i], &a[1])) {
            int ri = i / cr->cols, ci = i % cr->cols;
            const wubuval *sv = (na >= 3 && sr->grid) ? &sr->grid[ri * sr->cols + ci] : &cr->grid[i];
            int ok; double d = wubu_to_num(sv, &ok); if (ok) { sum += d; c++; }
        }
    }
    wubuval_set_num(out, c ? sum / c : 0.0);
}

/* SUMIFS(sum_range, crit_range1, crit1 [, crit_range2, crit2, ...]) */
static void f_sumifs(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 3 || (na - 1) % 2 != 0) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *sr = &ranges[0];
    if (!sr->grid || sr->rows < 1 || sr->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    int n = sr->rows * sr->cols;
    double sum = 0;
    for (int i = 0; i < n; i++) {
        int ok_all = 1;
        for (int p = 1; p + 1 < na; p += 2) {
            const wubu_func_range *cr = &ranges[p];
            int ri = i / sr->cols, ci = i % sr->cols;
            const wubuval *cv = (cr->grid && cr->rows * cr->cols == n)
                ? &cr->grid[ri * cr->cols + ci] : &cr->grid[0];
            if (!wubu_match_criteria(cv, &a[p + 1])) { ok_all = 0; break; }
        }
        if (ok_all) { int ok; double d = wubu_to_num(&sr->grid[i], &ok); if (ok) sum += d; }
    }
    wubuval_set_num(out, sum);
}
/* COUNTIFS(crit_range1, crit1 [, crit_range2, crit2, ...]) */
static void f_countifs(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)a;
    if (na < 2 || na % 2 != 0) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *cr0 = &ranges[0];
    if (!cr0->grid || cr0->rows < 1 || cr0->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    int n = cr0->rows * cr0->cols;
    int c = 0;
    for (int i = 0; i < n; i++) {
        int ok_all = 1;
        for (int p = 0; p + 1 < na; p += 2) {
            const wubu_func_range *cr = &ranges[p];
            int ri = i / cr0->cols, ci = i % cr0->cols;
            const wubuval *cv = (cr->grid && cr->rows * cr->cols == n)
                ? &cr->grid[ri * cr->cols + ci] : &cr->grid[0];
            if (!wubu_match_criteria(cv, &a[p + 1])) { ok_all = 0; break; }
        }
        if (ok_all) c++;
    }
    wubuval_set_num(out, (double)c);
}

void wubu_register_lookup(wubu_func_registrar reg) {
    reg("VLOOKUP", f_vlookup);
    reg("HLOOKUP", f_hlookup);
    reg("INDEX", f_index);
    reg("MATCH", f_match);
    reg("CHOOSE", f_choose);
    reg("SUMIF", f_sumif);
    reg("COUNTIF", f_countif);
    reg("AVERAGEIF", f_averageif);
    reg("SUMIFS", f_sumifs);
    reg("COUNTIFS", f_countifs);
}
