/* odf_sheet.c -- OpenDocument Spreadsheet (.ods) writer + reader. See odf.h.
 * Clean-room C11. Model: wubucell_book. Body XML from shared odf_body. */

#include "odf.h"
#include "odf_body.h"
#include "../../src/wubuxml/parser.h"
#include "../../src/wubuoxml/reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- writer ---- */

int wubuodf_write_ods(const wubucell_book *bk, const char *path) {
    if (!bk) return -1;
    odf_sbuf b = {0};
    odf_sb_puts(&b,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<office:document-content ");
    odf_sb_puts(&b, WUBUODF_NS_ALL);
    odf_sb_puts(&b, " office:version=\"1.3\">\n");
    wubuodf_emit_sheet_body(&b, bk);
    odf_sb_puts(&b, "</office:document-content>\n");
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
    if (content && bk) rc = wubuodf_parse_sheet_xml(content->bytes, content->len, bk);
    wubuoxml_free(&pkg);
    free(data);
    if (rc == 0) *out = bk; else wubucell_free(bk);
    return rc;
}

/* Shared XML-bytes entry: run the ODS SAX handler over `bytes` into `bk`.
 * Used by the packaged reader (content.xml) and the flat .fods reader. */
int wubuodf_parse_sheet_xml(const uint8_t *bytes, size_t len, wubucell_book *bk) {
    if (!bk || !bytes) return -1;
    ods_state st; memset(&st, 0, sizeof st);
    st.book = bk;
    int rc = wubuxml_parse(bytes, len, ods_ev, &st);
    free(st.txt); free(st.formula);
    return rc;
}
