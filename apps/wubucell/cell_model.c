/* WuBuOffice -- apps/wubucell/cell_model
 * Workbook/sheet/cell model + XML rendering for worksheets and charts.
 * Keeps the data model and its SpreadsheetML serialization together, separate
 * from formula evaluation (cell_eval.c) and package assembly (cell_io.c).
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "cell_internal.h"

sheet_t *cell_book_sheet(wubucell_book *b, int idx) {
    if (idx < 1 || (size_t)idx > b->n) return NULL;
    return &b->sheets[idx - 1];
}

wubucell_book *wubucell_create(void) { return calloc(1, sizeof(wubucell_book)); }

struct wubucell_style *wubucell_styles(wubucell_book *b) {
    if (!b->styles) b->styles = wubucell_style_create();
    return b->styles;
}

static sheet_t *book_sheet(wubucell_book *b, int idx) { return cell_book_sheet(b, idx); }

int wubucell_sheet(wubucell_book *b, const char *name) {
    if (b->n == b->cap) { b->cap = b->cap ? b->cap*2 : 4; b->sheets = realloc(b->sheets, b->cap*sizeof(*b->sheets)); }
    sheet_t *s = &b->sheets[b->n];
    memset(s, 0, sizeof *s);
    s->name = strdup(name);
    b->n++;
    return (int)b->n;
}

void wubucell_use_shared_strings(wubucell_book *b, int enable) { b->use_sst = enable; }

static void set_cell(wubucell_book *b, int sheet, int col, int row, cell_kind kind,
                     double num, const char *text, const char *formula, double cached, int style) {
    sheet_t *s = book_sheet(b, sheet); if (!s) return;
    if (s->n == s->cap) { s->cap = s->cap ? s->cap*2 : 16; s->cells = realloc(s->cells, s->cap*sizeof(*s->cells)); }
    cell_t *c = &s->cells[s->n++];
    c->col = col; c->row = row; c->kind = kind; c->num = num;
    c->text = text ? strdup(text) : NULL;
    c->formula = formula ? strdup(formula) : NULL;
    c->cached = cached;
    c->style = (style < 0) ? 0 : style;
}

void wubucell_cell_s(wubucell_book *b, int sheet, int col, int row, const char *text) {
    set_cell(b, sheet, col, row, C_STR, 0, text, NULL, 0, 0);
}
void wubucell_cell_sx(wubucell_book *b, int sheet, int col, int row, const char *text, int style) {
    set_cell(b, sheet, col, row, C_STR, 0, text, NULL, 0, style);
}
void wubucell_cell_n(wubucell_book *b, int sheet, int col, int row, double num) {
    set_cell(b, sheet, col, row, C_NUM, num, NULL, NULL, 0, 0);
}
void wubucell_cell_nx(wubucell_book *b, int sheet, int col, int row, double num, int style) {
    set_cell(b, sheet, col, row, C_NUM, num, NULL, NULL, 0, style);
}
void wubucell_cell_f(wubucell_book *b, int sheet, int col, int row, const char *formula, double cached) {
    set_cell(b, sheet, col, row, C_FORM, cached, NULL, formula, cached, 0);
}
void wubucell_cell_fx(wubucell_book *b, int sheet, int col, int row, const char *formula, double cached, int style) {
    set_cell(b, sheet, col, row, C_FORM, cached, NULL, formula, cached, style);
}

int wubucell_chart(wubucell_book *b, int sheet, const char *title, const char *cats, const char *vals) {
    if (b->ncharts == b->capc) { b->capc = b->capc ? b->capc*2 : 4; b->charts = realloc(b->charts, b->capc*sizeof(*b->charts)); }
    chart_t *c = &b->charts[b->ncharts++];
    c->sheet = sheet;
    c->title = title ? strdup(title) : NULL;
    c->cats = cats ? strdup(cats) : NULL;
    c->vals = vals ? strdup(vals) : NULL;
    return (int)b->ncharts;
}

void cell_col_letter(int col, char *out) {
    int n = col; char tmp[8]; int k = 0;
    while (n > 0) { int r = (n - 1) % 26; tmp[k++] = (char)('A' + r); n = (n - 1) / 26; }
    for (int i = 0; i < k; i++) out[i] = tmp[k - 1 - i];
    out[k] = '\0';
}

void cell_xml_escape(FILE *m, const char *t) {
    for (; t && *t; t++) {
        switch (*t) { case '&': fputs("&amp;", m); break; case '<': fputs("&lt;", m); break;
                      case '>': fputs("&gt;", m); break; default: fputc(*t, m); }
    }
}

int cell_sst_add(sst_t *t, const char *s) {
    for (size_t i = 0; i < t->n; i++) if (strcmp(t->e[i].s, s) == 0) return t->e[i].idx;
    if (t->n == t->cap) { t->cap = t->cap ? t->cap*2 : 16; t->e = realloc(t->e, t->cap*sizeof(*t->e)); }
    t->e[t->n].s = strdup(s); t->e[t->n].idx = (int)t->n;
    return (int)t->n++;
}

char *cell_render_sheet(const wubucell_book *b, const sheet_t *s, size_t sheet_idx, sst_t *sst) {
    char *out = NULL; size_t n = 0; FILE *m = open_memstream(&out, &n);
    fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    fprintf(m, "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n");
    for (size_t ci = 0; ci < b->ncharts; ci++) {
        if (b->charts[ci].sheet != (int)(sheet_idx + 1)) continue;
        size_t which = 0;
        for (size_t k = 0; k < ci; k++) if (b->charts[k].sheet == (int)(sheet_idx + 1)) which++;
        fprintf(m, "<drawing r:id=\"rId%d\"/>\n", 1 + (int)which);
    }
    fprintf(m, "<sheetData>\n");
    int maxrow = 0;
    for (size_t i = 0; i < s->n; i++) if (s->cells[i].row > maxrow) maxrow = s->cells[i].row;
    for (int r = 1; r <= maxrow; r++) {
        fprintf(m, "<row r=\"%d\">\n", r);
        for (size_t i = 0; i < s->n; i++) {
            const cell_t *c = &s->cells[i];
            if (c->row != r) continue;
            char cl[16]; cell_col_letter(c->col, cl);
            char ref[32]; snprintf(ref, sizeof ref, "%s%d", cl, r);
            /* Cell type attribute rules (OOXML SpreadsheetML):
             *  - shared-string cells MUST carry t="s" so the <v> index is read
             *    as a shared-string reference (not a literal number).
             *  - inline-string cells (default non-shared mode) MUST carry
             *    t="inlineStr"; without it the cell defaults to t="n" (number)
             *    and conformant readers (openpyxl, Excel) drop the <is> text.
             *  - numbers and formulas use the default t="n" (attribute omitted).
             * The style attribute (s=) is independent and may co-occur. */
            if (c->kind == C_STR && b->use_sst) {
                if (c->style) fprintf(m, "<c r=\"%s\" s=\"%d\" t=\"s\">", ref, c->style);
                else fprintf(m, "<c r=\"%s\" t=\"s\">", ref);
            } else if (c->kind == C_STR) { /* inline string (default mode) */
                if (c->style) fprintf(m, "<c r=\"%s\" s=\"%d\" t=\"inlineStr\">", ref, c->style);
                else fprintf(m, "<c r=\"%s\" t=\"inlineStr\">", ref);
            } else { /* number or formula: default numeric type */
                if (c->style) fprintf(m, "<c r=\"%s\" s=\"%d\">", ref, c->style);
                else fprintf(m, "<c r=\"%s\">", ref);
            }
            if (c->kind == C_NUM) fprintf(m, "<v>%.12g</v>", c->num);
            else if (c->kind == C_FORM) {
                fprintf(m, "<f>%s</f>", c->formula ? c->formula : "");
                if (c->text) { fprintf(m, "<v>"); cell_xml_escape(m, c->text); fprintf(m, "</v>"); }
                else { fprintf(m, "<v>%.12g</v>", c->cached); }
            }
            else if (b->use_sst) { int si = cell_sst_add(sst, c->text ? c->text : ""); fprintf(m, "<v>%d</v>", si); }
            else { fprintf(m, "<is><t xml:space=\"preserve\">"); cell_xml_escape(m, c->text); fprintf(m, "</t></is>"); }
            fprintf(m, "</c>\n");
        }
        fprintf(m, "</row>\n");
    }
    fprintf(m, "</sheetData>\n</worksheet>\n");
    fflush(m); fclose(m);
    return out;
}

char *cell_render_chart(const chart_t *c, size_t idx) {
    char *out = NULL; size_t n = 0; FILE *m = open_memstream(&out, &n);
    fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    fprintf(m, "<c:chartSpace xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
                 "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n");
    fprintf(m, "<c:chart><c:title><c:tx><c:rich><a:bodyPr/><a:lstStyle/>"
                 "<a:p><a:r><a:t>%s</a:t></a:r></a:p></c:rich></c:tx><c:overlay val=\"0\"/></c:title>\n",
            c->title ? c->title : "Chart");
    fprintf(m, "<c:plotArea><c:barChart><c:barDir val=\"col\"/>\n");
    fprintf(m, "<c:ser><c:idx val=\"0\"/><c:order val=\"0\"/>\n");
    if (c->cats) fprintf(m, "<c:cat><c:strRef><c:f>%s</c:f></c:strRef></c:cat>\n", c->cats);
    if (c->vals) fprintf(m, "<c:val><c:numRef><c:f>%s</c:f></c:numRef></c:val>\n", c->vals);
    fprintf(m, "</c:ser></c:barChart></c:plotArea>\n");
    fprintf(m, "<c:legend><c:legendPos val=\"b\"/></c:legend>\n");
    fprintf(m, "</c:chart></c:chartSpace>\n");
    (void)idx;
    fflush(m); fclose(m);
    return out;
}

/* --- read-back accessors (public API in cell.h) --- */

int wubucell_sheet_count(const wubucell_book *b) {
    return (int)(b ? b->n : 0);
}

const char *wubucell_sheet_name(const wubucell_book *b, int sheet) {
    if (!b || sheet < 1 || (size_t)sheet > b->n) return NULL;
    return b->sheets[sheet - 1].name;
}

int wubucell_sheet_dims(const wubucell_book *b, int sheet, int *max_col, int *max_row) {
    if (!b || sheet < 1 || (size_t)sheet > b->n) return -1;
    const sheet_t *s = &b->sheets[sheet - 1];
    int mc = 0, mr = 0;
    for (size_t i = 0; i < s->n; i++) {
        if (s->cells[i].col > mc) mc = s->cells[i].col;
        if (s->cells[i].row > mr) mr = s->cells[i].row;
    }
    if (max_col) *max_col = mc;
    if (max_row) *max_row = mr;
    return 0;
}

int wubucell_get(const wubucell_book *b, int sheet, int col, int row,
                 wubucell_ckind *kind, const char **text, double *num, double *cached) {
    if (!b) return -1;
    const sheet_t *s = cell_book_sheet((wubucell_book *)b, sheet);
    if (!s) return -1;
    for (size_t i = 0; i < s->n; i++) {
        const cell_t *c = &s->cells[i];
        if (c->col == col && c->row == row) {
            if (kind) {
                if (c->kind == C_STR) *kind = WUBUCELL_STR;
                else if (c->kind == C_NUM) *kind = WUBUCELL_NUM;
                else *kind = WUBUCELL_FORM;
            }
            /* For a formula cell, expose the formula string (the numeric result
             * is available separately via `cached`); for str/num expose text. */
            if (text) *text = (c->kind == C_FORM && c->formula) ? c->formula : c->text;
            if (num) *num = c->num;
            if (cached) *cached = c->cached;
            return 0;
        }
    }
    return -1;   /* no cell at that slot */
}
