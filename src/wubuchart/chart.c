/* chart.c -- dependency-free C11 chart renderer (see chart.h). */
#include "chart.h"
#include "wubusvg.h"          /* SVG validate/regurgitate pipeline */
#include "eval.h"             /* optional formula subtitle */
#include "funcs.h"            /* wubu_formula_register_all */
#include "value_util.h"
#include "wububase.h"         /* shared Buf + wububase_xml_escape (was private here) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- internal model ---------- */
typedef struct {
    char  *name;
    double *y;
    char  **labels;
    int     n;
} Series;

struct Chart {
    char    *title;
    char    *subtitle_formula;
    Series  *s;
    int      nseries;
    int      cap;
    ChartType type;
    int      w, h;
};

/* ---------- creation ---------- */
Chart *chart_create(const char *title){
    Chart *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->title = title ? strdup(title) : NULL;
    c->type = CHART_BAR;
    c->w = 640; c->h = 400;
    return c;
}

void chart_free(Chart *c){
    if (!c) return;
    free(c->title);
    free(c->subtitle_formula);
    for (int i=0;i<c->nseries;i++){
        free(c->s[i].name);
        free(c->s[i].y);
        for (int j=0;j<c->s[i].n;j++) free(c->s[i].labels[j]);
        free(c->s[i].labels);
    }
    free(c->s);
    free(c);
}

void chart_set_size(Chart *c, int w, int h){ if(c){ c->w=w>0?w:c->w; c->h=h>0?h:c->h; } }
void chart_set_type(Chart *c, ChartType t){ if(c) c->type=t; }
void chart_set_subtitle_formula(Chart *c, const char *formula){
    if(!c) return;
    free(c->subtitle_formula);
    c->subtitle_formula = formula ? strdup(formula) : NULL;
}

static Series *new_series(Chart *c){
    if (c->nseries >= c->cap){
        int nc = c->cap ? c->cap*2 : 4;
        Series *ns = realloc(c->s, (size_t)nc*sizeof *ns);
        if (!ns) return NULL;
        c->s = ns; c->cap = nc;
    }
    Series *s = &c->s[c->nseries++];
    memset(s, 0, sizeof *s);
    return s;
}

void chart_add_series(Chart *c, const char *name,
                      const double *y, int n, const char *const *labels){
    if (!c || n <= 0 || !y) return;
    Series *s = new_series(c);
    if (!s) return;
    s->n = n;
    s->name = name ? strdup(name) : NULL;
    s->y = malloc((size_t)n * sizeof(double));
    s->labels = malloc((size_t)n * sizeof(char*));
    if (!s->y || !s->labels){ free(s->y); free(s->labels); s->y=NULL; s->labels=NULL; c->nseries--; return; }
    for (int i=0;i<n;i++){
        s->y[i] = y[i];
        s->labels[i] = (labels && labels[i]) ? strdup(labels[i]) : NULL;
    }
}

/* ---------- helpers ---------- */
static double total_y(Chart *c){
    double t=0;
    for (int i=0;i<c->nseries;i++) for (int j=0;j<c->s[i].n;j++) t += c->s[i].y[j];
    return t;
}
static double max_abs_y(Chart *c){
    double m=0;
    for (int i=0;i<c->nseries;i++) for (int j=0;j<c->s[i].n;j++){
        double v = fabs(c->s[i].y[j]);
        if (v>m) m=v;
    }
    return m;
}

static void draw_background(Buf *b, Chart *c){
    buf_printf(b, "<rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" fill=\"#ffffff\"/>\n", c->w, c->h);
}
static void draw_title(Buf *b, Chart *c, int y){
    buf_printf(b, "<text x=\"%d\" y=\"%d\" font-family=\"sans-serif\" font-size=\"16\" "
                  "font-weight=\"bold\" text-anchor=\"middle\" fill=\"#222\">", c->w/2, y);
    wububase_xml_escape(b, c->title ? c->title : "Chart");
    buf_add(b, "</text>\n");
}

/* pie wedge path from angle a0->a1 (radians) at center cx,cy radius r */
static void pie_path(Buf *b, double cx, double cy, double r, double a0, double a1){
    double x0 = cx + r*cos(a0), y0 = cy + r*sin(a0);
    double x1 = cx + r*cos(a1), y1 = cy + r*sin(a1);
    int large = (a1 - a0) > M_PI ? 1 : 0;
    buf_printf(b, "M %.2f %.2f L %.2f %.2f A %.2f %.2f 0 %d 1 %.2f %.2f Z",
               cx, cy, x0, y0, r, r, large, x1, y1);
}

/* ---------- render ---------- */
char *chart_render_svg(Chart *c){
    if (!c || c->nseries == 0) return NULL;

    Buf b; buf_init(&b);
    buf_printf(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    buf_printf(&b, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" "
                  "viewBox=\"0 0 %d %d\">\n", c->w, c->h, c->w, c->h);

    draw_background(&b, c);
    draw_title(&b, c, 24);

    int top = 40, bottom = c->h - 40, left = 50, right = c->w - 20;

    if (c->type == CHART_PIE){
        double total = total_y(c);
        double cx = c->w/2, cy = (top+bottom)/2 + 10;
        double r = (bottom - top) / 2.0;
        if (r > (right-left)/2) r = (right-left)/2.0;
        double a = -M_PI/2.0;
        const char *palette[] = {"#4e79a7","#f28e2b","#59a14f","#e15759","#76b7b2",
                                 "#edc948","#b07aa1","#9c755f","#86bcb6","#d37295"};
        int pc = (int)(sizeof(palette)/sizeof(palette[0]));
        int si = 0;
        for (int i=0;i<c->nseries;i++){
            Series *s = &c->s[i];
            for (int j=0;j<s->n;j++){
                double frac = total>0 ? s->y[j]/total : 0;
                double a1 = a + frac*2*M_PI;
                buf_printf(&b, "<path d=\"");
                pie_path(&b, cx, cy, r, a, a1);
                buf_printf(&b, "\" fill=\"%s\" stroke=\"#fff\" stroke-width=\"1\"/>\n",
                           palette[si % pc]);
                /* legend: category label */
                const char *lab = s->labels[j] ? s->labels[j] : "";
                buf_printf(&b, "<rect x=\"%d\" y=\"%d\" width=\"12\" height=\"12\" fill=\"%s\"/>"
                               "<text x=\"%d\" y=\"%d\" font-family=\"sans-serif\" font-size=\"12\" fill=\"#222\">",
                           right+4, top + si*18, palette[si%pc], right+20, top+si*18+10);
                wububase_xml_escape(&b, lab); buf_add(&b, "</text>\n");
                a = a1; si++;
            }
        }
    } else {
        /* axis box */
        buf_printf(&b, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#888\"/>\n",
                   left, top, left, bottom);
        buf_printf(&b, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#888\"/>\n",
                   left, bottom, right, bottom);

        double maxv = max_abs_y(c);
        if (maxv <= 0) maxv = 1;
        double scale = (bottom - top) / maxv;

        /* categories: union of first series labels (or indices) */
        int ncat = c->s[0].n;
        const char *palette[] = {"#4e79a7","#f28e2b","#59a14f","#e15759","#76b7b2"};
        int pc = (int)(sizeof(palette)/sizeof(palette[0]));

        if (c->type == CHART_BAR){
            double catw = (double)(right - left) / (double)ncat;
            double gw = catw / (c->nseries > 0 ? c->nseries : 1);
            for (int i=0;i<c->nseries;i++){
                Series *s = &c->s[i];
                for (int j=0;j<s->n;j++){
                    double v = s->y[j];
                    double bh = v * scale;
                    double x = left + j*catw + i*gw + 1;
                    double y = (v >= 0) ? bottom - bh : bottom;
                    double hh = fabs(bh);
                    buf_printf(&b, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
                                   "fill=\"%s\"/>\n", x, y, gw-2, hh, palette[i%pc]);
                }
            }
            /* x labels under bars */
            for (int j=0;j<ncat;j++){
                const char *lab = c->s[0].labels[j] ? c->s[0].labels[j] : "";
                if (*lab){
                    buf_printf(&b, "<text x=\"%.2f\" y=\"%d\" font-family=\"sans-serif\" "
                                       "font-size=\"10\" text-anchor=\"middle\" fill=\"#222\">",
                               left + (j+0.5)*catw, bottom+14);
                    wububase_xml_escape(&b, lab); buf_add(&b, "</text>\n");
                }
            }
        } else if (c->type == CHART_LINE){
            for (int i=0;i<c->nseries;i++){
                Series *s = &c->s[i];
                Buf path; buf_init(&path);
                for (int j=0;j<s->n;j++){
                    double x = left + (ncat>1 ? (double)j/(ncat-1)*(right-left) : left);
                    double y = bottom - s->y[j]*scale;
                    buf_printf(&path, j==0 ? "M %.2f %.2f" : " L %.2f %.2f", x, y);
                }
                buf_printf(&b, "<path d=\"%s\" fill=\"none\" stroke=\"%s\" stroke-width=\"2\"/>\n",
                           path.p ? path.p : "", palette[i%pc]);
                buf_free(&path);
                for (int j=0;j<s->n;j++){
                    double x = left + (ncat>1 ? (double)j/(ncat-1)*(right-left) : left);
                    double y = bottom - s->y[j]*scale;
                    buf_printf(&b, "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"3\" fill=\"%s\"/>\n", x, y, palette[i%pc]);
                }
                if (c->s[0].labels[0]){
                    for (int j=0;j<ncat;j++){
                        const char *lab = c->s[0].labels[j];
                        if (lab && *lab){
                            double x = left + (ncat>1 ? (double)j/(ncat-1)*(right-left) : left);
                            buf_printf(&b, "<text x=\"%.2f\" y=\"%d\" font-family=\"sans-serif\" "
                                       "font-size=\"10\" text-anchor=\"middle\" fill=\"#222\">", x, bottom+14);
                            wububase_xml_escape(&b, lab); buf_add(&b, "</text>\n");
                        }
                    }
                }
            }
        } else { /* SCATTER */
            for (int i=0;i<c->nseries;i++){
                Series *s = &c->s[i];
                for (int j=0;j<s->n;j++){
                    double v = s->y[j];
                    /* scatter: x by index, y by value */
                    double x = left + (s->n>1 ? (double)j/(s->n-1)*(right-left) : left);
                    double y = bottom - v*scale;
                    buf_printf(&b, "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" fill=\"%s\" "
                                   "fill-opacity=\"0.7\"/>\n", x, y, palette[i%pc]);
                }
            }
        }
    }

    /* Optional formula subtitle */
    if (c->subtitle_formula){
        wubu_formula_register_all();
        wubuval v; memset(&v, 0, sizeof v);
        if (wubu_formula_eval(c->subtitle_formula, NULL, NULL, &v) == 0){
            char *s = wubu_num_to_str(v.num);
            buf_printf(&b, "<text x=\"%d\" y=\"%d\" font-family=\"sans-serif\" font-size=\"11\" "
                           "text-anchor=\"middle\" fill=\"#666\">= %s &#8594; %s</text>\n",
                       c->w/2, c->h-12, c->subtitle_formula, s ? s : "");
            free(s);
        }
    }

    buf_add(&b, "</svg>\n");

    /* Finalize through wubusvg (validate well-formed + canonicalize output). */
    char *out = NULL;
    SvgDoc *doc = svg_parse(buf_str(&b), buf_len(&b));
    if (doc){
        out = svg_regurgitate(doc);
        svg_free(doc);
    }
    buf_free(&b);
    return out;
}
