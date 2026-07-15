/* model_from_json.c -- inverse of model_json.c. See model_from_json.h.
 * Clean-room C11; reuses wubujson for parsing. */
#include "model_from_json.h"

#include "json.h"   /* wubujson (src/wubujson) */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------- document ---------- */
dm_doc *wubuconv_doc_from_json(const char *json) {
    const char *end = NULL;
    JVal *root = j_parse(json, &end);
    if (!root || j_type(root) != J_OBJ) { j_free(root); return NULL; }
    const JVal *blocks = j_obj_get(root, "blocks");
    if (!blocks || j_type(blocks) != J_ARR) { j_free(root); return NULL; }

    dm_doc *d = malloc(sizeof *d);
    if (!d) { j_free(root); return NULL; }
    memset(d, 0, sizeof *d);

    int ok = 1;
    for (size_t i = 0; i < j_len(blocks); i++) {
        const JVal *b = j_arr_at(blocks, i);
        if (!b || j_type(b) != J_OBJ) { ok = 0; break; }
        const JVal *kind = j_obj_get(b, "kind");
        if (!kind || j_type(kind) != J_STR) { ok = 0; break; }
        const char *ks = j_as_str(kind);
        if (strcmp(ks, "paragraph") == 0) {
            const JVal *style = j_obj_get(b, "style");  /* may be null */
            const JVal *bold  = j_obj_get(b, "bold");
            const JVal *text  = j_obj_get(b, "text");
            const char *st = (style && j_type(style) == J_STR) ? j_as_str(style) : NULL;
            const char *tx = (text && j_type(text) == J_STR) ? j_as_str(text) : "";
            int bo = (bold && j_type(bold) == J_BOOL) ? j_as_bool(bold) : 0;
            if (wubuedit_docmodel_add_para(d, st, bo, tx) != 0) { ok = 0; break; }
        } else if (strcmp(ks, "table") == 0) {
            const JVal *rows = j_obj_get(b, "rows");
            const JVal *cols = j_obj_get(b, "cols");
            const JVal *cells = j_obj_get(b, "cells");
            if (!rows || !cols || !cells || j_type(cells) != J_ARR) { ok = 0; break; }
            size_t R = (size_t)j_as_num(rows);
            size_t C = (size_t)j_as_num(cols);
            /* cells is an array of R arrays, each C strings */
            dm_para **cp = calloc(R * C, sizeof *cp);
            if (!cp) { ok = 0; break; }
            int built = 1;
            for (size_t r = 0; r < R && built; r++) {
                const JVal *row = j_arr_at(cells, r);
                if (!row || j_type(row) != J_ARR) { built = 0; break; }
                for (size_t c = 0; c < C; c++) {
                    const JVal *cell = j_arr_at(row, c);
                    const char *cs = (cell && j_type(cell) == J_STR) ? j_as_str(cell) : "";
                    dm_para *p = malloc(sizeof *p);
                    if (!p) { built = 0; break; }
                    memset(p, 0, sizeof *p);
                    p->text = strdup(cs ? cs : "");
                    cp[r * C + c] = p;
                }
            }
            if (built) {
                /* add_table takes ownership of the cp array (frees it on every
                 * path) and, on success, of every para -- never touch cp after. */
                if (wubuedit_docmodel_add_table(d, cp, R, C) != 0) ok = 0;
            } else {
                /* construction failed before hand-off: we still own cp + paras */
                for (size_t k = 0; k < R * C; k++)
                    if (cp[k]) { free(cp[k]->text); free(cp[k]); }
                free(cp);
                ok = 0;
            }
            if (!ok) break;
        } else { ok = 0; break; }
    }
    j_free(root);
    if (!ok) { wubuedit_docmodel_free(d); free(d); return NULL; }
    return d;
}

/* ---------- workbook ---------- */
wubucell_book *wubuconv_book_from_json(const char *json) {
    const char *end = NULL;
    JVal *root = j_parse(json, &end);
    if (!root || j_type(root) != J_OBJ) { j_free(root); return NULL; }
    const JVal *sheets = j_obj_get(root, "sheets");
    if (!sheets || j_type(sheets) != J_ARR) { j_free(root); return NULL; }

    wubucell_book *b = wubucell_create();
    if (!b) { j_free(root); return NULL; }

    int ok = 1;
    for (size_t s = 0; s < j_len(sheets); s++) {
        const JVal *sh = j_arr_at(sheets, s);
        if (!sh || j_type(sh) != J_OBJ) { ok = 0; break; }
        const JVal *name = j_obj_get(sh, "name");
        int idx = wubucell_sheet(b, (name && j_type(name) == J_STR) ? j_as_str(name) : "Sheet1");
        const JVal *cells = j_obj_get(sh, "cells");
        if (!cells || j_type(cells) != J_ARR) { ok = 0; break; }
        for (size_t k = 0; k < j_len(cells); k++) {
            const JVal *c = j_arr_at(cells, k);
            if (!c || j_type(c) != J_OBJ) continue;
            const JVal *rv = j_obj_get(c, "r");
            const JVal *cv = j_obj_get(c, "c");
            const JVal *kv = j_obj_get(c, "kind");
            const JVal *vv = j_obj_get(c, "value");
            int r = (int)j_as_num(rv), col = (int)j_as_num(cv);
            const char *ks = (kv && j_type(kv) == J_STR) ? j_as_str(kv) : "str";
            if (strcmp(ks, "num") == 0) {
                wubucell_cell_n(b, idx, col, r, vv ? j_as_num(vv) : 0);
            } else if (strcmp(ks, "formula") == 0) {
                const JVal *cv2 = j_obj_get(c, "cached");
                wubucell_cell_f(b, idx, col, r, (vv && j_type(vv) == J_STR) ? j_as_str(vv) : "",
                                cv2 ? j_as_num(cv2) : 0);
            } else {
                wubucell_cell_s(b, idx, col, r, (vv && j_type(vv) == J_STR) ? j_as_str(vv) : "");
            }
        }
    }
    j_free(root);
    if (!ok) { wubucell_free(b); return NULL; }
    return b;
}

/* ---------- presentation ---------- */
wubushow_pres *wubuconv_pres_from_json(const char *json) {
    const char *end = NULL;
    JVal *root = j_parse(json, &end);
    if (!root || j_type(root) != J_OBJ) { j_free(root); return NULL; }
    const JVal *slides = j_obj_get(root, "slides");
    if (!slides || j_type(slides) != J_ARR) { j_free(root); return NULL; }

    wubushow_pres *p = wubushow_create();
    if (!p) { j_free(root); return NULL; }

    int ok = 1;
    for (size_t i = 0; i < j_len(slides); i++) {
        const JVal *sl = j_arr_at(slides, i);
        if (!sl || j_type(sl) != J_OBJ) { ok = 0; break; }
        const JVal *title = j_obj_get(sl, "title");
        const JVal *body  = j_obj_get(sl, "body");
        const char *t = (title && j_type(title) == J_STR) ? j_as_str(title) : "";
        const char *bd = (body && j_type(body) == J_STR) ? j_as_str(body) : "";
        if (wubushow_slide(p, t, bd) != 0) { ok = 0; break; }
    }
    j_free(root);
    if (!ok) { wubushow_free(p); return NULL; }
    return p;
}
