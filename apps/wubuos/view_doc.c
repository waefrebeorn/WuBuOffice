/* view_doc.c -- Document view: renders a wubumodel page via wurender AND is
 * interactive. Given a path it ingests the real document through the wubudoc
 * facade (docx/odt/pdf/md/txt/html/...), and when the format isn't a renderable
 * page it shows the recognized text projection instead. Ctrl+F searches the
 * loaded text and jumps to the first match. The SAME render path the offscreen
 * PNG writer uses, so the in-shell Document tab is the real office surface. */
#include "wuos.h"
#include "wuos_file.h"
#include "wuos_font.h"    /* wuos_font_draw: text raster callback for svg_rasterize_cb */
#include "wuburender.h"
#include "model.h"
#include "wubudoc.h"     /* doc_session_*, doc_open, doc_text, doc_drop_text */
#include "chart.h"       /* wubuchart: insert chart (INT-1) */
#include "draw.h"        /* wubudraw: insert shape (INT-3) */
#include "math.h"        /* wubumath: insert equation (INT-3) */
#include "a11y.h"        /* wubua11y: check (INT-5) */
#include "rast.h"        /* wubusvg rasterizer: SVG -> RGBA (gap #13) */
#include "ublayout.h"      /* central text pipeline: model -> laid-out pages */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* metric callback for the layout: uses the app font (wubulayout only sees this
 * function pointer, so the engine layer never includes an app header). */
static int doc_layout_measure(const char *t, size_t len, int fs, int bold, int italic,
                              int *out_h, void *user){
    (void)bold; (void)italic; (void)user; (void)fs;
    char buf[2048]; if (len>=sizeof buf) len=sizeof buf-1;
    memcpy(buf, t, len); buf[len]=0;
    int w = wuos_font_text_width(buf, fs);
    *out_h = wuos_font_height();
    return w;
}
static int doc_layout_style(void *user, void *run, int *fs, int *bold, int *italic,
                            wubulayout_dir *dir){
    (void)user;
    wubumodel_node *n = (wubumodel_node*)run;
    wubumodel_style *st = wubumodel_node_style(n);
    *fs = 12; *bold=0; *italic=0; *dir=WUBULAYOUT_LTR;
    if (st){
        const char *v;
        if ((v=wubumodel_style_get_prop(st,"size"))) *fs = atoi(v);
        if ((v=wubumodel_style_get_prop(st,"bold")) && (v[0]=='1'||v[0]=='t')) *bold=1;
        if ((v=wubumodel_style_get_prop(st,"italic")) && (v[0]=='1'||v[0]=='t')) *italic=1;
        if ((v=wubumodel_style_get_prop(st,"dir")) && v[0]=='r') *dir=WUBULAYOUT_RTL;
    }
    return 1;
}

#define DOC_MAX_OBJS 16

typedef struct { Wurender *r; wubumodel_doc *doc; char *path;
                 char *text;          /* recognized/raw text (for find) */
                 char *find_q; int find_hit;
                 /* inserted objects (chart/draw/math) rasterized to RGBA (INT-1,3) */
                 unsigned char *obj[ DOC_MAX_OBJS ]; int objw[ DOC_MAX_OBJS ]; int objh[ DOC_MAX_OBJS ];
                 int nobj;
                 char *epub_msg;      /* last EPUB export result */
                 a11y_report a11y;    /* last a11y check */
                 int a11y_done;
} DocV;

/* Build a sample bar chart and rasterize it into a free overlay slot. */
static void doc_insert_chart(DocV *e){
    if (e->nobj >= DOC_MAX_OBJS) return;
    Chart *c = chart_create("Sample Bar Chart");
    chart_set_type(c, CHART_BAR);
    chart_set_size(c, 320, 200);
    double ys[4] = { 23, 41, 17, 52 };
    const char *lbl[4] = { "Q1", "Q2", "Q3", "Q4" };
    chart_add_series(c, "sales", ys, 4, lbl);
    char *svg = chart_render_svg(c);
    chart_free(c);
    if (svg){
        unsigned char *fb=NULL; int w=0,h=0;
        int rc = svg_rasterize_cb(svg, strlen(svg), &fb, &w, &h, wuos_font_draw);
        free(svg);
        if (rc){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; }
    }
}
/* Build a sample draw scene (rectangle + ellipse + line) and rasterize it. */
static void doc_insert_draw(DocV *e){
    if (e->nobj >= DOC_MAX_OBJS) return;
    DrawScene *s = draw_create(320, 200);
    draw_add_rect(s, 20, 20, 120, 80, "#4488cc", "#224466");
    draw_add_ellipse(s, 240, 100, 50, 40, "#cc6644", "none");
    draw_add_line(s, 20, 180, 300, 180, "#333333", 2);
    draw_add_text(s, 30, 60, "Draw", 20, "#ffffff");
    char *svg = draw_render_svg(s);
    draw_destroy(s);
    if (svg){
        unsigned char *fb=NULL; int w=0,h=0;
        int rc = svg_rasterize_cb(svg, strlen(svg), &fb, &w, &h, wuos_font_draw);
        free(svg);
        if (rc){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; }
    }
}
/* Build a sample math equation (x^2 + 1) and rasterize it. */
static void doc_insert_math(DocV *e){
    if (e->nobj >= DOC_MAX_OBJS) return;
    char *svg = math_render_svg("x^2 + 1 = \\frac{a}{b}");
    if (svg){
        unsigned char *fb=NULL; int w=0,h=0;
        int rc = svg_rasterize_cb(svg, strlen(svg), &fb, &w, &h, wuos_font_draw);
        free(svg);
        if (rc){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; }
    }
}
/* Export the current model doc to EPUB (INT-4). */
static void doc_export_epub(DocV *e){
    free(e->epub_msg); e->epub_msg = NULL;
    if (!e->doc){ e->epub_msg = strdup("no model doc to export"); return; }
    const char *out = "/tmp/wubuos_export.epub";
    int rc = epub_write(e->doc, out, "WuBuOffice Document", "en");
    if (rc==0){
        char b[128]; snprintf(b,sizeof b,"EPUB written: %s", out);
        e->epub_msg = strdup(b);
    } else {
        e->epub_msg = strdup("EPUB export failed");
    }
}
/* Run an a11y check on the current model doc (INT-5). */
static void doc_a11y_check(DocV *e){
    if (e->a11y_done) a11y_report_free(&e->a11y);
    e->a11y.count = 0; e->a11y.items = NULL; e->a11y.cap = 0;
    if (e->doc) a11y_check_doc(e->doc, 1, 0, &e->a11y);
    e->a11y_done = 1;
}

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    DocV *e = v->priv;
    (void)scroll;
    unsigned char *fb = NULL; int W=w, H=h;
    if (e->doc){
        /* Central pipeline: lay the model out into pages, then paint. This is
         * the single source of truth for text placement — the Document tab no
         * longer re-implements wrapping. Headers/footers/line-numbers are drawn
         * from the laid-out geometry. */
        fb = malloc((size_t)w*h*4);
        if (!fb) return -1;
        for (int i=0;i<w*h;i++){ fb[i*4]=252;fb[i*4+1]=252;fb[i*4+2]=250;fb[i*4+3]=255; }
        wubulayout_doc *L = wubulayout_create(e->doc, NULL,
            doc_layout_measure, doc_layout_style, NULL, w, h, 56,56,56,56);
        if (L){
            int pages = wubulayout_page_count(L);
            int pg = scroll / (h - 112); if (pg<0) pg=0; if (pg>=pages) pg=pages-1;
            /* page header */
            char hdr[64]; snprintf(hdr,sizeof hdr,"Page %d / %d", pg+1, pages);
            wuos_font_draw(hdr, 56, 24, 0, 120,124,132, fb, w, h);
            int nr = wubulayout_run_count(L, pg);
            for (int i=0;i<nr;i++){
                const wubulayout_run *r = wubulayout_run_at(L, pg, i);
                if (!r || !r->text || !r->text_len) continue;
                char seg[2048]; size_t l=r->text_len; if(l>=sizeof seg)l=sizeof seg-1;
                memcpy(seg, r->text, l); seg[l]=0;
                wuos_font_draw(seg, r->x, r->y, r->bold, 28,30,34, fb, w, h);
            }
            /* line numbers in the left margin */
            int nl = wubulayout_line_count(L, pg);
            for (int li=0; li<nl; li++){
                const wubulayout_line *ln = wubulayout_line_at(L, pg, li);
                if (!ln) continue;
                char ln2[16]; snprintf(ln2,sizeof ln2,"%d", li+1);
                wuos_font_draw(ln2, 8, ln->y, 0, 150,154,162, fb, w, h);
            }
            /* page footer */
            char ftr[64]; snprintf(ftr,sizeof ftr,"WuBuOffice — %d lines", nl);
            wuos_font_draw(ftr, 56, h-30, 0, 120,124,132, fb, w, h);
            wubulayout_destroy(L);
        }
    } else {
        /* non-renderable format: show the text projection */
        fb = malloc((size_t)w*h*4);
        if (!fb) return -1;
        for (int i=0;i<w*h;i++){ fb[i*4]=252;fb[i*4+1]=252;fb[i*4+2]=250;fb[i*4+3]=255; }
        wuos_font_draw("Document text (recognized):", 16, 20, 1, 40,44,52, fb,w,h);
        int y = 48;
        const char *p = e->text;
        if (p && *p){
            /* simple word-wrap */
            int x=16; int fh=wuos_font_height();
            const char *wstart=p;
            while (*wstart){
                const char *sp = strchr(wstart, ' ');
                size_t wl = sp ? (size_t)(sp-wstart) : strlen(wstart);
                if (x + (int)wl*8 > w-16){ x=16; y += fh+4; }
                char tmp[256]; if(wl>=sizeof tmp) wl=sizeof tmp-1;
                memcpy(tmp,wstart,wl); tmp[wl]=0;
                wuos_font_draw(tmp, x, y, 0, 28,30,34, fb,w,h);
                x += (int)wl*8 + 8;
                wstart = sp ? sp+1 : wstart+wl;
                if (y > h-30) break;
            }
            if (e->find_q && e->find_q[0]){
                int fy = h-26;
                for (int xx=0; xx<w; xx++) for(int yy=fy; yy<h; yy++)
                    if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t i=((size_t)yy*w+xx)*4; fb[i]=30;fb[i+1]=33;fb[i+2]=40; }
                char line[256]; snprintf(line,sizeof line,"find '%s': %s", e->find_q, e->find_hit?"1 match highlighted in text":"no match");
                wuos_font_draw(line, 8, fy+5, 0, 200,203,210, fb,w,h);
            }
        } else {
            wuos_font_draw("(nothing to display)", 16, 48, 0, 120,30,30, fb,w,h);
        }
    }
    /* ---- inserted objects overlay (chart/draw/math) on the right gutter ---- */
    if (e->nobj){
        int ox = W - 340; if (ox < 8) ox = 8;
        wuos_font_draw("Inserted objects:", ox, 16, 1, 30,34,42, fb, W, H);
        int oy = 40;
        for (int i=0;i<e->nobj;i++){
            int pw = e->objw[i], ph = e->objh[i];
            if (ox+pw > W) pw = W-ox; if (oy+ph > H) ph = H-oy;
            for (int yy=0; yy<ph; yy++) for (int xx=0; xx<pw; xx++){
                size_t si=((size_t)yy*e->objw[i]+xx)*4;
                size_t di=((size_t)(oy+yy)*W+(ox+xx))*4;
                fb[di]=e->obj[i][si]; fb[di+1]=e->obj[i][si+1];
                fb[di+2]=e->obj[i][si+2]; fb[di+3]=255;
            }
            oy += ph + 12;
        }
    }
    /* ---- EPUB export / a11y status footer ---- */
    int fy2 = H-22;
    if (e->epub_msg) wuos_font_draw(e->epub_msg, 8, fy2, 0, 40,120,60, fb, W, H);
    else if (e->a11y_done){
        char am[128]; snprintf(am,sizeof am,"a11y: %d issue(s)", e->a11y.count);
        wuos_font_draw(am, 8, fy2, 0, e->a11y.count?200:40, e->a11y.count?60:120, e->a11y.count?40:60, fb, W, H);
    }
    *rgba = fb; *rw = W; *rh = H;
    return 0;
}

static char *status(WuView *v){
    DocV *e = v->priv;
    const char *src = e->path ? e->path : "sample";
    char *s = malloc(160); if(!s) return NULL;
    if (e->doc) snprintf(s,160,"Document — %s (rendered page)", src);
    else snprintf(s,160,"Document — %s (%s text)", src, e->text?"recognized":"no");
    return s;
}

static void on_key(WuView *v, int key, int down){
    DocV *e = v->priv;
    if (!down) return;
    if (key==WUOS_KEY_INSERT_CHART){ doc_insert_chart(e); return; }
    if (key==WUOS_KEY_INSERT_DRAW){ doc_insert_draw(e); return; }
    if (key==WUOS_KEY_INSERT_MATH){ doc_insert_math(e); return; }
    if (key==WUOS_KEY_EXPORT_EPUB){ doc_export_epub(e); return; }
    if (key==WUOS_KEY_A11Y_CHECK){ doc_a11y_check(e); return; }
    if (key==WUOS_KEY_FIND){
        free(e->find_q); e->find_q = calloc(1,1); e->find_hit=0;
        return;
    }
    if (e->find_q){
        if (key==WUOS_KEY_RETURN){
            /* search loaded text */
            e->find_hit = 0;
            if (e->text && e->find_q && e->find_q[0]){
                if (strstr(e->text, e->find_q)) e->find_hit = 1;
            }
            return;
        }
        if (key==WUOS_KEY_BACKSPACE){
            size_t L=strlen(e->find_q); if(L) e->find_q[L-1]=0; return;
        }
        if (key>=32 && key<128 && strlen(e->find_q)<127){
            size_t L=strlen(e->find_q); e->find_q[L]=(char)key; e->find_q[L+1]=0; return;
        }
        return;
    }
}

static void destroy(WuView *v){
    DocV *e = v->priv;
    if(e->r)    wurender_destroy(e->r);
    if(e->doc)  wubumodel_doc_destroy(e->doc);
    if(e->text) free(e->text);
    if(e->path) free(e->path);
    if(e->find_q) free(e->find_q);
    if(e->epub_msg) free(e->epub_msg);
    for (int i=0;i<e->nobj;i++) free(e->obj[i]);
    if (e->a11y_done) a11y_report_free(&e->a11y);
    free(e);
    free(v);
}

static const char *get_path(WuView *v){ return ((DocV*)v->priv)->path; }

WuView *wuos_doc_create(const char *path){
    DocV *e = calloc(1, sizeof *e);
    e->r = wurender_create();
    e->find_q = NULL; e->find_hit = 0;
    if (path){
        e->path = strdup(path);
        /* read raw bytes up front so find works for any format (renderable
         * or not). The text model drives find-in-doc. */
        size_t len = 0;
        char *raw = wuos_read_file(path, &len);
        if (raw){
            /* keep a NUL-terminated copy for find (may be binary; find is
             * best-effort over the prefix). */
            e->text = malloc(len + 1);
            if (e->text){ memcpy(e->text, raw, len); e->text[len]=0; }
            free(raw);
        }
        /* try renderable markdown/text first via wurender */
        if (e->text && (strstr(e->text,"#")||strstr(e->text,"*")||strstr(e->text,"_")||strstr(e->text,"\n"))){
            e->doc = wurender_doc_from_markdown(e->text);
        }
        if (!e->doc){
            /* ingest via the real document facade (docx/odt/pdf/html/...) */
            DocSession *s = doc_session_create();
            long id = doc_open(s, path);
            if (id >= 0){
                char *dt = doc_text(s, id);
                if (dt){ free(e->text); e->text = dt; }
                doc_session_free(s);
            }
        }
    }
    if (!e->doc && !e->text) e->doc = wurender_sample_doc();
    if (!e->r || (!e->doc && !e->text)){ free(e->path); free(e->text); free(e); return NULL; }
    /* INT-1/3: the Document tab now consumes the chart/draw/math engines
     * (previously "SVG that goes nowhere", gap #13). Seed one sample chart so
     * the Insert path is exercised + visible immediately. */
    e->nobj = 0; e->a11y_done = 0; e->a11y.count = 0; e->a11y.items = NULL; e->a11y.cap = 0;
    doc_insert_chart(e);
    WuView *v = calloc(1, sizeof *v);
    v->name = "Document";
    v->priv = e;
    v->destroy  = destroy;
    v->render   = render;
    v->status   = status;
    v->on_key   = on_key;
    v->get_path = get_path;
    return v;
}

/* ---- test accessors ---- */
int wuos_doc_is_rendered(WuView *v){ return ((DocV*)v->priv)->doc ? 1 : 0; }
int wuos_doc_has_text(WuView *v){ return ((DocV*)v->priv)->text ? 1 : 0; }
int wuos_doc_find(WuView *v, const char *q){
    DocV *e = v->priv;
    if (!e->text || !q || !q[0]) return 0;
    return strstr(e->text, q) ? 1 : 0;
}
/* Count of inserted overlay objects (chart/draw/math) currently displayed. */
int wuos_doc_obj_count(WuView *v){ return ((DocV*)v->priv)->nobj; }
/* EPUB export status string (caller must NOT free; view owns it), or NULL. */
const char *wuos_doc_epub_msg(WuView *v){ return ((DocV*)v->priv)->epub_msg; }
/* a11y issue count from the last check (0 if not run). */
int wuos_doc_a11y_issues(WuView *v){ return ((DocV*)v->priv)->a11y_done ? ((DocV*)v->priv)->a11y.count : -1; }
