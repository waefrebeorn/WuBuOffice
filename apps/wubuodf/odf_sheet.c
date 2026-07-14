/* odf_sheet.c -- OpenDocument Spreadsheet (.ods) writer + reader. See odf.h.
 * Clean-room C11. Model: wubucell_book. */

#include "odf.h"
#include "../../src/wubuxml/parser.h"
#include "../../src/wubuoxml/reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- growable buffer + escaping ---- */
typedef struct { char *s; size_t n, cap; } sbuf;
static void sb_putn(sbuf *b, const char *p, size_t n) {
    if (b->n + n + 1 > b->cap) { while (b->n + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 1024; b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->n, p, n); b->n += n; b->s[b->n] = '\0';
}
static void sb_puts(sbuf *b, const char *s) { sb_putn(b, s, strlen(s)); }
static void sb_esc(sbuf *b, const char *s) {
    for (const char *p = s ? s : ""; *p; p++) {
        switch (*p) {
            case '&': sb_puts(b, "&amp;"); break;
            case '<': sb_puts(b, "&lt;"); break;
            case '>': sb_puts(b, "&gt;"); break;
            default: sb_putn(b, p, 1);
        }
    }
}

/* ---- writer ---- */

int wubuodf_write_ods(const wubucell_book *bk, const char *path) {
    if (!bk) return -1;
    sbuf b = {0};
    sb_puts(&b,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<office:document-content "
        "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
        "xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" "
        "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
        "office:version=\"1.3\">\n"
        "<office:body><office:spreadsheet>\n");

    int ns = wubucell_sheet_count(bk);
    for (int s = 1; s <= ns; s++) {
        sb_puts(&b, "<table:table table:name=\"");
        sb_esc(&b, wubucell_sheet_name(bk, s));
        sb_puts(&b, "\">\n");
        int mc = 0, mr = 0; wubucell_sheet_dims(bk, s, &mc, &mr);
        char col[96];
        snprintf(col, sizeof col, "<table:table-column table:number-columns-repeated=\"%d\"/>\n", mc > 0 ? mc : 1);
        sb_puts(&b, col);
        for (int r = 1; r <= mr; r++) {
            sb_puts(&b, "<table:table-row>\n");
            for (int c = 1; c <= mc; c++) {
                wubucell_ckind k; const char *t = NULL; double num = 0, cached = 0;
                if (wubucell_get(bk, s, c, r, &k, &t, &num, &cached) != 0) {
                    sb_puts(&b, "<table:table-cell/>\n");
                    continue;
                }
                char buf[128];
                if (k == WUBUCELL_STR) {
                    sb_puts(&b, "<table:table-cell office:value-type=\"string\"><text:p>");
                    sb_esc(&b, t ? t : "");
                    sb_puts(&b, "</text:p></table:table-cell>\n");
                } else if (k == WUBUCELL_NUM) {
                    snprintf(buf, sizeof buf, "<table:table-cell office:value-type=\"float\" office:value=\"%g\"><text:p>%g</text:p></table:table-cell>\n", num, num);
                    sb_puts(&b, buf);
                } else { /* formula: ODF prefixes with "of:=" */
                    sb_puts(&b, "<table:table-cell office:value-type=\"float\" table:formula=\"of:=");
                    sb_esc(&b, t ? t : "");
                    snprintf(buf, sizeof buf, "\" office:value=\"%g\"><text:p>%g</text:p></table:table-cell>\n", cached, cached);
                    sb_puts(&b, buf);
                }
            }
            sb_puts(&b, "</table:table-row>\n");
        }
        sb_puts(&b, "</table:table>\n");
    }

    sb_puts(&b, "</office:spreadsheet></office:body>\n</office:document-content>\n");
    int rc = wubuodf_assemble(path, "application/vnd.oasis.opendocument.spreadsheet", b.s, b.n);
    free(b.s);
    return rc;
}

/* ---- reader ---- */

typedef struct {
    wubucell_book *book;
    int sheet;             /* 1-based current sheet */
    int row, col;
    int repeat;            /* number-columns-repeated for current cell */
    int in_cell, cell_is_num, cell_is_formula;
    double cell_val;
    char *formula;
    char *txt; size_t tn, tcap;
} ods_state;

static void acc(char **buf, size_t *n, size_t *cap, const char *s, size_t len) {
    if (*n + len + 1 > *cap) { while (*n + len + 1 > *cap) *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap); }
    memcpy(*buf + *n, s, len); *n += len; (*buf)[*n] = '\0';
}

static int ods_ev(wubuxml_event evt, const wubuxml_info *info, void *user) {
    ods_state *st = (ods_state *)user;
    const char *name = info->name;

    if (evt == WUBUXML_EVT_START) {
        if (strcmp(name, "table:table") == 0) {
            const char *nm = "Sheet";
            for (int a = 0; a < info->attr_count; a++)
                if (strcmp(info->attr_name[a], "table:name") == 0) nm = info->attr_val[a];
            st->sheet = wubucell_sheet(st->book, nm);
            st->row = 0;
        } else if (strcmp(name, "table:table-row") == 0) {
            st->row++; st->col = 0;
        } else if (strcmp(name, "table:table-cell") == 0 || strcmp(name, "table:covered-table-cell") == 0) {
            st->in_cell = 1; st->cell_is_num = 0; st->cell_is_formula = 0; st->cell_val = 0;
            st->repeat = 1; st->tn = 0; if (st->txt) st->txt[0] = '\0';
            free(st->formula); st->formula = NULL;
            for (int a = 0; a < info->attr_count; a++) {
                const char *an = info->attr_name[a], *av = info->attr_val[a];
                if (strcmp(an, "office:value-type") == 0 && strcmp(av, "float") == 0) st->cell_is_num = 1;
                else if (strcmp(an, "office:value") == 0) st->cell_val = strtod(av, NULL);
                else if (strcmp(an, "table:number-columns-repeated") == 0) st->repeat = atoi(av);
                else if (strcmp(an, "table:formula") == 0) {
                    st->cell_is_formula = 1;
                    const char *fp = av;
                    if (strncmp(fp, "of:=", 4) == 0) fp += 4; else if (fp[0] == '=') fp += 1;
                    st->formula = strdup(fp);
                }
            }
        }
        return 0;
    }

    if (evt == WUBUXML_EVT_TEXT) {
        if (st->in_cell) acc(&st->txt, &st->tn, &st->tcap, info->text, info->text_len);
        return 0;
    }

    if (strcmp(name, "table:table-cell") == 0 || strcmp(name, "table:covered-table-cell") == 0) {
        /* commit cell(s): honor repeat only when the cell has content */
        int has_content = (st->cell_is_formula || st->cell_is_num || (st->txt && st->txt[0]));
        int reps = st->repeat < 1 ? 1 : st->repeat;
        /* avoid runaway trailing empty repeats (ODF pads rows heavily) */
        if (!has_content) reps = 1;
        for (int k = 0; k < reps; k++) {
            st->col++;
            if (st->cell_is_formula) {
                wubucell_cell_f(st->book, st->sheet, st->col, st->row, st->formula ? st->formula : "", st->cell_val);
            } else if (st->cell_is_num) {
                wubucell_cell_n(st->book, st->sheet, st->col, st->row, st->cell_val);
            } else if (st->txt && st->txt[0]) {
                wubucell_cell_s(st->book, st->sheet, st->col, st->row, st->txt);
            }
        }
        st->in_cell = 0;
        free(st->formula); st->formula = NULL;
    }
    return 0;
}

int wubuodf_read_ods(const char *path, wubucell_book **out) {
    if (!out) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(sz ? (size_t)sz : 1);
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return -1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(data, (size_t)sz, &pkg) != 0) { free(data); return -1; }
    const wubuoxml_part *content = wubuoxml_part_find(&pkg, "content.xml");
    int rc = -1;
    wubucell_book *bk = wubucell_create();
    if (content && bk) {
        ods_state st; memset(&st, 0, sizeof st);
        st.book = bk;
        rc = wubuxml_parse(content->bytes, content->len, ods_ev, &st);
        free(st.txt); free(st.formula);
    }
    wubuoxml_free(&pkg);
    free(data);
    if (rc == 0) *out = bk; else wubucell_free(bk);
    return rc;
}
