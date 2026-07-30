/* view_doc.c -- Document view: renders a wubumodel page via wurender AND is
 * interactive. Given a path it ingests the real document through the wubudoc
 * facade (docx/odt/pdf/md/txt/html/...), and when the format isn't a renderable
 * page it shows the recognized text projection instead. Ctrl+F searches the
 * loaded text and jumps to the first match. The SAME render path the offscreen
 * PNG writer uses, so the in-shell Document tab is the real office surface. */
#include "wuos.h"
#include "wuos_file.h"
#include "wuos_font.h"    /* wuos_font_draw: text raster callback for svg_rasterize_cb */
#include "wuos_theme.h"
#include "wuburender.h"
#include "model.h"
#include "wubudoc.h"     /* doc_session_*, doc_open, doc_text, doc_drop_text */
#include "chart.h"       /* wubuchart: insert chart (INT-1) */
#include "draw.h"        /* wubudraw: insert shape (INT-3) */
#include "math.h"        /* wubumath: insert equation (INT-3) */
#include "a11y.h"        /* wubua11y: check (INT-5) */
#include "rast.h"        /* wubusvg rasterizer: SVG -> RGBA (gap #13) */
#include "ublayout.h"      /* central text pipeline: model -> laid-out pages */
#include "shape.h"        /* wubushape: Bidi per-run reorder for RTL */
#include "toc.h"          /* DOC-54: table-of-contents generator */
#include "settings.h"      /* UXA-41: high-contrast colors */
#include "script.h"        /* DOC-97: wubuscript computed fields */
#include "doccmd.h"        /* opaque document-editing command module */

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
    /* DOC-58: a paragraph-level style must inherit to its runs. If the run
     * itself has no style, fall back to the parent (paragraph) style. */
    wubumodel_style *st = wubumodel_node_style(n);
    if (!st && n) st = wubumodel_node_style(wubumodel_node_parent(n));
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
                 /* DOC-58: current paragraph index for style application */
                 int cur_para;
                 /* DOC-60: recorded link boxes (for click hit-testing) */
                 struct { int x, y, w, h; const char *target; } linkbox[32];
                 int nlink;
} DocV;

/* All document-editing commands now live in the opaque doccmd module
 * (doccmd.h/doccmd.c); this view delegates to them below in on_key. */

/* DOC-58: return the Nth top-level paragraph in the document body. */
static wubumodel_node *doc_nth_paragraph(DocV *e, int idx){
    if (!e->doc) return NULL;
    wubumodel_node *sec = wubumodel_node_first_child(wubumodel_doc_root(e->doc));
    if (!sec) return NULL;
    int i = 0;
    for (wubumodel_node *n = wubumodel_node_first_child(sec); n;
         n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n) == WUBUMODEL_PARAGRAPH){
            if (i == idx) return n;
            i++;
        }
    }
    return NULL;
}
/* DOC-58: apply a named style preset to the current paragraph. */
static void doc_apply_named_style(DocV *e, const char *name){
    if (!e->doc) return;
    wubumodel_node *p = doc_nth_paragraph(e, e->cur_para);
    if (!p) return;
    wubumodel_node_apply_named_style(p, name);
    e->toc_dirty = 1;
}
/* DOC-58: move the current-paragraph cursor (for the style picker target). */
static void doc_move_para(DocV *e, int dx){
    if (!e->doc) return;
    int n = 0;
    wubumodel_node *sec = wubumodel_node_first_child(wubumodel_doc_root(e->doc));
    if (!sec) return;
    for (wubumodel_node *m = wubumodel_node_first_child(sec); m;
         m = wubumodel_node_next_sibling(m))
        if (wubumodel_node_kind(m) == WUBUMODEL_PARAGRAPH) n++;
    if (n == 0) return;
    e->cur_para += dx;
    if (e->cur_para < 0) e->cur_para = 0;
    if (e->cur_para >= n) e->cur_para = n - 1;
}

/* DOC-45: UI chrome font size scaled by the user's UI-scale setting
 * (independent of document zoom). Bounded to a sane range. */
static int doc_chrome_fs(DocV *e, int base){
    double us = 1.0;
    WubuSettings *sh = wubusettings_shared();
    if (sh) us = wubusettings_ui_scale(sh);
    int fs = (int)(base * us + 0.5);
    if (fs < 8) fs = 8;
    if (fs > 64) fs = 64;
    (void)e;
    return fs;
}

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    (void)scroll;
    DocV *e = v->priv;
    int dark = wubusettings_dark(wubusettings_shared());
    WuosRGB doc_bg = dark ? WUOS_DARK(TAB_BAR) : WUOS_LIGHT(TAB_BAR);
    WuosRGB doc_body = dark ? WUOS_DARK(TABTEXT_ON) : WUOS_LIGHT(TABTEXT_ON);
    WuosRGB doc_hint = dark ? WUOS_DARK(TABTEXT)   : WUOS_LIGHT(TABTEXT);
    unsigned char *fb = NULL; int W=w, H=h;
    e->nlink = 0;
    if (e->doc){
        /* Central pipeline: lay the model out into pages, then paint. This is
         * the single source of truth for text placement — the Document tab no
         * longer re-implements wrapping. Headers/footers/line-numbers are drawn
         * from the laid-out geometry. */
        fb = malloc((size_t)w*h*4);
        if (!fb) return -1;
        for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=doc_bg.r;fb[k+1]=doc_bg.g;fb[k+2]=doc_bg.b;fb[k+3]=255; }
        wubulayout_doc *L = wubulayout_create(e->doc, NULL,
            doc_layout_measure, doc_layout_style, NULL, w, h, 56,56,56,56);
        if (L){
            int pages = wubulayout_page_count(L);
            int pg = e->jump_page; e->jump_page = -1;
            if (pg < 0) pg = scroll / (h - 112);
            if (pg<0) pg=0;
            if (pg>=pages) pg=pages-1;
            /* UXA-41: high-contrast swaps to maximal-distinction palette */
            int hc = wubusettings_high_contrast(wubusettings_shared());
            unsigned char BG[3] = { hc?255:252, hc?255:252, hc?255:250 };
            unsigned char FG[3] = { hc?0:28,   hc?0:30,   hc?0:34 };
            unsigned char MU[3] = { hc?0:120, hc?0:124, hc?0:132 };
            (void)BG;  /* page background override; retained for future use */
            unsigned char LN[3] = { hc?127:150, hc?127:154, hc?127:162 };
            unsigned char TT[3] = { hc?0:200, hc?120:203, hc?200:210 };
            /* page header (DOC-56: use model HEADER if present) */
            char hdr[128];
            const char *hm = NULL, *fm = NULL;
            if (e->doc){
                for (wubumodel_node *n = wubumodel_node_first_child(
                            wubumodel_doc_root(e->doc)); n; n = wubumodel_node_next_sibling(n)){
                    wubumodel_kind k = wubumodel_node_kind(n);
                    if (k==WUBUMODEL_HEADER && !hm) hm = wubumodel_node_text(n);
                    else if (k==WUBUMODEL_FOOTER && !fm) fm = wubumodel_node_text(n);
                }
            }
            snprintf(hdr,sizeof hdr,"%s   (Page %d / %d)",
                     hm?hm:"WuBuOffice", pg+1, pages);
            wuos_font_draw_s(hdr, 56, 24, 0, doc_chrome_fs(e,12), MU[0],MU[1],MU[2], fb, w, h);
            /* DOC-62/61: draw table borders + blit embedded images from boxes */
            int nboxes = wubulayout_box_count(L, pg);
            for (int bi=0; bi<nboxes; bi++){
                const wubulayout_box *bx = wubulayout_box_at(L, pg, bi);
                if (!bx) continue;
                /* DOC-61: an IMAGE node -> blit its RGBA into the box */
                if (bx->user && wubumodel_node_kind((wubumodel_node*)bx->user) == WUBUMODEL_IMAGE){
                    int iw=0, ih=0;
                    const uint8_t *px = wubumodel_node_image((const wubumodel_node*)bx->user, &iw, &ih);
                    if (px && iw>0 && ih>0){
                        int dw = bx->w, dh = bx->h;
                        for (int yy=0; yy<dh; yy++){
                            int sy = (iw>0)? (yy*dh<ih*dw ? yy*ih/dh : ih-1) : 0;
                            if (sy<0) sy=0;
                            if (sy>=ih) sy=ih-1;
                            for (int xx=0; xx<dw; xx++){
                                int sx = (iw>0)? (xx*dw<iw*dh ? xx*iw/dw : iw-1) : 0;
                                if (sx<0) sx=0;
                                if (sx>=iw) sx=iw-1;
                                const uint8_t *p = px + ((size_t)sy*iw+sx)*4;
                                int dx = bx->x + xx, dy = bx->y + yy;
                                if (dx<0||dy<0||dx>=w||dy>=h) continue;
                                size_t di=((size_t)dy*w+dx)*4;
                                /* alpha blend over page bg */
                                float a = p[3]/255.0f;
                                fb[di]   = (uint8_t)(fb[di]*(1-a) + p[0]*a);
                                fb[di+1] = (uint8_t)(fb[di+1]*(1-a) + p[1]*a);
                                fb[di+2] = (uint8_t)(fb[di+2]*(1-a) + p[2]*a);
                            }
                        }
                        continue;
                    }
                }
                /* box border: 1px rect in a muted ink */
                unsigned char bc[3] = { hc?90:170, hc?120:175, hc?150:185 };
                for (int xx=bx->x; xx<bx->x+bx->w && xx<w; xx++)
                    for (int yy=bx->y; yy<bx->y+bx->h && yy<h; yy++){
                        if (xx==bx->x || xx==bx->x+bx->w-1 ||
                            yy==bx->y || yy==bx->y+bx->h-1){
                            size_t di=((size_t)yy*w+xx)*4;
                            fb[di]=bc[0]; fb[di+1]=bc[1]; fb[di+2]=bc[2];
                        }
                    }
            }
            int nr = wubulayout_run_count(L, pg);
            for (int i=0;i<nr;i++){
                const wubulayout_run *r = wubulayout_run_at(L, pg, i);
                if (!r || !r->text || !r->text_len) continue;
                char seg[2048]; size_t l=r->text_len; if(l>=sizeof seg)l=sizeof seg-1;
                /* INT-7: RTL/BIDI reorder per run. layout.c sets r->rtl
                 * when the dominant direction of this run is RTL. */
                const char *draw_text = r->text;
                if (r->rtl && l < sizeof seg - 1) {
                    char vis[2048];
                    shape_reorder(r->text, SHAPE_RTL, vis, sizeof vis);
                    draw_text = vis;
                    l = strlen(vis);
                }
                memcpy(seg, draw_text, l); seg[l]=0;
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
                /* DOC-63/64/65: color review/field runs by node metadata */
                int is_comment=0, is_tc_ins=0, is_tc_del=0, is_field=0;
                unsigned char cm[3] = { hc?0:30, hc?120:120, hc?255:200 };   /* comment: blue */
                unsigned char ti[3] = { hc?0:20, hc?160:150, hc?0:40 };       /* insert: green */
                unsigned char td[3] = { hc?220:200, hc?40:30, hc?40:30 };     /* delete: red */
                unsigned char fd[3] = { hc?160:120, hc?80:80, hc?200:200 };   /* field: purple */
                if (r->user){
                    wubumodel_node *rn = (wubumodel_node*)r->user;
                    wubumodel_kind rk = wubumodel_node_kind(rn);
                    if (rk == WUBUMODEL_COMMENT){ is_comment=1; lk[0]=cm[0];lk[1]=cm[1];lk[2]=cm[2]; }
                    else if (rk == WUBUMODEL_TRACKCHANGE){
                        is_tc_del = (wubumodel_node_tc(rn)==1);
                        is_tc_ins = !is_tc_del;
                        if (is_tc_del){ lk[0]=td[0];lk[1]=td[1];lk[2]=td[2]; }
                        else { lk[0]=ti[0];lk[1]=ti[1];lk[2]=ti[2]; }
                    } else if (rk == WUBUMODEL_FIELD){ is_field=1; lk[0]=fd[0];lk[1]=fd[1];lk[2]=fd[2]; }
                }
                /* Markers retained for downstream consumers (a11y checks etc.)
                 * that may consult view-internal state; here they only feed lk[]. */
                (void)is_comment; (void)is_tc_ins; (void)is_tc_del; (void)is_field;
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
            /* page footer (DOC-56: use model FOOTER if present) */
            char ftr[128];
            const char *ft = NULL;
            if (e->doc){
                for (wubumodel_node *n = wubumodel_node_first_child(
                            wubumodel_doc_root(e->doc)); n; n = wubumodel_node_next_sibling(n)){
                    if (wubumodel_node_kind(n)==WUBUMODEL_FOOTER){ ft = wubumodel_node_text(n); break; }
                }
            }
            snprintf(ftr,sizeof ftr,"%s   — %d lines", ft?ft:"WuBuOffice", nl);
            wuos_font_draw_s(ftr, 56, h-30, 0, doc_chrome_fs(e,12), MU[0],MU[1],MU[2], fb, w, h);
            /* UI-34: minimap on the right edge — one tick per line, colored by
             * heading level if the TOC knows about it. */
            int mmx = w - 10;
            for (int li=0; li<nl; li++){
                const wubulayout_line *ln = wubulayout_line_at(L, pg, li);
                if (!ln) continue;
                int my = 56 + (int)((double)(ln->y) / (h) * (h-120));
                if (my < 0) my = 0;
                if (my >= h) my = h-1;
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
        for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=doc_bg.r;fb[k+1]=doc_bg.g;fb[k+2]=doc_bg.b;fb[k+3]=255; }
        wuos_font_draw("Document text (recognized):", WUOS_SPACE_8*2, WUOS_SPACE_8*2, 1, doc_body.r,doc_body.g,doc_body.b, fb,w,h);
        int y = WUOS_SPACE_8*6;
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
                wuos_font_draw(tmp, x, y, 0, doc_body.r,doc_body.g,doc_body.b, fb,w,h);
                x += (int)wl*8 + 8;
                wstart = sp ? sp+1 : wstart+wl;
                if (y > h-30) break;
            }
            if (e->find_q && e->find_q[0]){
                int fy = h-26;
                for (int xx=0; xx<w; xx++) for(int yy=fy; yy<h; yy++)
                    if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t i=((size_t)yy*w+xx)*4; fb[i]=30;fb[i+1]=33;fb[i+2]=40; }
                char line[256]; snprintf(line,sizeof line,"find '%s': %s", e->find_q, e->find_hit?"1 match highlighted in text":"no match");
                wuos_font_draw(line, WUOS_SPACE_8, fy+5, 0, doc_body.r,doc_body.g,doc_body.b, fb,w,h);
            }
        } else {
            wuos_font_draw("(nothing to display)", WUOS_SPACE_8*2, WUOS_SPACE_8*6, 0, doc_hint.r,doc_hint.g,doc_hint.b, fb,w,h);
        }
    }
    /* ---- inserted objects overlay (chart/draw/math) on the right gutter ---- */
    if (e->nobj){
        int ox = W - 340; if (ox < 8) ox = 8;
        wuos_font_draw("Inserted objects:", ox, 16, 1, 30,34,42, fb, W, H);
        int oy = 40;
        for (int i=0;i<e->nobj;i++){
            int pw = e->objw[i], ph = e->objh[i];
            if (ox+pw > W) pw = W-ox;
            if (oy+ph > H) ph = H-oy;
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
    /* DOC-46: report a11y issues inline (list the actual findings, not just a
     * count). Drawn as a translucent panel stacked above the status footer. */
    if (e->a11y_done && e->a11y.count > 0){
        int line_h = 16, maxlines = 8;
        int n = e->a11y.count < maxlines ? e->a11y.count : maxlines;
        int ph = 10 + n*line_h + (e->a11y.count>maxlines? line_h:0);
        int px = 8, py = H - 26 - ph, pw = 460;
        /* panel background */
        for (int yy=py; yy<py+ph && yy<H; yy++)
            for (int xx=px; xx<px+pw && xx<W; xx++){
                if (xx<0||yy<0) continue;
                size_t di=((size_t)yy*W+xx)*4;
                fb[di]=28; fb[di+1]=26; fb[di+2]=34;
            }
        for (int i=0;i<n;i++){
            const char *it = e->a11y.items[i] ? e->a11y.items[i] : "(issue)";
            wuos_font_draw(it, px+8, py+6+i*line_h, 0, 255,180,120, fb, W, H);
        }
        if (e->a11y.count > maxlines){
            char more[32]; snprintf(more,sizeof more,"... +%d more", e->a11y.count-maxlines);
            wuos_font_draw(more, px+8, py+6+n*line_h, 0, 200,200,210, fb, W, H);
        }
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
    if (key==WUOS_KEY_INSERT_CHART){
        if (e->nobj < DOC_MAX_OBJS){ int w=0,h=0; unsigned char *fb=doccmd_insert_chart(wuos_svg_text,&w,&h);
            if (fb){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; } }
        return; }
    if (key==WUOS_KEY_INSERT_DRAW){
        if (e->nobj < DOC_MAX_OBJS){ int w=0,h=0; unsigned char *fb=doccmd_insert_draw(wuos_svg_text,&w,&h);
            if (fb){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; } }
        return; }
    if (key==WUOS_KEY_INSERT_MATH){
        if (e->nobj < DOC_MAX_OBJS){ int w=0,h=0; unsigned char *fb=doccmd_insert_math(wuos_svg_text,&w,&h);
            if (fb){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; } }
        return; }
    if (key==WUOS_KEY_EXPORT_EPUB){ free(e->epub_msg); e->epub_msg = doccmd_export_epub(e->doc, "/tmp/wubuos_export.epub"); return; }
    if (key==WUOS_KEY_EXPORT_PDF){      free(e->epub_msg); e->epub_msg = doccmd_export_pdf     (e->doc); return; }
    if (key==WUOS_KEY_EXPORT_HTML){     free(e->epub_msg); e->epub_msg = doccmd_export_html    (e->doc); return; }
    if (key==WUOS_KEY_EXPORT_MARKDOWN){ free(e->epub_msg); e->epub_msg = doccmd_export_markdown(e->doc); return; }
    if (key==WUOS_KEY_EXPORT_LATEX){    free(e->epub_msg); e->epub_msg = doccmd_export_latex   (e->doc); return; }
    if (key==WUOS_KEY_EXPORT_RTF){      free(e->epub_msg); e->epub_msg = doccmd_export_rtf     (e->doc); return; }
    if (key==WUOS_KEY_SAVE){ free(e->epub_msg); e->epub_msg = doccmd_save(e->doc, e->path); return; }
    if (key==WUOS_KEY_A11Y_CHECK){ doccmd_a11y_check(e->doc, &e->a11y); e->a11y_done = 1; return; }
    if (key==WUOS_KEY_INSERT_LINK){ if (doccmd_insert_link(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_LIST){ if (doccmd_insert_list(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_TABLE){ if (doccmd_insert_table(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_IMAGE){ if (doccmd_insert_image(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_PAGEBREAK){ if (doccmd_insert_pagebreak(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_SECTIONBREAK){ if (doccmd_insert_sectionbreak(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_HEADER){ if (doccmd_insert_header(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_FOOTER){ if (doccmd_insert_footer(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_COMMENT){ if (doccmd_insert_comment(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_TRACKCHANGE){ if (doccmd_insert_trackchange(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_FIELD){ if (doccmd_insert_field(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_SCRIPT){ if (doccmd_insert_script_field(e->doc, "lines * 2")) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_STYLE_H1){ doc_apply_named_style(e, "Heading1"); return; }
    if (key==WUOS_KEY_STYLE_H2){ doc_apply_named_style(e, "Heading2"); return; }
    if (key==WUOS_KEY_STYLE_H3){ doc_apply_named_style(e, "Heading3"); return; }
    if (key==WUOS_KEY_STYLE_BODY){ doc_apply_named_style(e, "Body"); return; }
    if (key==WUOS_KEY_STYLE_QUOTE){ doc_apply_named_style(e, "Quote"); return; }
    if (key==WUOS_KEY_STYLE_CODE){ doc_apply_named_style(e, "Code"); return; }
    if (key==WUOS_KEY_PARA_PREV){ doc_move_para(e, -1); return; }
    if (key==WUOS_KEY_PARA_NEXT){ doc_move_para(e, +1); return; }
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
    if (e->r) wurender_destroy(e->r);
    free(e);
    free(v);
}

/* Thin view-level save hook: delegate to the doccmd module (DOC-76). */
static void doc_save(WuView *v){
    DocV *e = v ? v->priv : NULL;
    if (!e) return;
    free(e->epub_msg); e->epub_msg = doccmd_save(e->doc, e->path);
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
            /* DOC-76: open Word files into a real, round-trippable model so
             * save-as-DOCX preserves structure (headings/paragraphs/runs). */
            int is_docx = (strstr(path,".docx")!=NULL||strstr(path,".docm")!=NULL||strstr(path,".dotx")!=NULL);
            int is_odt  = (strstr(path,".odt")!=NULL||strstr(path,".fodt")!=NULL);
            int is_rtf  = (strstr(path,".rtf")!=NULL);
            int is_epub = (strstr(path,".epub")!=NULL||strstr(path,".epub3")!=NULL);
            if (is_docx){
                wubumodel_doc *md = NULL;
                if (wubumodel_load_docx(path, &md) == 0 && md) e->doc = md;
            } else if (is_odt){
                wubumodel_doc *md = NULL;
                if (wubumodel_load_odt(path, &md) == 0 && md) e->doc = md;
            } else if (is_rtf){
                wubumodel_doc *md = NULL;
                if (wubumodel_load_rtf(path, &md) == 0 && md) e->doc = md;
            } else if (is_epub){
                wubumodel_doc *md = NULL;
                if (wubumodel_load_epub(path, &md) == 0 && md) e->doc = md;
            }
        }
        if (!e->doc){
            /* ingest via the real document facade (odt/pdf/html/...) */
            DocSession *s = doc_session_create();
            long id = doc_open(s, path);
            if (id >= 0){
                const char *dt = doc_text(s, id);
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
    /* INT-1/3: seed one sample chart so the Insert path is exercised + visible. */
    { int w=0,h=0; unsigned char *fb=doccmd_insert_chart(wuos_svg_text,&w,&h);
      if (fb){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; } }
    WuView *v = calloc(1, sizeof *v);
    v->name = "Document";
    v->priv = e;
    v->destroy  = destroy;
    v->render   = render;
    v->status   = status;
    v->on_key   = on_key;
    v->on_click = on_click;
    v->get_path = get_path;
    v->save    = doc_save;   /* DOC-76: Ctrl+S writes DOCX round-trip */
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
wubumodel_doc *wuos_doc_model(WuView *v){ return v ? ((DocV*)v->priv)->doc : NULL; }
/* Count of inserted overlay objects (chart/draw/math) currently displayed. */
int wuos_doc_obj_count(WuView *v){ return ((DocV*)v->priv)->nobj; }
/* EPUB export status string (caller must NOT free; view owns it), or NULL. */
const char *wuos_doc_epub_msg(WuView *v){ return ((DocV*)v->priv)->epub_msg; }
/* a11y issue count from the last check (0 if not run). */
int wuos_doc_a11y_issues(WuView *v){ return ((DocV*)v->priv)->a11y_done ? ((DocV*)v->priv)->a11y.count : -1; }
const char *wuos_doc_a11y_item(WuView *v, int i){
    DocV *e = (DocV*)v->priv;
    if (!e->a11y_done || i<0 || i>=e->a11y.count) return NULL;
    return e->a11y.items[i];
}
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
/* Arg-bearing inserts driven by the shell's modal dialogs. */
int wuos_doc_insert_link_url(WuView *v, const char *url){
    DocV *e = v ? (DocV*)v->priv : NULL;
    if (!e || !e->doc) return 0;
    int ok = doccmd_insert_link_url(e->doc, url);
    if (ok) e->toc_dirty = 1;
    return ok;
}
int wuos_doc_insert_image_alt(WuView *v, const char *alt){
    DocV *e = v ? (DocV*)v->priv : NULL;
    if (!e || !e->doc) return 0;
    int ok = doccmd_insert_image_alt(e->doc, alt);
    if (ok) e->toc_dirty = 1;
    return ok;
}
int wuos_doc_insert_qr(WuView *v, const char *text){
    DocV *e = v ? (DocV*)v->priv : NULL;
    if (!e || !e->doc) return 0;
    int ok = doccmd_insert_qr(e->doc, text);
    if (ok) e->toc_dirty = 1;
    return ok;
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
