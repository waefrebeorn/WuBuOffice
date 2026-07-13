#include "style.h"

#include <stdlib.h>
#include <string.h>

typedef struct { char *name; double size; int bold, italic; char *color; } font_t;
typedef struct { char *color; } fill_t;
typedef struct { char *style; char *color; } border_t;
typedef struct { char *code; int id; } numfmt_t;
typedef struct { int font, fill, border, numfmt; char *align; } xf_t;

struct wubucell_style {
    font_t *fonts; size_t nfonts, capf;
    fill_t *fills; size_t nfills, capfl;
    border_t *borders; size_t nbor, capb;
    numfmt_t *numfmts; size_t nnum, capn;
    xf_t *xfs; size_t nxfs, capx;
};

wubucell_style *wubucell_style_create(void) { return calloc(1, sizeof(wubucell_style)); }

int wubucell_style_font(wubucell_style *s, const char *name, double size, int bold, int italic, const char *color) {
    if (s->nfonts == s->capf) { s->capf = s->capf ? s->capf*2 : 4; s->fonts = realloc(s->fonts, s->capf*sizeof(*s->fonts)); }
    font_t *f = &s->fonts[s->nfonts];
    memset(f, 0, sizeof *f);
    f->name = strdup(name ? name : "Calibri");
    f->size = size > 0 ? size : 11;
    f->bold = bold; f->italic = italic;
    f->color = color ? strdup(color) : NULL;
    return (int)s->nfonts++;
}

int wubucell_style_fill(wubucell_style *s, const char *color) {
    if (s->nfills == s->capfl) { s->capfl = s->capfl ? s->capfl*2 : 4; s->fills = realloc(s->fills, s->capfl*sizeof(*s->fills)); }
    fill_t *f = &s->fills[s->nfills];
    memset(f, 0, sizeof *f);
    f->color = color ? strdup(color) : NULL;
    return (int)s->nfills++;
}

int wubucell_style_border(wubucell_style *s, const char *style, const char *color) {
    if (s->nbor == s->capb) { s->capb = s->capb ? s->capb*2 : 4; s->borders = realloc(s->borders, s->capb*sizeof(*s->borders)); }
    border_t *b = &s->borders[s->nbor];
    memset(b, 0, sizeof *b);
    b->style = strdup(style ? style : "none");
    b->color = color ? strdup(color) : NULL;
    return (int)s->nbor++;
}

int wubucell_style_numfmt(wubucell_style *s, const char *code) {
    if (s->nnum == s->capn) { s->capn = s->capn ? s->capn*2 : 4; s->numfmts = realloc(s->numfmts, s->capn*sizeof(*s->numfmts)); }
    numfmt_t *n = &s->numfmts[s->nnum];
    n->code = strdup(code ? code : "General");
    n->id = 164 + (int)s->nnum;   /* custom numFmtIds start at 164 */
    return n->id;
}

int wubucell_style_cell(wubucell_style *s, int font, int fill, int border, int numfmt, const char *align) {
    if (s->nxfs == s->capx) { s->capx = s->capx ? s->capx*2 : 4; s->xfs = realloc(s->xfs, s->capx*sizeof(*s->xfs)); }
    xf_t *x = &s->xfs[s->nxfs];
    memset(x, 0, sizeof *x);
    x->font = font; x->fill = fill; x->border = border; x->numfmt = numfmt;
    x->align = align ? strdup(align) : NULL;
    return (int)s->nxfs++;
}

static void esc_attr(FILE *m, const char *t) {
    for (; t && *t; t++) {
        switch (*t) { case '&': fputs("&amp;", m); break; case '<': fputs("&lt;", m); break;
                      case '>': fputs("&gt;", m); break; case '"': fputs("&quot;", m); break; default: fputc(*t, m); }
    }
}

char *wubucell_style_render(const wubucell_style *s, size_t *out_len) {
    char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
    fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    fprintf(m, "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n");

    /* fonts */
    fprintf(m, "<fonts count=\"%zu\">\n", s->nfonts);
    for (size_t i = 0; i < s->nfonts; i++) {
        const font_t *f = &s->fonts[i];
        fprintf(m, "<font>");
        if (f->bold) fprintf(m, "<b/>");
        if (f->italic) fprintf(m, "<i/>");
        fprintf(m, "<sz val=\"%.0f\"/>", f->size);
        if (f->color) fprintf(m, "<color rgb=\"%s\"/>", f->color);
        fprintf(m, "<name val=\""); esc_attr(m, f->name); fprintf(m, "\"/></font>\n");
    }
    fprintf(m, "</fonts>\n");

    /* fills: index 0 = none, 1 = gray125 (required placeholders) then solids */
    fprintf(m, "<fills count=\"%zu\">\n", s->nfills + 2);
    fprintf(m, "<fill><patternFill patternType=\"none\"/></fill>\n");
    fprintf(m, "<fill><patternFill patternType=\"gray125\"/></fill>\n");
    for (size_t i = 0; i < s->nfills; i++) {
        const fill_t *f = &s->fills[i];
        if (f->color)
            fprintf(m, "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"%s\"/></patternFill></fill>\n", f->color);
        else
            fprintf(m, "<fill><patternFill patternType=\"none\"/></fill>\n");
    }
    fprintf(m, "</fills>\n");

    /* borders */
    fprintf(m, "<borders count=\"%zu\">\n", s->nbor ? s->nbor : 1);
    if (s->nbor == 0) {
        fprintf(m, "<border><left/><right/><top/><bottom/><diagonal/></border>\n");
    } else {
        for (size_t i = 0; i < s->nbor; i++) {
            const border_t *bd = &s->borders[i];
            if (strcmp(bd->style, "none") == 0) {
                fprintf(m, "<border><left/><right/><top/><bottom/><diagonal/></border>\n");
            } else {
                fprintf(m, "<border>");
                const char *edges[] = {"left","right","top","bottom",NULL};
                for (int e = 0; edges[e]; e++) {
                    fprintf(m, "<%s style=\"%s\"", edges[e], bd->style);
                    if (bd->color) fprintf(m, "><color rgb=\"%s\"/></%s>", bd->color, edges[e]);
                    else fprintf(m, "/>");
                }
                fprintf(m, "<diagonal/></border>\n");
            }
        }
    }
    fprintf(m, "</borders>\n");

    /* cellStyleXfs (one default) then cellXfs */
    fprintf(m, "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>\n");
    fprintf(m, "<cellXfs count=\"%zu\">\n", s->nxfs);
    for (size_t i = 0; i < s->nxfs; i++) {
        const xf_t *x = &s->xfs[i];
        fprintf(m, "<xf numFmtId=\"%d\" fontId=\"%d\" fillId=\"%d\" borderId=\"%d\" xfId=\"0\"",
                x->numfmt, x->font, x->fill + 2, x->border);  /* fillId offset by 2 placeholders */
        int applied = (x->font != 0) || (x->fill != 0) || (x->border != 0) || (x->numfmt != 0) || x->align;
        if (applied) fprintf(m, " applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyNumberFormat=\"1\"");
        if (x->align) { fprintf(m, "><alignment horizontal=\"%s\"/></xf>\n", x->align); }
        else fprintf(m, "/>\n");
    }
    fprintf(m, "</cellXfs>\n");

    fprintf(m, "</styleSheet>\n");
    fflush(m); fclose(m);
    if (out_len) *out_len = n;
    return b;
}

void wubucell_style_free(wubucell_style *s) {
    if (!s) return;
    for (size_t i = 0; i < s->nfonts; i++) { free(s->fonts[i].name); free(s->fonts[i].color); }
    for (size_t i = 0; i < s->nfills; i++) free(s->fills[i].color);
    for (size_t i = 0; i < s->nbor; i++) { free(s->borders[i].style); free(s->borders[i].color); }
    for (size_t i = 0; i < s->nnum; i++) free(s->numfmts[i].code);
    for (size_t i = 0; i < s->nxfs; i++) free(s->xfs[i].align);
    free(s->fonts); free(s->fills); free(s->borders); free(s->numfmts); free(s->xfs);
    free(s);
}
