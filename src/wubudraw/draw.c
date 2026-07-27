/* draw.c -- dependency-free C11 vector drawing model (see draw.h). */
#include "draw.h"
#include "wububase.h"         /* shared Buf (was private here) */
#include "wubusvg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef enum { SH_RECT, SH_ELLIPSE, SH_LINE, SH_POLY, SH_TEXT } SKind;

typedef struct Shape {
    SKind kind;
    double a, b, c, d, e, f;     /* per-kind geometry */
    double *pts; int n;          /* polyline points (owned copy) */
    char *fill; char *stroke;
    double sw;                    /* stroke width */
    char *text;                   /* text payload */
    double size;
    struct Shape *next;
} Shape;

struct DrawScene {
    int w, h;
    Shape *head, *tail;
};

DrawScene *draw_create(int w, int h){
    DrawScene *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->w = w > 0 ? w : 100; s->h = h > 0 ? h : 100;
    return s;
}
void draw_destroy(DrawScene *s){
    if (!s) return;
    Shape *n = s->head;
    while (n){ Shape *nx = n->next; free(n->pts); free(n->fill); free(n->stroke); free(n->text); free(n); n = nx; }
    free(s);
}

static char *dup_or(const char *s, const char *def){
    if (!s || !*s) s = def;
    return strdup(s);
}
static Shape *new_shape(DrawScene *s, SKind k){
    Shape *sh = calloc(1, sizeof *sh);
    if (!sh) return NULL;
    sh->kind = k;
    sh->fill = strdup("none");
    sh->stroke = strdup("#000000");
    sh->sw = 1.0;
    sh->size = 16;
    if (!sh->fill || !sh->stroke){ free(sh->fill); free(sh->stroke); free(sh); return NULL; }
    if (s->tail) s->tail->next = sh; else s->head = sh;
    s->tail = sh;
    return sh;
}

void draw_add_rect(DrawScene *s, double x, double y, double w, double h,
                   const char *fill, const char *stroke){
    if (!s) return;
    Shape *sh = new_shape(s, SH_RECT); if (!sh) return;
    sh->a=x; sh->b=y; sh->c=w; sh->d=h;
    free(sh->fill); sh->fill = dup_or(fill, "none");
    free(sh->stroke); sh->stroke = dup_or(stroke, "#000000");
}
void draw_add_ellipse(DrawScene *s, double cx, double cy, double rx, double ry,
                      const char *fill, const char *stroke){
    if (!s) return;
    Shape *sh = new_shape(s, SH_ELLIPSE); if (!sh) return;
    sh->a=cx; sh->b=cy; sh->c=rx; sh->d=ry;
    free(sh->fill); sh->fill = dup_or(fill, "none");
    free(sh->stroke); sh->stroke = dup_or(stroke, "#000000");
}
void draw_add_line(DrawScene *s, double x1, double y1, double x2, double y2,
                   const char *stroke, double sw){
    if (!s) return;
    Shape *sh = new_shape(s, SH_LINE); if (!sh) return;
    sh->a=x1; sh->b=y1; sh->c=x2; sh->d=y2;
    free(sh->stroke); sh->stroke = dup_or(stroke, "#000000");
    if (sw > 0) sh->sw = sw;
}
void draw_add_polyline(DrawScene *s, const double *pts, int n,
                       const char *stroke, double sw, int closed){
    if (!s || n < 2 || !pts) return;
    Shape *sh = new_shape(s, SH_POLY); if (!sh) return;
    sh->n = closed ? n + 1 : n;
    sh->pts = malloc((size_t)sh->n * 2 * sizeof(double));
    if (!sh->pts){ s->tail = (s->tail==sh)?NULL:s->tail; free(sh); if(s->tail)s->tail->next=NULL; else s->head=NULL; return; }
    for (int i=0;i<n;i++){ sh->pts[i*2]=pts[i*2]; sh->pts[i*2+1]=pts[i*2+1]; }
    if (closed){ sh->pts[n*2]=pts[0]; sh->pts[n*2+1]=pts[1]; }
    free(sh->stroke); sh->stroke = dup_or(stroke, "#000000");
    if (sw > 0) sh->sw = sw;
}
void draw_add_text(DrawScene *s, double x, double y, const char *text,
                   double size, const char *fill){
    if (!s || !text) return;
    Shape *sh = new_shape(s, SH_TEXT); if (!sh) return;
    sh->a=x; sh->b=y; sh->size = size > 0 ? size : 16;
    free(sh->fill); sh->fill = dup_or(fill, "#000000");
    sh->text = strdup(text);
}

char *draw_render_svg(DrawScene *s){
    if (!s) return NULL;
    Buf b; buf_init(&b);
    buf_printf(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    buf_printf(&b, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" "
                  "viewBox=\"0 0 %d %d\">\n", s->w, s->h, s->w, s->h);
    buf_printf(&b, "<rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" fill=\"#ffffff\"/>\n", s->w, s->h);

    for (Shape *sh = s->head; sh; sh = sh->next){
        switch (sh->kind){
        case SH_RECT:
            buf_printf(&b, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
                           "fill=\"%s\" stroke=\"%s\"/>\n",
                       sh->a, sh->b, sh->c, sh->d, sh->fill, sh->stroke);
            break;
        case SH_ELLIPSE:
            buf_printf(&b, "<ellipse cx=\"%.2f\" cy=\"%.2f\" rx=\"%.2f\" ry=\"%.2f\" "
                           "fill=\"%s\" stroke=\"%s\"/>\n",
                       sh->a, sh->b, sh->c, sh->d, sh->fill, sh->stroke);
            break;
        case SH_LINE:
            buf_printf(&b, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                           "stroke=\"%s\" stroke-width=\"%.2f\"/>\n",
                       sh->a, sh->b, sh->c, sh->d, sh->stroke, sh->sw);
            break;
        case SH_POLY: {
            buf_printf(&b, "<polyline points=\"");
            for (int i=0;i<sh->n;i++)
                buf_printf(&b, "%.2f,%.2f ", sh->pts[i*2], sh->pts[i*2+1]);
            buf_printf(&b, "\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\"/>\n",
                       sh->stroke, sh->sw);
            break;
        }
        case SH_TEXT:
            buf_printf(&b, "<text x=\"%.2f\" y=\"%.2f\" font-family=\"sans-serif\" "
                           "font-size=\"%.2f\" fill=\"%s\">", sh->a, sh->b, sh->size, sh->fill);
            wububase_xml_escape(&b, sh->text);
            buf_add(&b, "</text>\n");
            break;
        }
    }
    buf_add(&b, "</svg>\n");

    char *out = NULL;
    SvgDoc *doc = svg_parse(buf_str(&b), buf_len(&b));
    if (doc){ out = svg_regurgitate(doc); svg_free(doc); }
    buf_free(&b);
    return out;
}
