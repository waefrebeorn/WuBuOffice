/* view_doc_render.c -- document view rendering (page layout, objects,
 * link boxes, TOC pane), split from view_doc.c. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "view_doc_internal.h"
#include "wuos_font.h"
#include "wuburender.h"
#include "model.h"
#include "wubudoc.h"
#include "rast.h"
#include "ublayout.h"
#include "shape.h"
#include "settings.h"
#include "wuos_theme.h"
#include "ublayout.h"
#include "model.h"
#include "wuos_font.h"

/* metric callbacks for wubulayout (app font behind function pointers) */
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

static wubumodel_node *nth_para_in(wubumodel_node *n, int *i, int idx){
    for (; n; n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n) == WUBUMODEL_PARAGRAPH){
            if (*i == idx) return n;
            (*i)++;
        } else {
            wubumodel_node *hit = nth_para_in(
                wubumodel_node_first_child(n), i, idx);
            if (hit) return hit;
        }
    }
    return NULL;
}
wubumodel_node * doc_nth_paragraph(DocV *e, int idx){
    if (!e->doc || idx < 0) return NULL;
    /* walk all sections/blocks recursively (markdown nests paras in blocks) */
    int i = 0;
    for (wubumodel_node *sec = wubumodel_node_first_child(wubumodel_doc_root(e->doc));
         sec; sec = wubumodel_node_next_sibling(sec)){
        wubumodel_node *hit = nth_para_in(sec, &i, idx);
        if (hit) return hit;
    }
    return NULL;
}
/* DOC-58: apply a named style preset to the current paragraph. */
void doc_apply_named_style(DocV *e, const char *name){
    if (!e->doc) return;
    wubumodel_node *p = doc_nth_paragraph(e, e->cur_para);
    if (!p) return;
    wubumodel_node_apply_named_style(p, name);
    e->toc_dirty = 1;
}
/* N3: toggle a direct character prop (bold/italic) on all runs of the
 * current paragraph. Clone-on-write: each run gets its own style copy with
 * the prop flipped; runs with no style get a fresh one. This is exactly the
 * surface cnfStyle resolves INTO (H6d), so table cells styled by cnf flags
 * can be overridden here too. */
void doc_toggle_run_prop(DocV *e, const char *prop){
    if (!e->doc) return;
    wubumodel_node *p = doc_nth_paragraph(e, e->cur_para);
    if (!p) return;
    int any_on = 0, nruns = 0;
    for (wubumodel_node *n = wubumodel_node_first_child(p); n;
         n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n) != WUBUMODEL_RUN) continue;
        nruns++;
        const wubumodel_style *st = wubumodel_node_style(n);
        if (st && wubumodel_style_get_prop(st, prop) &&
            !strcmp(wubumodel_style_get_prop(st, prop), "1")) any_on++;
    }
    int turn_on = (nruns == 0 || any_on < nruns);  /* on if any run lacks it */
    for (wubumodel_node *n = wubumodel_node_first_child(p); n;
         n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n) != WUBUMODEL_RUN) continue;
        wubumodel_style *ns = wubumodel_style_create();
        const wubumodel_style *old = wubumodel_node_style(n);
        if (old){
            /* copy existing props (prop_at fails past the end) */
            for (int i = 0;; i++){
                const char *nm = NULL, *vl = NULL;
                if (wubumodel_style_prop_at(old, i, &nm, &vl) == 0) break;
                if (nm && vl) wubumodel_style_set_prop(ns, nm, vl);
            }
        }
        wubumodel_style_set_prop(ns, prop, turn_on ? "1" : "0");
        wubumodel_node_set_style(n, ns);   /* node owns the ref */
        /* NOTE: no destroy -- set_style transfers ownership (refcount++) */
    }
    e->toc_dirty = 1;
}
/* DOC-58: move the current-paragraph cursor (for the style picker target). */
void doc_move_para(DocV *e, int dx){
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
int doc_chrome_fs(DocV *e, int base){
    double us = 1.0;
    WubuSettings *sh = wubusettings_shared();
    if (sh) us = wubusettings_ui_scale(sh);
    int fs = (int)(base * us + 0.5);
    if (fs < 8) fs = 8;
    if (fs > 64) fs = 64;
    (void)e;
    return fs;
}

int doc_render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    (void)scroll;
    DocV *e = v->priv;
    int dark = wubusettings_dark(wubusettings_shared());
    /* The DOCUMENT PAGE is a paper surface — NOT the app chrome color. Office
     * renders a distinct page sheet (white in light mode, light-slate in dark)
     * with margins, so text is always high-contrast ink on paper. */
    WuosRGB paper   = dark ? (WuosRGB){250,251,253} : (WuosRGB){255,255,255};
    WuosRGB ink     = dark ? (WuosRGB){ 28, 30, 34}  : (WuosRGB){ 32, 33, 37};
    WuosRGB sheet_edge = dark ? (WuosRGB){210,214,220} : (WuosRGB){210,214,220};
    WuosRGB app_bg  = dark ? WUOS_DARK(TAB_BAR) : WUOS_LIGHT(TAB_BAR);
    unsigned char *fb = NULL; int W=w, H=h;
    e->nlink = 0;
    if (e->doc){
        /* Central pipeline: lay the model out into pages, then paint. This is
         * the single source of truth for text placement. */
        fb = malloc((size_t)w*h*4);
        if (!fb) return -1;
        /* app chrome backdrop behind the page sheet */
        for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=app_bg.r;fb[k+1]=app_bg.g;fb[k+2]=app_bg.b;fb[k+3]=255; }
        /* page sheet: margins + subtle edge so it reads as a document, not chrome */
        int pg_x = 72, pg_y = 84, pg_w = w - 72 - 220, pg_h = h - 84 - 28;
        if (pg_w < 200) pg_w = 200;
        for (int y=pg_y-3; y<pg_y+pg_h+3; y++)
            for (int x=pg_x-3; x<pg_x+pg_w+3; x++)
                if (x>=0&&y>=0&&x<w&&y<h){ size_t di=((size_t)y*w+x)*4;
                    int edge = (x<pg_x||x>=pg_x+pg_w||y<pg_y||y>=pg_y+pg_h);
                    fb[di]=edge?sheet_edge.r:sheet_edge.g; fb[di+1]=sheet_edge.g; fb[di+2]=sheet_edge.b; }
        for (int y=pg_y; y<pg_y+pg_h; y++)
            for (int x=pg_x; x<pg_x+pg_w; x++)
                if (x>=0&&y>=0&&x<w&&y<h){ size_t di=((size_t)y*w+x)*4;
                    fb[di]=paper.r; fb[di+1]=paper.g; fb[di+2]=paper.b; }
        /* DOC-54: TOC pane lives in the reserved right margin band (x: pg_x+pg_w..w-8),
         * fully outside the page sheet, so it never overlaps page text. */
        int toc_x = pg_x + pg_w + 12;
        wubulayout_doc *L = wubulayout_create(e->doc, NULL,
            doc_layout_measure, doc_layout_style, NULL, w, h, pg_x,
            /* margin_r: keep text within the sheet (pg_x+pg_w), reserving a
             * 340px inserted-objects gutter only when objects exist. */
            (e->nobj > 0) ? (w - (pg_x+pg_w) + 340) : (w - (pg_x+pg_w)),
            56, 56);
        if (L){
            int pages = wubulayout_page_count(L);
            int pg = e->jump_page; e->jump_page = -1;
            if (pg < 0) pg = scroll / (h - 112);
            if (pg<0) pg=0;
            if (pg>=pages) pg=pages-1;
            /* UXA-41: high-contrast swaps to maximal-distinction palette */
            int hc = wubusettings_high_contrast(wubusettings_shared());
            unsigned char BG[3] = { hc?255:252, hc?255:252, hc?255:250 };
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
                /* DOC-60: links are blue + underlined; record box for clicks.
                 * Default text color is INK (dark on the paper sheet), not blue —
                 * previously the default was blue, painting every body word blue. */
                int is_link = 0; unsigned char lk[3];
                lk[0]=ink.r; lk[1]=ink.g; lk[2]=ink.b;
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
            /* line numbers in the left margin (DOC-72), inside the app-chrome
             * band to the left of the page sheet (sheet starts at pg_x=72). */
            int nl = wubulayout_line_count(L, pg);
            int gut = 60;   /* rule sits just left of the sheet */
            for (int y=0; y<h; y++){
                size_t di=((size_t)y*w+(gut-1))*4;
                fb[di]=LN[0]; fb[di+1]=LN[1]; fb[di+2]=LN[2];
            }
            for (int li=0; li<nl; li++){
                const wubulayout_line *ln = wubulayout_line_at(L, pg, li);
                if (!ln) continue;
                char ln2[16]; snprintf(ln2,sizeof ln2,"%d", li+1);
                int tw = wuos_font_text_width(ln2, wuos_font_height());
                wuos_font_draw(ln2, gut - 4 - tw, ln->y, 0, LN[0],LN[1],LN[2], fb, w, h);
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
            /* DOC-54: TOC side pane — drawn in the reserved left column
             * (x: 56 .. toc_x). The page text margin already clears this band,
             * so there is NO overlap with document text. A 1px separator marks
             * the column edge; entry text is clamped to the column width. */
            if (e->toc_dirty || !e->toc){
                toc_free(e->toc); e->toc = toc_build(e->doc, NULL, L);
                e->toc_dirty = 0;
            }
            /* separator rule at the right edge of the TOC column */
            for (int y=0; y<h; y++){
                size_t di=((size_t)y*w+(toc_x-1))*4;
                fb[di]=LN[0]; fb[di+1]=LN[1]; fb[di+2]=LN[2];
            }
            {
                int tx = toc_x, ty = 56;
                /* header sits over the DARK app chrome — use a light token. */
                wuos_font_draw("Contents", tx, ty-20, 1, 214, 218, 226, fb, w, h);
                int tcount = toc_count(e->toc);
                int col_w = (w - 8) - tx - 8;          /* usable width inside column */
                for (int i=0;i<tcount;i++){
                    const char *tt = toc_title(e->toc, i);
                    if (!tt) continue;
                    int lvl = toc_level(e->toc, i);
                    /* clamp the entry so it never overflows the TOC column */
                    char te[128];
                    int avail = col_w - (lvl-1)*10;
                    if (avail < 8) avail = 8;
                    int L_ = (int)strlen(tt);
                    if (L_ > avail) L_ = avail;
                    memcpy(te, tt, L_); te[L_]=0;
                    unsigned char r=TT[0],g=TT[1],b=TT[2];
                    if (lvl==1){ r=hc?0:40; g=hc?90:120; b=hc?200:60; }
                    wuos_font_draw(te, tx + (lvl-1)*10, ty, 0, r,g,b, fb, w, h);
                    ty += 18;
                    if (ty > h-40) break;
                }
                if (tcount == 0){
                    wuos_font_draw("(no headings)", tx, ty, 0,
                                   TT[0], TT[1], TT[2], fb, w, h);
                }
            }
            wubulayout_destroy(L);
        }
    } else {
        /* non-renderable format: show the text projection */
        fb = malloc((size_t)w*h*4);
        if (!fb) return -1;
        for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=app_bg.r;fb[k+1]=app_bg.g;fb[k+2]=app_bg.b;fb[k+3]=255; }
        wuos_font_draw("Document text (recognized):", WUOS_SPACE_8*2, WUOS_SPACE_8*2, 1, ink.r,ink.g,ink.b, fb,w,h);
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
                wuos_font_draw(tmp, x, y, 0, ink.r,ink.g,ink.b, fb,w,h);
                x += (int)wl*8 + 8;
                wstart = sp ? sp+1 : wstart+wl;
                if (y > h-30) break;
            }
            if (e->find_q && e->find_q[0]){
                int fy = h-26;
                for (int xx=0; xx<w; xx++) for(int yy=fy; yy<h; yy++)
                    if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t i=((size_t)yy*w+xx)*4; fb[i]=30;fb[i+1]=33;fb[i+2]=40; }
                /* count ALL occurrences (VS Code/Word highlight every match,
                 * not just the first — research 2026-08-12). Accurate count,
                 * not a boolean. */
                int nfound = 0;
                if (e->text && e->find_q[0]){
                    const char *hay = e->text;
                    while ((hay = strstr(hay, e->find_q))){
                        nfound++; hay += strlen(e->find_q);
                    }
                }
                char line[256];
                snprintf(line,sizeof line,"find '%s': %d match%s", e->find_q, nfound, nfound==1?"":"es");
                wuos_font_draw(line, WUOS_SPACE_8, fy+5, 0, ink.r,ink.g,ink.b, fb,w,h);
            }
        } else {
            wuos_font_draw("(nothing to display)", WUOS_SPACE_8*2, WUOS_SPACE_8*6, 0, ink.r,ink.g,ink.b, fb,w,h);
        }
    }
    /* ---- inserted objects overlay (chart/draw/math) on the right gutter.
     * GUI_SPEC: a themed CARD with border + heading, and it must NOT cover
     * the page sheet: it lives strictly right of the TOC separator. ---- */
    if (e->nobj){
        int ox = W - 340; if (ox < 8) ox = 8;
        WuosRGB obj_bg = dark ? WUOS_DARK(OVERLAY_SURFACE) : WUOS_LIGHT(OVERLAY_SURFACE);
        WuosRGB obj_bd = dark ? WUOS_DARK(OVERLAY_BD)     : WUOS_LIGHT(OVERLAY_BD);
        WuosRGB obj_tx = dark ? WUOS_DARK(OVERLINE_TEXT)  : WUOS_LIGHT(OVERLINE_TEXT);
        int oy0 = WUOS_SPACE_8;
        int oh = H - oy0 - WUOS_SPACE_32;
        for (int yy=oy0; yy<oy0+oh && yy<H; yy++)
            for (int xx=ox; xx<W-WUOS_SPACE_8 && xx<W; xx++){
                size_t di=((size_t)yy*W+xx)*4;
                fb[di]=obj_bg.r; fb[di+1]=obj_bg.g; fb[di+2]=obj_bg.b;
            }
        for (int xx=ox; xx<W-WUOS_SPACE_8; xx++){ size_t di=((size_t)oy0*W+xx)*4; fb[di]=obj_bd.r;fb[di+1]=obj_bd.g;fb[di+2]=obj_bd.b; }
        wuos_font_draw("Inserted objects:", ox+WUOS_SPACE_8, oy0+WUOS_SPACE_16+wuos_font_height(), 1, obj_tx.r,obj_tx.g,obj_tx.b, fb, W, H);
        int oy = oy0 + WUOS_SPACE_16 + wuos_font_height()*2;
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

/* Navigator sidebar content: the document outline (TOC) as indented entries.
 * Real structure from the TOC engine; NULL if no model/TOC. Caller frees. */
