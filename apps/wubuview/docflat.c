/* docflat.c -- flatten a normalized JSON model to readable plain text. */
#include "docflat.h"
#include "json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* growable text buffer */
typedef struct { char *p; size_t n, cap; int oom; } Sb;

static void sb_ensure(Sb *b, size_t extra) {
    if (b->oom) return;
    if (b->n + extra + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->n + extra + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) { b->oom = 1; return; }
        b->p = np; b->cap = nc;
    }
}
static void sb_puts(Sb *b, const char *s) {
    if (!s) return;
    size_t l = strlen(s);
    sb_ensure(b, l);
    if (b->oom) return;
    memcpy(b->p + b->n, s, l);
    b->n += l;
}
static void sb_putc(Sb *b, char c) {
    sb_ensure(b, 1);
    if (b->oom) return;
    b->p[b->n++] = c;
}
static void sb_num(Sb *b, double d) {
    char tmp[64];
    if (d == (double)(long long)d) snprintf(tmp, sizeof tmp, "%lld", (long long)d);
    else snprintf(tmp, sizeof tmp, "%g", d);
    sb_puts(b, tmp);
}

/* Append the scalar text of a value (string/number/bool); returns 1 if it
 * wrote something. */
static int append_scalar(Sb *b, const JVal *v) {
    if (!v) return 0;
    switch (j_type(v)) {
        case J_STR:  sb_puts(b, j_as_str(v)); return 1;
        case J_NUM:  sb_num(b, j_as_num(v));  return 1;
        case J_BOOL: sb_puts(b, j_as_bool(v) ? "true" : "false"); return 1;
        default: return 0;
    }
}

/* Render a document-model blocks array: one paragraph per block, using "text"
 * (headings get an underline). Returns 1 if handled. */
static int flatten_doc(Sb *b, const JVal *root) {
    const JVal *blocks = j_obj_get(root, "blocks");
    if (!blocks || j_type(blocks) != J_ARR) return 0;
    size_t n = j_len(blocks);
    for (size_t i = 0; i < n; i++) {
        const JVal *blk = j_arr_at(blocks, i);
        if (!blk || j_type(blk) != J_OBJ) continue;
        const JVal *txt = j_obj_get(blk, "text");
        const JVal *kind = j_obj_get(blk, "kind");
        const JVal *style = j_obj_get(blk, "style");
        const char *ks = (kind && j_type(kind) == J_STR) ? j_as_str(kind) : "";
        const char *ss = (style && j_type(style) == J_STR) ? j_as_str(style) : "";
        const char *ts = (txt && j_type(txt) == J_STR) ? j_as_str(txt) : "";
        if (ts && ts[0]) {
            /* a block is a heading if its kind OR its style says so
             * (md/docx use style="Heading1"; ocr/others use kind="heading") */
            int is_head = (ks[0] && (strstr(ks, "head") || strstr(ks, "title"))) ||
                          (ss[0] && (strstr(ss, "Head") || strstr(ss, "head") ||
                                     strstr(ss, "Title") || strstr(ss, "title")));
            sb_puts(b, ts);
            sb_putc(b, '\n');
            /* underline headings/titles */
            if (is_head) {
                size_t l = strlen(ts);
                for (size_t k = 0; k < l && k < 80; k++) sb_putc(b, '=');
                sb_putc(b, '\n');
            }
            sb_putc(b, '\n');   /* blank line between blocks */
        }
    }
    return 1;
}

/* Render a sheet model: rows/cells arrays as tab-separated lines. Handled=1. */
static int flatten_sheet(Sb *b, const JVal *root) {
    const JVal *rows = j_obj_get(root, "rows");
    if (!rows || j_type(rows) != J_ARR) rows = j_obj_get(root, "cells");
    if (!rows || j_type(rows) != J_ARR) return 0;
    size_t nr = j_len(rows);
    for (size_t r = 0; r < nr; r++) {
        const JVal *row = j_arr_at(rows, r);
        if (row && j_type(row) == J_ARR) {
            size_t nc = j_len(row);
            for (size_t c = 0; c < nc; c++) {
                const JVal *cell = j_arr_at(row, c);
                /* a cell may be a scalar or {"v":...}/{"text":...} */
                if (cell && j_type(cell) == J_OBJ) {
                    const JVal *cv = j_obj_get(cell, "text");
                    if (!cv) cv = j_obj_get(cell, "v");
                    if (!cv) cv = j_obj_get(cell, "value");
                    append_scalar(b, cv);
                } else {
                    append_scalar(b, cell);
                }
                if (c + 1 < nc) sb_putc(b, '\t');
            }
        }
        sb_putc(b, '\n');
    }
    return 1;
}

char *docflat_from_json(const char *model_json) {
    if (!model_json) return NULL;
    const JVal *root = j_parse(model_json, NULL);
    if (!root) {
        /* not JSON: just echo the raw text */
        char *dup = malloc(strlen(model_json) + 1);
        if (dup) strcpy(dup, model_json);
        return dup;
    }

    Sb b = {0};
    int handled = 0;

    /* doc_json() wraps the normalized model under a "model" key:
     *   {"model":{"type":"document","blocks":[...]}}
     * Unwrap it so the shape detectors below see the real model. */
    const JVal *doc = root;
    if (j_type(root) == J_OBJ) {
        const JVal *inner = j_obj_get(root, "model");
        if (inner && (j_type(inner) == J_OBJ || j_type(inner) == J_ARR)) doc = inner;
    }

    if (j_type(doc) == J_OBJ) {
        /* text wrapper first */
        const JVal *t = j_obj_get(doc, "text");
        if (t && j_type(t) == J_STR && !j_obj_get(doc, "blocks")) {
            sb_puts(&b, j_as_str(t));
            sb_putc(&b, '\n');
            handled = 1;
        }
        if (!handled) handled = flatten_doc(&b, doc);
        if (!handled) handled = flatten_sheet(&b, doc);
    }

    if (!handled) {
        /* generic fallback: show the compact JSON so nothing is hidden */
        free(b.p); b.p = NULL; b.n = b.cap = 0; b.oom = 0;
        char *emit = j_emit(doc);
        if (emit) { sb_puts(&b, emit); free(emit); }
    }

    j_free((JVal *)root);

    if (b.oom) { free(b.p); return NULL; }
    if (!b.p) { b.p = malloc(1); if (b.p) b.p[0] = '\0'; return b.p; }
    b.p[b.n] = '\0';
    return b.p;
}
