/* extract.c — SAX-based plain-text extraction for OOXML documents.
 *
 * One streaming handler (driven by wubuxml_parse) feeds three extraction
 * modes. We never build a full DOM: a small element-name stack plus a text
 * accumulator is all the state we keep, so memory is bounded by the longest
 * run, not the whole document.
 *
 * Modes:
 *   EXTRACT_DOCX — WordprocessingML. <w:t> runs concatenated; <w:p> ends a
 *                  line. (<w:tab>/<w:br> handled as whitespace/newline.)
 *   EXTRACT_PPTX — PresentationML. <a:t> runs concatenated within a paragraph;
 *                  <a:p> ends a line.
 *   EXTRACT_XLSX — SpreadsheetML. Shared-string table built first; each
 *                  <c> cell appended to the current row (gaps filled with
 *                  empty cells using the cell's column ref); <row> ends a line.
 */

#include "extract.h"
#include "../wubuxml/parser.h"

#include <stdlib.h>
#include <string.h>

/* ---- shared dynamic string buffer ----------------------------------- */
typedef struct { char *p; size_t len, cap; } sbuf;
static int sbuf_push(sbuf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return -1;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
    return 0;
}
static int sbuf_pushc(sbuf *b, char c) { return sbuf_push(b, &c, 1); }
static void sbuf_free(sbuf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }

/* ---- element-name stack (kept minimal: just the names) -------------- */
typedef struct { const char **v; size_t n, cap; } estr;
static int estr_push(estr *e, const char *name) {
    if (e->n == e->cap) {
        size_t nc = e->cap ? e->cap * 2 : 16;
        const char **nv = realloc(e->v, nc * sizeof *nv);
        if (!nv) return -1;
        e->v = nv; e->cap = nc;
    }
    e->v[e->n++] = name;
    return 0;
}
static const char *estr_top(const estr *e) { return e->n ? e->v[e->n - 1] : NULL; }
static void estr_pop(estr *e) { if (e->n > 0) e->n--; }
static void estr_free(estr *e) { free(e->v); e->v = NULL; e->n = e->cap = 0; }

/* ---- shared-string table (for xlsx) --------------------------------- */
typedef struct { char **s; size_t n, cap; } sstable;
static int sstable_add(sstable *t, char *str) {
    if (t->n == t->cap) {
        size_t nc = t->cap ? t->cap * 2 : 16;
        char **ns = realloc(t->s, nc * sizeof *ns);
        if (!ns) { free(str); return -1; }
        t->s = ns; t->cap = nc;
    }
    t->s[t->n++] = str;
    return 0;
}
static void sstable_free(sstable *t) {
    for (size_t i = 0; i < t->n; i++) free(t->s[i]);
    free(t->s); t->s = NULL; t->n = t->cap = 0;
}

typedef enum { EXTRACT_DOCX, EXTRACT_PPTX, EXTRACT_XLSX } emode;

typedef struct {
    emode mode;
    sbuf out;            /* final document text */
    estr stack;          /* open element names */

    /* run accumulation: text collected but not yet flushed to a line */
    sbuf run;

    /* xlsx shared-string table build state */
    sstable *ss;         /* table used to RESOLVE shared-string refs (NULL until built) */
    int building;       /* 1 while parsing sharedStrings.xml (accumulate into ss) */
    sbuf si_run;         /* current <si> text accumulation */

    /* xlsx per-sheet cell accumulation */
    sbuf row;            /* current row's cells (TSV) */
    int   last_col;      /* last emitted 1-based column; for gap filling */
    int   in_cell;       /* inside a <c> element */
    int   cell_is_str;   /* current <c> is a shared-string ref (t="s") */
    int   cell_inline;   /* current <c> is inlineStr */
    int   have_col;      /* parsed a column from r="..." */
    int   cur_col;       /* 1-based column of current <c> */
} ex_state;

/* Parse the column letters out of a cell ref like "B2" / "AB12". Returns the
 * 1-based column number, or 0 if none could be read. */
static int col_from_ref(const char *r) {
    int col = 0;
    for (const char *p = r; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') col = col * 26 + (*p - 'A' + 1);
        else break;
    }
    return col;
}

static int emit_gap_and_cell(ex_state *st, int col, const char *text, size_t tlen) {
    /* ensure columns up to `col` are present, separating with tabs */
    while (st->last_col < col - 1) {
        if (sbuf_pushc(&st->row, '\t') != 0) return -1;
        st->last_col++;
    }
    if (st->last_col >= col - 1 && st->last_col < col) {
        if (sbuf_pushc(&st->row, '\t') != 0) return -1;
        st->last_col = col;
    }
    if (sbuf_push(&st->row, text, tlen) != 0) return -1;
    st->last_col = col;
    return 0;
}

static int on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    ex_state *st = (ex_state *)user;

    if (evt == WUBUXML_EVT_START) {
        const char *name = info->name;
        if (estr_push(&st->stack, name) != 0) return -1;

        if (st->mode == EXTRACT_XLSX && st->building) {
            /* building shared-string table: a fresh <si> starts a run */
            if (strcmp(name, "si") == 0) { st->si_run.len = 0; }
            return 0;
        }
        if (st->mode == EXTRACT_XLSX) {
            if (strcmp(name, "c") == 0) {
                st->in_cell = 1;
                st->cell_is_str = 0; st->cell_inline = 0;
                st->have_col = 0; st->cur_col = 0;
                for (int a = 0; a < info->attr_count; a++) {
                    if (strcmp(info->attr_name[a], "r") == 0)
                        { st->cur_col = col_from_ref(info->attr_val[a]); st->have_col = 1; }
                    else if (strcmp(info->attr_name[a], "t") == 0) {
                        if (strcmp(info->attr_val[a], "s") == 0) st->cell_is_str = 1;
                        else if (strcmp(info->attr_val[a], "inlineStr") == 0) st->cell_inline = 1;
                    }
                }
                if (!st->have_col) st->cur_col = st->last_col + 1;
            }
            return 0;
        }
        return 0;
    }

    if (evt == WUBUXML_EVT_TEXT) {
        if (st->mode == EXTRACT_XLSX && st->building) {
            /* accumulate into current <si> run */
            if (sbuf_push(&st->si_run, info->text, info->text_len) != 0) return -1;
            return 0;
        }
        if (st->mode == EXTRACT_XLSX && st->in_cell) {
            /* literal value (or inline text) for the current cell */
            char buf[64];
            const char *val = info->text; size_t vlen = info->text_len;
            if (st->cell_is_str && st->ss) {
                /* shared-string ref: <v>N</v> -> index */
                char *end = NULL;
                long idx = strtol(info->text, &end, 10);
                if (end != info->text && idx >= 0 && (size_t)idx < st->ss->n) {
                    val = st->ss->s[idx]; vlen = strlen(val);
                } else { buf[0] = '\0'; val = buf; vlen = 0; }
            }
            /* inlineStr: the <t> inside <is> is the literal text already */
            if (emit_gap_and_cell(st, st->cur_col, val, vlen) != 0) return -1;
            return 0;
        }
        /* docx/pptx: accumulate run text */
        if (sbuf_push(&st->run, info->text, info->text_len) != 0) return -1;
        return 0;
    }

    /* WUBUXML_EVT_END */
    const char *name = info->name;
    const char *top = estr_top(&st->stack);
    (void)top;

    if (st->mode == EXTRACT_XLSX && st->building) {
        if (strcmp(name, "si") == 0) {
            char *s = malloc(st->si_run.len + 1);
            if (!s) return -1;
            memcpy(s, st->si_run.p, st->si_run.len);
            s[st->si_run.len] = '\0';
            st->si_run.len = 0;
            if (sstable_add(st->ss, s) != 0) return -1;
        }
        estr_pop(&st->stack);
        return 0;
    }

    if (st->mode == EXTRACT_XLSX) {
        if (strcmp(name, "c") == 0) {
            st->in_cell = 0;
        } else if (strcmp(name, "row") == 0) {
            /* end of row: flush the row line */
            if (sbuf_pushc(&st->row, '\n') != 0) return -1;
            if (sbuf_push(&st->out, st->row.p, st->row.len) != 0) return -1;
            st->row.len = 0; st->last_col = 0;
        }
        estr_pop(&st->stack);
        return 0;
    }

    /* docx / pptx end-of-element handling */
    int is_para_end = (st->mode == EXTRACT_DOCX && strcmp(name, "w:p") == 0)
                   || (st->mode == EXTRACT_PPTX && strcmp(name, "a:p") == 0);
    int is_run = (st->mode == EXTRACT_DOCX && strcmp(name, "w:t") == 0)
              || (st->mode == EXTRACT_PPTX && strcmp(name, "a:t") == 0);

    if (is_run) {
        /* flush accumulated run text into the document */
        if (sbuf_push(&st->out, st->run.p, st->run.len) != 0) return -1;
        st->run.len = 0;
    } else if (is_para_end) {
        if (sbuf_pushc(&st->out, '\n') != 0) return -1;
    }
    estr_pop(&st->stack);
    return 0;
}

/* helper: run the SAX handler and return the accumulated out buffer.
 * `building` == 1 means `ss` is being populated from sharedStrings.xml;
 * `building` == 0 means `ss` (if non-NULL) is used to resolve shared-string
 * cell refs in a worksheet. */
static int run_extract(emode mode, const uint8_t *xml, size_t len,
                       sstable *ss, int building, char **out) {
    ex_state st;
    memset(&st, 0, sizeof st);
    st.mode = mode; st.ss = ss; st.building = building;
    int rc = wubuxml_parse(xml, len, on_event, &st);
    if (rc != 0) {
        sbuf_free(&st.out); sbuf_free(&st.run); sbuf_free(&st.row);
        sbuf_free(&st.si_run); estr_free(&st.stack);
        return -1;
    }
    /* ensure NUL-terminated even if empty */
    if (!st.out.p) { st.out.p = malloc(1); if (!st.out.p) { estr_free(&st.stack); return -1; } st.out.p[0] = '\0'; }
    *out = st.out.p;
    /* leave sub-buffers; they are owned only via st.out (run/row/si_run freed
     * below since their content has been copied into out) */
    sbuf_free(&st.run); sbuf_free(&st.row); sbuf_free(&st.si_run);
    estr_free(&st.stack);
    return 0;
}

int wubuoxml_docx_text(const uint8_t *xml, size_t len, char **out) {
    return run_extract(EXTRACT_DOCX, xml, len, NULL, 0, out);
}

int wubuoxml_pptx_text(const uint8_t *xml, size_t len, char **out) {
    return run_extract(EXTRACT_PPTX, xml, len, NULL, 0, out);
}

int wubuoxml_xlsx_text(const uint8_t *shared, size_t shared_len,
                       const wubuoxml_sheet *sheets, size_t nsheets, char **out) {
    sstable ss; memset(&ss, 0, sizeof ss);
    if (shared && shared_len) {
        /* build the shared-string table first */
        char *discard = NULL;
        if (run_extract(EXTRACT_XLSX, shared, shared_len, &ss, 1, &discard) != 0)
            return -1;
        free(discard);   /* run_extract may return an empty (1-byte) sentinel */
    }
    sbuf all; memset(&all, 0, sizeof all);
    int rc = 0;
    for (size_t i = 0; i < nsheets && rc == 0; i++) {
        char *sheet_txt = NULL;
        if (run_extract(EXTRACT_XLSX, sheets[i].bytes, sheets[i].len, nsheets ? &ss : NULL, 0, &sheet_txt) != 0) {
            rc = -1; break;
        }
        /* sheet header line */
        if (sbuf_pushc(&all, '[') != 0 || sbuf_push(&all, sheets[i].name, strlen(sheets[i].name)) != 0
            || sbuf_push(&all, "]\n", 2) != 0) { free(sheet_txt); rc = -1; break; }
        if (sheet_txt) {
            if (sbuf_push(&all, sheet_txt, strlen(sheet_txt)) != 0) { free(sheet_txt); rc = -1; break; }
            free(sheet_txt);
        }
    }
    sstable_free(&ss);
    if (rc != 0) { sbuf_free(&all); return -1; }
    if (!all.p) { all.p = malloc(1); all.p[0] = '\0'; }
    *out = all.p;
    return 0;
}
