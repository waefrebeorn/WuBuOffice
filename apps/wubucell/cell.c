#include "cell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    int col, row;
    int is_num;
    double num;
    char *text;
} cell_t;

typedef struct {
    char *name;
    cell_t *cells; size_t n, cap;
} sheet_t;

struct wubucell_book {
    sheet_t *sheets; size_t n, cap;
};

wubucell_book *wubucell_create(void) { return calloc(1, sizeof(wubucell_book)); }

static sheet_t *book_sheet(wubucell_book *b, int idx) {
    if (idx < 1 || (size_t)idx > b->n) return NULL;
    return &b->sheets[idx - 1];
}

int wubucell_sheet(wubucell_book *b, const char *name) {
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 4;
        b->sheets = realloc(b->sheets, b->cap * sizeof(*b->sheets));
    }
    sheet_t *s = &b->sheets[b->n];
    memset(s, 0, sizeof *s);
    s->name = strdup(name);
    b->n++;
    return (int)b->n;
}

static void set_cell(sheet_t *s, int col, int row, int is_num, double num, const char *text) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->cells = realloc(s->cells, s->cap * sizeof(*s->cells));
    }
    cell_t *c = &s->cells[s->n++];
    c->col = col; c->row = row; c->is_num = is_num; c->num = num;
    c->text = text ? strdup(text) : NULL;
}

void wubucell_cell_s(wubucell_book *b, int sheet, int col, int row, const char *text) {
    sheet_t *s = book_sheet(b, sheet); if (!s) return;
    set_cell(s, col, row, 0, 0, text);
}
void wubucell_cell_n(wubucell_book *b, int sheet, int col, int row, double num) {
    sheet_t *s = book_sheet(b, sheet); if (!s) return;
    set_cell(s, col, row, 1, num, NULL);
}

static void col_letter(int col, char *out) {
    /* 1->A, 26->Z, 27->AA */
    int n = col; char tmp[8]; int k = 0;
    while (n > 0) { int r = (n - 1) % 26; tmp[k++] = (char)('A' + r); n = (n - 1) / 26; }
    for (int i = 0; i < k; i++) out[i] = tmp[k - 1 - i];
    out[k] = '\0';
}

static char *render_sheet(const sheet_t *s) {
    char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
    fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    fprintf(m, "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n");
    fprintf(m, "<sheetData>\n");
    /* group cells by row */
    int maxrow = 0;
    for (size_t i = 0; i < s->n; i++) if (s->cells[i].row > maxrow) maxrow = s->cells[i].row;
    for (int r = 1; r <= maxrow; r++) {
        fprintf(m, "<row r=\"%d\">\n", r);
        for (size_t i = 0; i < s->n; i++) {
            const cell_t *c = &s->cells[i];
            if (c->row != r) continue;
            char cl[16]; col_letter(c->col, cl);
            char ref[32]; snprintf(ref, sizeof ref, "%s%d", cl, r);
            if (c->is_num) fprintf(m, "<c r=\"%s\"><v>%.10g</v></c>\n", ref, c->num);
            else {
                fprintf(m, "<c r=\"%s\" t=\"inlineStr\"><is><t xml:space=\"preserve\">", ref);
                /* minimal escape */
                const char *t = c->text;
                for (; t && *t; t++) {
                    switch (*t) { case '&': fputs("&amp;", m); break; case '<': fputs("&lt;", m); break; case '>': fputs("&gt;", m); break; default: fputc(*t, m); }
                }
                fprintf(m, "</t></is></c>\n");
            }
        }
        fprintf(m, "</row>\n");
    }
    fprintf(m, "</sheetData>\n");
    fprintf(m, "</worksheet>\n");
    fflush(m); fclose(m);
    return b;
}

int wubucell_assemble(wubucell_book *b, const char *outpath) {
    FILE *out = fopen(outpath, "wb");
    if (!out) { perror("fopen"); return -1; }
    wubuoxml_package *pkg = wubuoxml_create(out);
    wubuoxml_add_default_type(pkg, "rels", "application/vnd.openxmlformats-package.relationships+xml");
    wubuoxml_add_default_type(pkg, "xml", "application/xml");
    wubuoxml_add_override(pkg, "/xl/workbook.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");
    for (size_t i = 0; i < b->n; i++) {
        char path[64];
        snprintf(path, sizeof path, "/xl/worksheets/sheet%d.xml", (int)(i + 1));
        wubuoxml_add_override(pkg, path, "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
    }
    wubuoxml_add_relationship(pkg, "", "xl/workbook.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");

    /* workbook.xml */
    {
        char *bb = NULL; size_t bn = 0; FILE *m = open_memstream(&bb, &bn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n");
        fprintf(m, "<sheets>\n");
        for (size_t i = 0; i < b->n; i++)
            fprintf(m, "  <sheet name=\"%s\" sheetId=\"%zu\" r:id=\"rId%zu\"/>\n",
                    b->sheets[i].name, i + 1, i + 1);
        fprintf(m, "</sheets>\n");
        fprintf(m, "</workbook>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "xl/workbook.xml", bb, bn);
        free(bb);
    }
    /* xl/_rels/workbook.xml.rels : one worksheet relationship per sheet */
    {
        char *sp = NULL; size_t sn = 0; FILE *m = open_memstream(&sp, &sn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
        for (size_t i = 0; i < b->n; i++)
            fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%zu.xml\"/>\n",
                    i + 1, i + 1);
        fprintf(m, "</Relationships>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "xl/_rels/workbook.xml.rels", sp, sn);
        free(sp);
    }
    /* worksheets */
    for (size_t i = 0; i < b->n; i++) {
        char *sxml = render_sheet(&b->sheets[i]);
        char path[64];
        snprintf(path, sizeof path, "xl/worksheets/sheet%d.xml", (int)(i + 1));
        wubuoxml_add_part(pkg, path, sxml, strlen(sxml));
        free(sxml);
    }
    int rc = wubuoxml_finalize(pkg);
    fclose(out);
    return rc;
}

void wubucell_free(wubucell_book *b) {
    if (!b) return;
    for (size_t i = 0; i < b->n; i++) {
        free(b->sheets[i].name);
        for (size_t j = 0; j < b->sheets[i].n; j++) free(b->sheets[i].cells[j].text);
        free(b->sheets[i].cells);
    }
    free(b->sheets);
    free(b);
}
