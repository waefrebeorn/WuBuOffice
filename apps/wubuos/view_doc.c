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
#include "toc.h"          /* DOC-54: table-of-contents generator */
#include "settings.h"      /* UXA-41: high-contrast colors */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* forward declarations (defined below) */
static int wuos_doc_footnote_count(WuView *v);
int epub_write(wubumodel_doc *doc, const char *out, const char *title, const char *lang);

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
                 int toc_dirty;       /* TOC needs rebuild */
                 Toc *toc;            /* DOC-54 side pane */
                 int jump_page;        /* pending TOC jump (set by on_key) */
                 /* DOC-60: recorded link boxes (for click hit-testing) */
                 struct { int x, y, w, h; const char *target; } linkbox[32];
                 int nlink;
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
/* Insert a hyperlink into the model (DOC-60): a LINK node whose RUN children
 * are the visible text and whose target is set via the model accessor. */
static void doc_insert_link(DocV *e){
    if (!e->doc) return;
    wubumodel_node *sec = wubumodel_node_first_child(wubumodel_doc_root(e->doc));
    if (!sec) return;
    wubumodel_node *lk = wubumodel_node_create(e->doc, WUBUMODEL_LINK);
    wubumodel_node *r = wubumodel_node_create(e->doc, WUBUMODEL_RUN);
    wubumodel_run_set_text(r, "WuBuOffice");
    wubumodel_node_append(e->doc, lk, r);
    wubumodel_node_set_link(lk, "https://github.com/waefrebeorn/WuBuOffice");
    wubumodel_node_append(e->doc, sec, lk);
    e->toc_dirty = 1;
}
/* Insert a bullet-list item into the model (DOC-59): a paragraph styled
 * list=bullet whose RUN is the item text. */
static void doc_insert_list(DocV *e){
    if (!e->doc) return;
    wubumodel_node *sec = wubumodel_node_first_child(wubumodel_doc_root(e->doc));
    if (!sec) return;
    wubumodel_node *p = wubumodel_node_create(e->doc, WUBUMODEL_PARAGRAPH);
    wubumodel_style *st = wubumodel_style_create();
    wubumodel_style_set_prop(st, "list", "bullet");
    wubumodel_node_set_style(p, st);
    wubumodel_style_destroy(st);
    wubumodel_node *r = wubumodel_node_create(e->doc, WUBUMODEL_RUN);
    wubumodel_run_set_text(r, "List item");
    wubumodel_node_append(e->doc, p, r);
    wubumodel_node_append(e->doc, sec, p);
    e->toc_dirty = 1;
}

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    DocV *e = v->priv;
    (void)scroll;
    unsigned char *fb = NULL; int W=w, H=h;
    e->nlink = 0;
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
            int pg = e->jump_page; e->jump_page = -1;
            if (pg < 0) pg = scroll / (h - 112);
            if (pg<0) pg=0; if (pg>=pages) pg=pages-1;
            /* UXA-41: high-contrast swaps to maximal-distinction palette */
            int hc = wubusettings_high_contrast(wubusettings_shared());
            unsigned char BG[3] = { hc?255:252, hc?255:252, hc?255:250 };
            unsigned char FG[3] = { hc?0:28,   hc?0:30,   hc?0:34 };
            unsigned char MU[3] = { hc?0:120, hc?0:124, hc?0:132 };
            unsigned char LN[3] = { hc?127:150, hc?127:154, hc?127:162 };
            unsigned char TT[3] = { hc?0:200, hc?120:203, hc?200:210 };
            /* page header */
            char hdr[64]; snprintf(hdr,sizeof hdr,"Page %d / %d", pg+1, pages);
            wuos_font_draw(hdr, 56, 24, 0, MU[0],MU[1],MU[2], fb, w, h);
            int nr = wubulayout_run_count(L, pg);
            for (int i=0;i<nr;i++){
                const wubulayout_run *r = wubulayout_run_at(L, pg, i);
                if (!r || !r->text || !r->text_len) continue;
                char seg[2048]; size_t l=r->text_len; if(l>=sizeof seg)l=sizeof seg-1;
                memcpy(seg, r->text, l); seg[l]=0;
                /* DOC-60: links are blue + underlined; record box for clicks */
                int is_link = 0; unsigned char lk[3];
                lk[0]=hc?0:24; lk[1]=hc?80:64; lk[2]=hc?220:200;
                const char *tgt = NULL;
                if (r->user){
                    wubumodel_node *rn = (wubumodel_node*)r->user;
                    /* the run's owner (or the run itself) may carry the link */
                    const char *a = wubumodel_node_link(rn);
                    if (!a && wubumodel_node_kind(rn) != WUBUMODEL_LINK){
                        wubumodel_node *par = wubumodel_node_parent(rn);
                        if (par && wubumodel_node_kind(par) == WUBUMODEL_LINK)
                            a = wubumodel_node_link(par);
                    }
                    if (a){ is_link = 1; tgt = a; }
                    if (is_link && e->nlink < 32){
                        e->linkbox[e->nlink].x = r->x;
                        e->linkbox[e->nlink].y = r->y - 14;
                        e->linkbox[e->nlink].w = r->w;
                        e->linkbox[e->nlink].h = 18;
                        e->linkbox[e->nlink].target = tgt;
                        e->nlink++;
                    }
                }
                wuos_font_draw(seg, r->x, r->y, r->bold, lk[0],lk[1],lk[2], fb, w, h);
                if (is_link){ /* underline */
                    int yy = r->y + 2;
                    if (yy < h) for (int xx=r->x; xx<r->x+r->w && xx<w; xx++){
                        size_t di=((size_t)yy*w+xx)*4; fb[di]=lk[0];fb[di+1]=lk[1];fb[di+2]=lk[2];
                    }
                }
            }
            /* line numbers in the left margin (DOC-72) */
            int nl = wubulayout_line_count(L, pg);
            for (int li=0; li<nl; li++){
                const wubulayout_line *ln = wubulayout_line_at(L, pg, li);
                if (!ln) continue;
                char ln2[16]; snprintf(ln2,sizeof ln2,"%d", li+1);
                wuos_font_draw(ln2, 8, ln->y, 0, LN[0],LN[1],LN[2], fb, w, h);
            }
            /* page footer */
            char ftr[64]; snprintf(ftr,sizeof ftr,"WuBuOffice — %d lines", nl);
            wuos_font_draw(ftr, 56, h-30, 0, MU[0],MU[1],MU[2], fb, w, h);
            /* UI-34: minimap on the right edge — one tick per line, colored by
             * heading level if the TOC knows about it. */
            int mmx = w - 10;
            int span = (h - 112);
            for (int li=0; li<nl; li++){
                const wubulayout_line *ln = wubulayout_line_at(L, pg, li);
                if (!ln) continue;
                int my = 56 + (int)((double)(ln->y) / (h) * (h-120));
                if (my < 0) my = 0; if (my >= h) my = h-1;
                if (mmx >= 0 && mmx < w){
                    fb[((size_t)my*w+mmx)*4]   = MU[0];
                    fb[((size_t)my*w+mmx)*4+1] = MU[1];
                    fb[((size_t)my*w+mmx)*4+2] = MU[2];
                }
            }
            /* DOC-54: TOC side pane (left, below line numbers) */
            if (e->toc_dirty || !e->toc){
                toc_free(e->toc); e->toc = toc_build(e->doc, NULL, L);
                e->toc_dirty = 0;
            }
            int tcount = toc_count(e->toc);
            if (tcount){
                int tx = 60, ty = 56;
                wuos_font_draw("Contents", tx, ty-20, 1, FG[0],FG[1],FG[2], fb, w, h);
                for (int i=0;i<tcount;i++){
                    const char *tt = toc_title(e->toc, i);
                    if (!tt) continue;
                    int lvl = toc_level(e->toc, i);
                    int pgno = toc_page(e->toc, i);
                    char te[128];
                    snprintf(te,sizeof te,"%*s%s%s", (lvl-1)*2, "", tt,
                             pgno>0? "":"");
                    unsigned char r=TT[0],g=TT[1],b=TT[2];
                    if (lvl==1){ r=hc?0:40; g=hc?90:120; b=hc?200:60; }
                    wuos_font_draw(te, tx + (lvl-1)*10, ty, 0, r,g,b, fb, w, h);
                    ty += 18;
                    if (ty > h-40) break;
                }
            }
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
    /* UI-35: breadcrumb / location bar over the loaded path */
    const char *src = e->path ? e->path : "sample";
    char *s = malloc(160); if(!s) return NULL;
    const char *base = strrchr(src, '/');
    base = base ? base+1 : src;
    int notes = wuos_doc_footnote_count(v);
    if (e->doc){
        if (notes > 0)
            snprintf(s,160,"Document ▸ %s ▸ rendered page ▸ %d note(s)", base, notes);
        else
            snprintf(s,160,"Document ▸ %s ▸ rendered page", base);
    }
    else snprintf(s,160,"Document ▸ %s ▸ %s text", base, e->text?"recognized":"no");
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
    if (key==WUOS_KEY_INSERT_LINK){ doc_insert_link(e); return; }
    if (key==WUOS_KEY_INSERT_LIST){ doc_insert_list(e); return; }
    /* DOC-54: jump to a TOC entry with Ctrl+[1..6] */
    if (key>=WUOS_KEY_TOC1 && key<=WUOS_KEY_TOC6){
        int idx = key - WUOS_KEY_TOC1;
        if (e->toc && idx < toc_count(e->toc)){
            int pg = toc_page(e->toc, idx);
            if (pg>0) e->jump_page = pg-1;
        }
        return;
    }
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

static void on_click(WuView *v, int x, int y){
    DocV *e = v->priv;
    /* DOC-60: hit-test the recorded link boxes; the first hit "opens" the
     * target (we surface it in the status / a toast via the epub_msg slot). */
    for (int i=0;i<e->nlink;i++){
        int bx=e->linkbox[i].x, by=e->linkbox[i].y;
        int bw=e->linkbox[i].w, bh=e->linkbox[i].h;
        if (x>=bx && x<=bx+bw && y>=by && y<=by+bh){
            free(e->epub_msg);
            const char *t = e->linkbox[i].target;
            size_t L = t?strlen(t):0;
            char *m = malloc(L+24); if (m){ snprintf(m, L+24, "link -> %s", t?t:""); }
            e->epub_msg = m;
            return;
        }
    }
}

static void destroy(WuView *v){
    DocV *e = v->priv;
    if (e->doc)  wubumodel_doc_destroy(e->doc);
    if (e->text) free(e->text);
    if (e->path) free(e->path);
    if (e->find_q) free(e->find_q);
    if (e->epub_msg) free(e->epub_msg);
    for (int i=0;i<e->nobj;i++) free(e->obj[i]);
    if (e->a11y_done) a11y_report_free(&e->a11y);
    toc_free(e->toc);
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
    e->toc = NULL; e->toc_dirty = 1; e->jump_page = -1;
    doc_insert_chart(e);
    WuView *v = calloc(1, sizeof *v);
    v->name = "Document";
    v->priv = e;
    v->destroy  = destroy;
    v->render   = render;
    v->status   = status;
    v->on_key   = on_key;
    v->on_click = on_click;
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
/* TOC entry count for the current render (DOC-54), or -1 if no model. */
int wuos_doc_toc_count(WuView *v){
    DocV *e = v->priv;
    if (!e->doc) return -1;
    if (!e->toc) e->toc = toc_build(e->doc, NULL, NULL);
    return toc_count(e->toc);
}
/* High-contrast setting (UXA-41) read from shared settings. */
int wuos_doc_high_contrast(WuView *v){
    (void)v;
    WubuSettings *s = wubusettings_shared();
    return s ? wubusettings_high_contrast(s) : 0;
}
/* Footnote/endnote count for the current model (DOC-55), or -1 if no model. */
int wuos_doc_footnote_count(WuView *v){
    DocV *e = v->priv;
    if (!e->doc) return -1;
    const char **out = NULL;
    int n = wubumodel_doc_notes(e->doc, &out);
    free(out);
    return n;
}
