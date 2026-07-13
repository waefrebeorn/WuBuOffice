#include "cell.h"
#include "style.h"
#include "value.h"
#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

typedef enum { C_STR, C_NUM, C_FORM } cell_kind;

typedef struct {
    int col, row;
    cell_kind kind;
    double num;
    double cached;      /* numeric result of a formula */
    char *text;         /* for C_STR: the value; for C_FORM: the evaluated result */
    char *formula;      /* for C_FORM: the original formula (without '=') */
    int style;
} cell_t;

typedef struct {
    char *name;
    cell_t *cells; size_t n, cap;
} sheet_t;

typedef struct {
    int sheet;
    char *title;
    char *cats;
    char *vals;
} chart_t;

struct wubucell_book {
    sheet_t *sheets; size_t n, cap;
    int use_sst;
    struct wubucell_style *styles;
    chart_t *charts; size_t ncharts, capc;
};

wubucell_book *wubucell_create(void) { return calloc(1, sizeof(wubucell_book)); }

struct wubucell_style *wubucell_styles(wubucell_book *b) {
    if (!b->styles) b->styles = wubucell_style_create();
    return b->styles;
}

static sheet_t *book_sheet(wubucell_book *b, int idx) {
    if (idx < 1 || (size_t)idx > b->n) return NULL;
    return &b->sheets[idx - 1];
}

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

static void col_letter(int col, char *out) {
    int n = col; char tmp[8]; int k = 0;
    while (n > 0) { int r = (n - 1) % 26; tmp[k++] = (char)('A' + r); n = (n - 1) / 26; }
    for (int i = 0; i < k; i++) out[i] = tmp[k - 1 - i];
    out[k] = '\0';
}

typedef struct { char *s; int idx; } sst_ent;
typedef struct { sst_ent *e; size_t n, cap; } sst_t;

static int sst_add(sst_t *t, const char *s) {
    for (size_t i = 0; i < t->n; i++) if (strcmp(t->e[i].s, s) == 0) return t->e[i].idx;
    if (t->n == t->cap) { t->cap = t->cap ? t->cap*2 : 16; t->e = realloc(t->e, t->cap*sizeof(*t->e)); }
    t->e[t->n].s = strdup(s); t->e[t->n].idx = (int)t->n;
    return (int)t->n++;
}

static void xml_escape(FILE *m, const char *t) {
    for (; t && *t; t++) {
        switch (*t) { case '&': fputs("&amp;", m); break; case '<': fputs("&lt;", m); break;
                      case '>': fputs("&gt;", m); break; default: fputc(*t, m); }
    }
}

static char *render_sheet(const wubucell_book *b, const sheet_t *s, size_t sheet_idx, sst_t *sst) {
    char *out = NULL; size_t n = 0; FILE *m = open_memstream(&out, &n);
    fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    fprintf(m, "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n");
    /* draw a chart on this sheet if one is rooted here */
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
            char cl[16]; col_letter(c->col, cl);
            char ref[32]; snprintf(ref, sizeof ref, "%s%d", cl, r);
            if (c->style) fprintf(m, "<c r=\"%s\" s=\"%d\">", ref, c->style);
            else fprintf(m, "<c r=\"%s\">", ref);
            if (c->kind == C_NUM) fprintf(m, "<v>%.12g</v>", c->num);
            else if (c->kind == C_FORM) {
                /* formula cell: keep the <f> element (original formula) and
                 * render the cached result. Numeric results go in <v> as a
                 * number; text/error results go as inline string <v>. */
                fprintf(m, "<f>%s</f>", c->formula ? c->formula : "");
                if (c->text) { /* string or error result */
                    fprintf(m, "<v>");
                    xml_escape(m, c->text);
                    fprintf(m, "</v>");
                } else {
                    fprintf(m, "<v>%.12g</v>", c->cached);
                }
            }
            else if (b->use_sst) { int si = sst_add(sst, c->text ? c->text : ""); fprintf(m, "<v>%d</v>", si); }
            else { fprintf(m, "<is><t xml:space=\"preserve\">"); xml_escape(m, c->text); fprintf(m, "</t></is>"); }
            fprintf(m, "</c>\n");
        }
        fprintf(m, "</row>\n");
    }
    fprintf(m, "</sheetData>\n</worksheet>\n");
    fflush(m); fclose(m);
    return out;
}

/* Render a chart XML part (barChart) for a chart definition. */
static char *render_chart(const chart_t *c, size_t idx) {
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

/* ---- formula engine integration ----
 * The wubucell_book is the resolver context. Each formula cell is evaluated
 * (recursively, with circular-reference detection) and its result cached back
 * into the cell so the worksheet can write a real <v>. */

typedef struct {
    wubucell_book *book;
    int cur_sheet;   /* 0-based sheet the current formula lives in */
    int *visit;      /* per-global-formula-index state: 0/1/2 */
} book_resolver;

static int strcasecmp_local(const char *a, const char *b) {
    while (*a && *b) { int ca=tolower((unsigned char)*a),cb=tolower((unsigned char)*b); if(ca!=cb)return ca-cb; a++; b++; }
    return tolower((unsigned char)*a)-tolower((unsigned char)*b);
}

/* Resolve a cell ref to a value. Recurses into formula cells through the same
 * resolver; cycle detection uses R->visit keyed by a global formula index. */
static int br_resolve(void *ctx, const wubucell_ref *ref, wubuval *out) {
    book_resolver *R = (book_resolver *)ctx;
    wubucell_book *b = R->book;
    int sheet = (ref->sheet == -1) ? R->cur_sheet : -1;
    if (ref->sheet == -2) {
        for (size_t i = 0; i < b->n; i++)
            if (strcasecmp_local(b->sheets[i].name, ref->sheet_name) == 0) { sheet = (int)i; break; }
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
        if (c->kind == C_STR) { wubuval_set_str(out, c->text ? c->text : ""); return 0; }
        if (c->kind == C_FORM) {
            /* find this formula's global index and evaluate it (handles cycles) */
            int fi = 0, found = -1;
            for (size_t sh = 0; sh < b->n && found < 0; sh++)
                for (size_t j = 0; j < b->sheets[sh].n; j++)
                    if (b->sheets[sh].cells[j].kind == C_FORM) {
                        if ((int)sh == sheet && b->sheets[sh].cells[j].col == c->col && b->sheets[sh].cells[j].row == c->row) { found = fi; break; }
                        fi++;
                    }
            if (found >= 0) {
                /* evaluate via the shared context; we need fsheet/fcol/frow.
                 * Rebuild on the fly is wasteful; instead resolve directly. */
                wubuval v; memset(&v, 0, sizeof v);
                /* guard against recursion: if visiting, return CYCLE */
                /* (visit state is in R for the top-level loop; for nested we
                 * re-resolve by calling evaluate on the same cell) */
                int *fsheet2=NULL,*fcol2=NULL,*frow2=NULL; int nf2=0;
                for (size_t sh2=0; sh2<b->n; sh2++)
                    for (size_t j2=0; j2<b->sheets[sh2].n; j2++)
                        if (b->sheets[sh2].cells[j2].kind==C_FORM) nf2++;
                if (nf2) {
                    fsheet2=malloc(sizeof(int)*nf2); fcol2=malloc(sizeof(int)*nf2); frow2=malloc(sizeof(int)*nf2);
                    int f2=0;
                    for (size_t sh2=0; sh2<b->n; sh2++)
                        for (size_t j2=0; j2<b->sheets[sh2].n; j2++)
                            if (b->sheets[sh2].cells[j2].kind==C_FORM) { fsheet2[f2]=(int)sh2; fcol2[f2]=b->sheets[sh2].cells[j2].col; frow2[f2]=b->sheets[sh2].cells[j2].row; f2++; }
                    /* mark visiting for `found` to detect cycles */
                    R->visit[found] = 1;
                    wubuval v2; memset(&v2,0,sizeof v2);
                    wubu_formula_eval(c->formula?c->formula:"", br_resolve, R, &v2);
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

/* Evaluate every formula cell in the book, caching results back into cells.
 * Called once at assemble time. */
static void wubucell_eval_all(wubucell_book *b) {
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
                fsheet[fi]=(int)sh; fcol[fi]=b->sheets[sh].cells[i].col; frow[fi]=b->sheets[sh].cells[i].row; fi++;
            }
    int *visit = calloc((size_t)nf, sizeof(int));
    book_resolver R; memset(&R, 0, sizeof R); R.book = b; R.visit = visit;
    for (int k = 0; k < nf; k++) {
        if (visit[k] == 2) continue;
        /* top-level: evaluate this formula cell */
        R.cur_sheet = fsheet[k];
        sheet_t *s = &b->sheets[fsheet[k]];
        cell_t *cell = NULL;
        for (size_t i = 0; i < s->n; i++)
            if (s->cells[i].col == fcol[k] && s->cells[i].row == frow[k]) { cell = &s->cells[i]; break; }
        if (!cell) continue;
        if (visit[k] == 1) { /* self/cycle at top: mark error */ free(cell->text); cell->text = strdup("#CYCLE!"); cell->kind = C_STR; visit[k]=2; continue; }
        visit[k] = 1;
        wubuval v; memset(&v, 0, sizeof v);
        wubu_formula_eval(cell->formula ? cell->formula : "", br_resolve, &R, &v);
        visit[k] = 2;
        if (v.kind == WV_NUM) { cell->cached = v.num; free(cell->text); cell->text = NULL; }
        else if (v.kind == WV_BOOL) { cell->cached = v.boolean ? 1.0 : 0.0; free(cell->text); cell->text = NULL; }
        else if (v.kind == WV_STR) { free(cell->text); cell->text = strdup(v.str ? v.str : ""); }
        else if (v.kind == WV_ERR) { free(cell->text); cell->text = strdup(wubuval_err_text(v.err)); }
        else { cell->cached = 0; free(cell->text); cell->text = NULL; }
        wubuval_free(&v);
    }
    free(fsheet); free(fcol); free(frow); free(visit);
}

int wubucell_assemble(wubucell_book *b, const char *outpath) {
    wubucell_eval_all(b);   /* backbone: compute all formula results first */
    FILE *out = fopen(outpath, "wb");
    if (!out) { perror("fopen"); return -1; }
    wubuoxml_package *pkg = wubuoxml_create(out);
    wubuoxml_add_default_type(pkg, "rels", "application/vnd.openxmlformats-package.relationships+xml");
    wubuoxml_add_default_type(pkg, "xml", "application/xml");

    int has_charts = (b->ncharts > 0);
    wubuoxml_add_override(pkg, "/xl/workbook.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");
    wubuoxml_add_override(pkg, "/xl/styles.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml");
    for (size_t i = 0; i < b->n; i++) {
        char path[64];
        snprintf(path, sizeof path, "/xl/worksheets/sheet%d.xml", (int)(i + 1));
        wubuoxml_add_override(pkg, path, "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
    }
    if (b->use_sst)
        wubuoxml_add_override(pkg, "/xl/sharedStrings.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml");
    if (has_charts) {
        wubuoxml_add_override(pkg, "/xl/drawings/drawing1.xml", "application/vnd.openxmlformats-officedocument.drawing+xml");
        wubuoxml_add_default_type(pkg, "png", "image/png");
    }
    wubuoxml_add_relationship(pkg, "", "xl/workbook.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");

    /* workbook.xml */
    {
        char *bb = NULL; size_t bn = 0; FILE *m = open_memstream(&bb, &bn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n<sheets>\n");
        for (size_t i = 0; i < b->n; i++)
            fprintf(m, "  <sheet name=\"%s\" sheetId=\"%zu\" r:id=\"rId%zu\"/>\n", b->sheets[i].name, i + 1, i + 1);
        fprintf(m, "</sheets>\n</workbook>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "xl/workbook.xml", bb, bn);
        free(bb);
    }
    /* workbook rels */
    {
        char *sp = NULL; size_t sn = 0; FILE *m = open_memstream(&sp, &sn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
        size_t rid = 1;
        for (size_t i = 0; i < b->n; i++)
            fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%zu.xml\"/>\n", rid++, i + 1);
        fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n", rid++);
        if (b->use_sst)
            fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" Target=\"sharedStrings.xml\"/>\n", rid++);
        if (has_charts)
            fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"drawings/drawing1.xml\"/>\n", rid++);
        fprintf(m, "</Relationships>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "xl/_rels/workbook.xml.rels", sp, sn);
        free(sp);
    }
    /* styles.xml from the registry (or a minimal default) */
    {
        char *sb = NULL; size_t sn = 0;
        if (b->styles) sb = wubucell_style_render(b->styles, &sn);
        else {
            static const char *def =
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                "<fonts count=\"1\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
                "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill></fills>"
                "<borders count=\"1\"><border/></borders>"
                "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
                "<cellXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/></cellXfs></styleSheet>\n";
            sn = strlen(def); sb = strdup(def);
        }
        wubuoxml_add_part(pkg, "xl/styles.xml", sb, sn);
        free(sb);
    }
    /* sharedStrings.xml */
    sst_t sst = {0};
    if (b->use_sst) {
        for (size_t i = 0; i < b->n; i++)
            for (size_t j = 0; j < b->sheets[i].n; j++) {
                const cell_t *c = &b->sheets[i].cells[j];
                if (c->kind == C_STR) sst_add(&sst, c->text ? c->text : "");
            }
        char *sb = NULL; size_t sn = 0; FILE *m = open_memstream(&sb, &sn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"%zu\" uniqueCount=\"%zu\">\n", sst.n, sst.n);
        for (size_t i = 0; i < sst.n; i++) {
            fprintf(m, "<si><t xml:space=\"preserve\">"); xml_escape(m, sst.e[i].s); fprintf(m, "</t></si>\n");
        }
        fprintf(m, "</sst>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "xl/sharedStrings.xml", sb, sn);
        free(sb);
    }
    /* worksheets */
    for (size_t i = 0; i < b->n; i++) {
        char *sxml = render_sheet(b, &b->sheets[i], i, &sst);
        char path[64];
        snprintf(path, sizeof path, "xl/worksheets/sheet%d.xml", (int)(i + 1));
        wubuoxml_add_part(pkg, path, sxml, strlen(sxml));
        free(sxml);
        /* per-sheet drawing rels if this sheet has a chart */
        size_t sheet_charts = 0;
        for (size_t k = 0; k < b->ncharts; k++) if (b->charts[k].sheet == (int)(i + 1)) sheet_charts++;
        if (sheet_charts) {
            char *dr = NULL; size_t dn = 0; FILE *m = open_memstream(&dr, &dn);
            fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
            fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
            fprintf(m, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../drawings/drawing1.xml\"/>\n");
            fprintf(m, "</Relationships>\n");
            fflush(m); fclose(m);
            char rp[80]; snprintf(rp, sizeof rp, "xl/worksheets/_rels/sheet%d.xml.rels", (int)(i + 1));
            wubuoxml_add_part(pkg, rp, dr, dn);
            free(dr);
        }
    }
    /* charts + drawing */
    if (has_charts) {
        for (size_t i = 0; i < b->ncharts; i++) {
            char *cx = render_chart(&b->charts[i], i);
            char path[80]; snprintf(path, sizeof path, "xl/charts/chart%d.xml", (int)(i + 1));
            wubuoxml_add_part(pkg, path, cx, strlen(cx));
            free(cx);
            char *cr = NULL; size_t cn = 0; FILE *m = open_memstream(&cr, &cn);
            fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
            fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
            fprintf(m, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart%d.xml\"/>\n", (int)(i + 1));
            fprintf(m, "</Relationships>\n");
            fflush(m); fclose(m);
            char rp[80]; snprintf(rp, sizeof rp, "xl/charts/_rels/chart%d.xml.rels", (int)(i + 1));
            wubuoxml_add_part(pkg, rp, cr, cn);
            free(cr);
        }
        /* drawing1.xml */
        {
            char *dw = NULL; size_t dn = 0; FILE *m = open_memstream(&dw, &dn);
            fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
            fprintf(m, "<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" "
                         "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
                         "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n");
            fprintf(m, "<xdr:twoCellAnchor><xdr:from><xdr:col>4</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>1</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from>"
                         "<xdr:to><xdr:col>10</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>16</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:to>"
                         "<xdr:graphicFrame><xdr:nvGraphicFramePr><xdr:cNvPr id=\"2\" name=\"Chart 1\"/><xdr:cNvGraphicFramePr/></xdr:nvGraphicFramePr>"
                         "<xdr:graphicFrameLocks noGrp=\"1\"/>"
                         "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\">"
                         "<c:chart xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" r:id=\"rId1\"/>"
                         "</a:graphicData></a:graphic></xdr:graphicFrame>"
                         "<xdr:clientData/></xdr:twoCellAnchor>\n");
            fprintf(m, "</xdr:wsDr>\n");
            fflush(m); fclose(m);
            wubuoxml_add_part(pkg, "xl/drawings/drawing1.xml", dw, dn);
            free(dw);
            /* drawing rels */
            char *dr = NULL; size_t dnr = 0; FILE *m2 = open_memstream(&dr, &dnr);
            fprintf(m2, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
            fprintf(m2, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
            fprintf(m2, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart1.xml\"/>\n");
            fprintf(m2, "</Relationships>\n");
            fflush(m2); fclose(m2);
            wubuoxml_add_part(pkg, "xl/drawings/_rels/drawing1.xml.rels", dr, dnr);
            free(dr);
        }
    }
    int rc = wubuoxml_finalize(pkg);
    for (size_t i = 0; i < sst.n; i++) free(sst.e[i].s);
    free(sst.e);
    fclose(out);
    return rc;
}

void wubucell_free(wubucell_book *b) {
    if (!b) return;
    for (size_t i = 0; i < b->n; i++) {
        free(b->sheets[i].name);
        for (size_t j = 0; j < b->sheets[i].n; j++) { free(b->sheets[i].cells[j].text); free(b->sheets[i].cells[j].formula); }
        free(b->sheets[i].cells);
    }
    for (size_t i = 0; i < b->ncharts; i++) { free(b->charts[i].title); free(b->charts[i].cats); free(b->charts[i].vals); }
    free(b->sheets);
    free(b->charts);
    if (b->styles) wubucell_style_free(b->styles);
    free(b);
}
