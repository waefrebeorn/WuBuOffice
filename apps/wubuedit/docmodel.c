/* docmodel.c — reconstruct a WordprocessingML document model from
 * word/document.xml so wubuedit can re-emit it losslessly.
 *
 * Single-pass SAX over wubuxml_parse. We track a small context stack instead
 * of the full element stack (we only care about w:p / w:tbl / w:tr / w:tc /
 * w:pStyle / w:b / w:t). Text runs accumulate into the "current paragraph";
 * paragraphs encountered inside a w:tc become that cell's content. */

#include "docmodel.h"
#include "../../src/wubuxml/parser.h"

#include <stdlib.h>
#include <string.h>

/* ---- dynamic arrays -------------------------------------------------- */
static int dm_block_add(dm_doc *d, dm_block **out) {
    if (d->n == d->cap) {
        size_t nc = d->cap ? d->cap * 2 : 8;
        dm_block *nb = realloc(d->blocks, nc * sizeof *nb);
        if (!nb) return -1;
        d->blocks = nb; d->cap = nc;
    }
    dm_block *b = &d->blocks[d->n++];
    memset(b, 0, sizeof *b);
    *out = b;
    return 0;
}

int wubuedit_docmodel_add_para(dm_doc *d, const char *style, int bold, const char *text) {
    dm_block *b;
    if (dm_block_add(d, &b) != 0) return -1;
    b->kind = DM_BLOCK_PARA;
    b->para.style = style ? strdup(style) : NULL;
    b->para.text  = text  ? strdup(text)  : NULL;
    b->para.bold  = bold;
    return 0;
}

int wubuedit_docmodel_add_table(dm_doc *d, dm_para **cells, size_t rows, size_t cols) {
    if (rows == 0 || cols == 0) { free(cells); return 0; }
    dm_block *b;
    if (dm_block_add(d, &b) != 0) { free(cells); return -1; }
    b->kind = DM_BLOCK_TABLE;
    b->table.rows = rows;
    b->table.cols = cols;
    size_t ncells = rows * cols;
    b->table.cells = calloc(ncells, sizeof(dm_para *));
    if (!b->table.cells) { free(cells); return -1; }
    for (size_t i = 0; i < ncells; i++) b->table.cells[i] = cells ? cells[i] : NULL;
    free(cells);   /* take ownership of the caller's row-major array */
    return 0;
}

/* ---- SAX state ------------------------------------------------------- */
typedef enum { S_TOP, S_TBL, S_TR, S_TC } dm_cur;

typedef struct {
    dm_doc *doc;

    /* current paragraph being filled (top-level or in a cell) */
    dm_para *cur_para;
    int      in_pstyle;   /* inside w:pStyle, capture @w:val next */
    int      in_t;        /* inside w:t, collect text */
    int      bold_seen;   /* a <w:b> appeared in the current paragraph run */

    /* table nesting */
    dm_cur   ctx;         /* where we are */
    dm_table *cur_table;  /* table under construction (when ctx==S_TBL) */
    dm_para **rowbuf;     /* cells of the row being built */
    size_t    rowcap, rown;
    size_t    rowcols;    /* max cols seen this row */
} dm_state;

/* forward decls */
static void capture_attrs(dm_state *st, const wubuxml_info *info);
static int flush_cell_row(dm_state *st);

static dm_para *new_para(void) {
    dm_para *p = calloc(1, sizeof *p);
    return p;
}
static void free_para(dm_para *p) {
    if (!p) return;
    free(p->style); free(p->text); free(p);
}

static int flush_cell_row(dm_state *st) {
    dm_table *t = st->cur_table;
    if (t->rows == 0) t->cols = st->rown ? st->rown : 1;
    /* grow the flat cell array to hold one more row of t->cols slots */
    dm_para **grown = realloc(t->cells, (t->rows + 1) * t->cols * sizeof(dm_para *));
    if (!grown) return -1;
    t->cells = grown;
    size_t base = t->rows * t->cols;
    for (size_t c = 0; c < t->cols; c++)
        t->cells[base + c] = (c < st->rown) ? st->rowbuf[c] : NULL;
    t->rows++;
    /* reset rowbuf for the next row */
    st->rown = 0; st->rowcols = 0;
    return 0;
}

static int on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    dm_state *st = (dm_state *)user;
    const char *name = info->name;

    if (evt == WUBUXML_EVT_START) {
        if (strcmp(name, "w:p") == 0) {
            if (st->ctx == S_TC) {
                /* paragraph inside a cell: treat as the cell content */
                st->cur_para = new_para();
                st->bold_seen = 0;
            } else if (st->ctx == S_TOP) {
                dm_block *b;
                if (dm_block_add(st->doc, &b) != 0) return -1;
                b->kind = DM_BLOCK_PARA;
                st->cur_para = &b->para;
                st->bold_seen = 0;
            }
            /* (inside w:tr but not yet in a tc: ignore) */
        } else if (strcmp(name, "w:pStyle") == 0) {
            st->in_pstyle = 1;
            capture_attrs(st, info);   /* reads @w:val */
        } else if (strcmp(name, "w:b") == 0) {
            st->bold_seen = 1;
        } else if (strcmp(name, "w:t") == 0) {
            st->in_t = 1;
        } else if (strcmp(name, "w:tbl") == 0) {
            if (st->ctx == S_TOP) {
                dm_block *b;
                if (dm_block_add(st->doc, &b) != 0) return -1;
                b->kind = DM_BLOCK_TABLE;
                st->cur_table = &b->table;
                memset(st->cur_table, 0, sizeof *st->cur_table);
                st->ctx = S_TBL;
            }
        } else if (strcmp(name, "w:tr") == 0) {
            if (st->ctx == S_TBL) {
                st->rown = 0; st->rowcols = 0;
                st->ctx = S_TR;
            }
        } else if (strcmp(name, "w:tc") == 0) {
            if (st->ctx == S_TR) st->ctx = S_TC;
        }
        return 0;
    }

    if (evt == WUBUXML_EVT_TEXT) {
        if (st->in_pstyle) {
            /* w:pStyle @w:val is delivered as an attribute, not text. The
             * value is captured in START via attr_val["w:val"]; handle there.
             * (kept for safety) */
        }
        if (st->in_t && st->cur_para) {
            /* append text to current paragraph */
            size_t add = info->text_len;
            size_t old = st->cur_para->text ? strlen(st->cur_para->text) : 0;
            char *nt = realloc(st->cur_para->text, old + add + 1);
            if (!nt) return -1;
            if (!old) nt[0] = '\0';
            memcpy(nt + old, info->text, add);
            nt[old + add] = '\0';
            st->cur_para->text = nt;
        }
        return 0;
    }

    /* WUBUXML_EVT_END */
    if (strcmp(name, "w:pStyle") == 0) {
        st->in_pstyle = 0;
    } else if (strcmp(name, "w:t") == 0) {
        st->in_t = 0;
    } else if (strcmp(name, "w:b") == 0) {
        /* already flagged bold_seen */
    } else if (strcmp(name, "w:p") == 0) {
        if (st->cur_para) {
            st->cur_para->bold = st->bold_seen;
            /* if inside a cell, move it into rowbuf */
            if (st->ctx == S_TC) {
                if (st->rown == st->rowcap) {
                    size_t nc = st->rowcap ? st->rowcap * 2 : 4;
                    dm_para **nb = realloc(st->rowbuf, nc * sizeof *nb);
                    if (!nb) return -1;
                    st->rowbuf = nb; st->rowcap = nc;
                }
                st->rowbuf[st->rown++] = st->cur_para;
                if (st->rown > st->rowcols) st->rowcols = st->rown;
                st->cur_para = NULL;
            } else if (st->ctx == S_TOP) {
                st->cur_para = NULL; /* top-level para committed to block */
            }
        }
    } else if (strcmp(name, "w:tc") == 0) {
        if (st->ctx == S_TC) st->ctx = S_TR;
    } else if (strcmp(name, "w:tr") == 0) {
        if (st->ctx == S_TR) {
            if (flush_cell_row(st) != 0) return -1;
            st->ctx = S_TBL;
        }
    } else if (strcmp(name, "w:tbl") == 0) {
        if (st->ctx == S_TBL) st->ctx = S_TOP;
    }
    return 0;
}

/* capture pStyle @w:val on START (attributes arrive with the START event) */
static void capture_attrs(dm_state *st, const wubuxml_info *info) {
    if (st->cur_para && strcmp(info->name, "w:pStyle") == 0) {
        for (int a = 0; a < info->attr_count; a++)
            if (strcmp(info->attr_name[a], "w:val") == 0) {
                free(st->cur_para->style);
                st->cur_para->style = strdup(info->attr_val[a]);
            }
    }
}

int wubuedit_docmodel_parse(const uint8_t *xml, size_t len, dm_doc *out) {
    memset(out, 0, sizeof *out);
    dm_state st;
    memset(&st, 0, sizeof st);
    st.doc = out;

    int rc = wubuxml_parse(xml, len, on_event, &st);
    /* the rowbuf array (and any dangling cur_para) is transient scratch; the
     * parsed cells live in the model's table (ownership transferred), so free
     * the scratch array itself here. */
    free(st.rowbuf);
    if (st.cur_para) free_para(st.cur_para);
    if (rc != 0) { wubuedit_docmodel_free(out); return -1; }
    return 0;
}

void wubuedit_docmodel_free(dm_doc *d) {
    if (!d) return;
    for (size_t i = 0; i < d->n; i++) {
        dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            free(b->para.style); free(b->para.text);
        } else if (b->kind == DM_BLOCK_TABLE) {
            for (size_t r = 0; r < b->table.rows; r++)
                for (size_t c = 0; c < b->table.cols; c++) {
                    dm_para *p = b->table.cells[r * b->table.cols + c];
                    if (p) free_para(p);
                }
            free(b->table.cells);
        }
    }
    free(d->blocks);
    d->blocks = NULL; d->n = d->cap = 0;
}
