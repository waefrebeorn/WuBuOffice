/* WuBuOffice -- apps/wubucell/cell_eval
 * Formula evaluation: the wubucell_book is the resolver context. Each formula
 * cell is evaluated (recursively, with circular-reference detection) and its
 * result cached back into the cell so the worksheet writer emits a real <v>.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "cell_internal.h"
#include "value_util.h"

/* Resolve a cell ref to a value. Recurses into formula cells through the same
 * resolver; cycle detection uses R->visit keyed by a global formula index. */
int cell_br_resolve(void *ctx, const wubucell_ref *ref, wubuval *out) {
    book_resolver *R = (book_resolver *)ctx;
    wubucell_book *b = R->book;
    int sheet = (ref->sheet == -1) ? R->cur_sheet : -1;
    if (ref->sheet == -2) {
        for (size_t i = 0; i < b->n; i++)
            if (wubu_strcasecmp(b->sheets[i].name, ref->sheet_name) == 0) { sheet = (int)i; break; }
        if (sheet < 0) { wubuval_set_err(out, WERR_REF); return 0; }
    } else if (ref->sheet >= 0) {
        sheet = ref->sheet;
    }
    if (sheet < 0 || (size_t)sheet >= b->n) { wubuval_set_err(out, WERR_REF); return 0; }
    const sheet_t *s = &b->sheets[sheet];
    for (size_t i = 0; i < s->n; i++) {
        const cell_t *c = &s->cells[i];
        if (c->col != ref->col || c->row != ref->row) continue;
        if (c->kind == C_NUM) { wubuval_set_num(out, c->num); return 0; }
        if (c->kind == C_STR) {
            /* A formula that errored is stored as a C_STR bearing the error
             * literal; surface it as a real error value so dependents
             * propagate it (Excel: =A1*2 where A1 is #CYCLE! -> #CYCLE!). */
            if (c->cached_err != 0 && c->text && c->text[0] == '#')
                { wubuval_set_err(out, (wubuval_err)c->cached_err); return 0; }
            wubuval_set_str(out, c->text ? c->text : ""); return 0;
        }
        if (c->kind == C_FORM) {
            int fi = 0, found = -1;
            for (size_t sh = 0; sh < b->n && found < 0; sh++)
                for (size_t j = 0; j < b->sheets[sh].n; j++)
                    if (b->sheets[sh].cells[j].kind == C_FORM) {
                        if ((int)sh == sheet && b->sheets[sh].cells[j].col == c->col && b->sheets[sh].cells[j].row == c->row) { found = fi; break; }
                        fi++;
                    }
            if (found >= 0) {
                /* Circular-reference detection. visit[found] is 1 while this
                 * formula cell is mid-evaluation up the call stack; if we
                 * re-enter it, that is a cycle -> surface #CYCLE! instead of
                 * recursing forever (which stack-overflows the app). */
                if (R->visit[found] == 1) {
                    wubuval_set_err(out, WERR_CYCLE);
                    return 0;
                }
                wubuval v; memset(&v, 0, sizeof v);
                int *fsheet2 = NULL, *fcol2 = NULL, *frow2 = NULL; int nf2 = 0;
                for (size_t sh2 = 0; sh2 < b->n; sh2++)
                    for (size_t j2 = 0; j2 < b->sheets[sh2].n; j2++)
                        if (b->sheets[sh2].cells[j2].kind == C_FORM) nf2++;
                if (nf2) {
                    fsheet2 = malloc(sizeof(int)*nf2); fcol2 = malloc(sizeof(int)*nf2); frow2 = malloc(sizeof(int)*nf2);
                    int f2 = 0;
                    for (size_t sh2 = 0; sh2 < b->n; sh2++)
                        for (size_t j2 = 0; j2 < b->sheets[sh2].n; j2++)
                            if (b->sheets[sh2].cells[j2].kind == C_FORM) { fsheet2[f2] = (int)sh2; fcol2[f2] = b->sheets[sh2].cells[j2].col; frow2[f2] = b->sheets[sh2].cells[j2].row; f2++; }
                    R->visit[found] = 1;
                    wubuval v2; memset(&v2, 0, sizeof v2);
                    wubu_formula_eval(c->formula ? c->formula : "", cell_br_resolve, R, &v2);
                    R->visit[found] = 2;
                    v = v2;
                    free(fsheet2); free(fcol2); free(frow2);
                }
                *out = v;
                return 0;
            }
            wubuval_set_empty(out); return 0;
        }
    }
    wubuval_set_empty(out);
    return 0;
}

void cell_eval_all(wubucell_book *b) {
    int nf = 0;
    for (size_t sh = 0; sh < b->n; sh++)
        for (size_t i = 0; i < b->sheets[sh].n; i++)
            if (b->sheets[sh].cells[i].kind == C_FORM) nf++;
    if (!nf) return;
    int *fsheet = malloc(sizeof(int)*nf), *fcol = malloc(sizeof(int)*nf), *frow = malloc(sizeof(int)*nf);
    int fi = 0;
    for (size_t sh = 0; sh < b->n; sh++)
        for (size_t i = 0; i < b->sheets[sh].n; i++)
            if (b->sheets[sh].cells[i].kind == C_FORM) {
                fsheet[fi] = (int)sh; fcol[fi] = b->sheets[sh].cells[i].col; frow[fi] = b->sheets[sh].cells[i].row; fi++;
            }
    int *visit = calloc((size_t)nf, sizeof(int));
    int *skipped = calloc((size_t)nf, sizeof(int));   /* formula cells the main loop
                                                        skipped (only reached via a
                                                        sub-evaluation -> cycle members) */
    book_resolver R; memset(&R, 0, sizeof R); R.book = b; R.visit = visit;
    for (int k = 0; k < nf; k++) {
        if (visit[k] == 2) { skipped[k] = 1; continue; }
        R.cur_sheet = fsheet[k];
        sheet_t *s = &b->sheets[fsheet[k]];
        cell_t *cell = NULL;
        for (size_t i = 0; i < s->n; i++)
            if (s->cells[i].col == fcol[k] && s->cells[i].row == frow[k]) { cell = &s->cells[i]; break; }
        if (!cell) continue;
        visit[k] = 1;
        wubuval v; memset(&v, 0, sizeof v);
        wubu_formula_eval(cell->formula ? cell->formula : "", cell_br_resolve, &R, &v);
        visit[k] = 2;
        if (v.kind == WV_NUM) { cell->cached = v.num; free(cell->text); cell->text = NULL; cell->cached_err = 0; }
        else if (v.kind == WV_BOOL) { cell->cached = v.boolean ? 1.0 : 0.0; free(cell->text); cell->text = NULL; cell->cached_err = 0; }
        else if (v.kind == WV_STR) { free(cell->text); cell->text = strdup(v.str ? v.str : ""); cell->kind = C_STR; cell->cached_err = 0; }
        else if (v.kind == WV_ERR) { free(cell->text); cell->text = strdup(wubuval_err_text(v.err)); cell->kind = C_STR; cell->cached_err = (int)v.err; }
        else { cell->cached = 0; free(cell->text); cell->text = NULL; }
        wubuval_free(&v);
    }
    /* Any formula cell the main loop skipped (only ever reached via a
     * sub-evaluation, i.e. it sits inside a circular reference) but never had
     * a result assigned above is a cycle member. Mark it #CYCLE! so every
     * member of the cycle surfaces the error, matching Excel. Normal numeric
     * formulas are NOT skipped (their k ran the eval block) and are left
     * untouched. */
    for (int k = 0; k < nf; k++) {
        if (!skipped[k]) continue;
        sheet_t *s = &b->sheets[fsheet[k]];
        for (size_t i = 0; i < s->n; i++) {
            cell_t *c = &s->cells[i];
            if (c->col == fcol[k] && c->row == frow[k] && c->kind == C_FORM && !c->text) {
                c->text = strdup(wubuval_err_text(WERR_CYCLE));
                c->kind = C_STR;
            }
        }
    }
    free(fsheet); free(fcol); free(frow); free(visit); free(skipped);
}
