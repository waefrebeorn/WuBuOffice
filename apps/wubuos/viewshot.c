/* viewshot.c -- headless WuBuOffice (wubuos) screenshot tool.
 *
 * Composes the SAME full window the live shell draws -- top tab bar, the
 * active view, and the bottom status bar -- into an in-memory RGBA buffer
 * (no SDL display required) and writes a pixel-faithful PNG. The composite
 * code is lifted verbatim from main.c's render loop so screenshots match the
 * real GUI exactly.
 *
 * Usage:
 *   viewshot <out.png> <tab> [file]
 *   tab in: doc|cell|slide|ocr|editor|compare|settings
 *
 * Clean C11. */
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_theme.h"
#include "plugin.h"
#include "settings.h"
#include "shape.h"
#include "toast.h"
#include "palette.h"
#include "macro.h"
#include "dialog.h"
#include "wubupng.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define WIN_W 960
#define WIN_H 720
#define TAB_H 30
#define STATUS_H 26
#define VSHOT_MENU_H 24   /* must precede all uses in this TU */

/* Paint `text` onto the RGBA buffer at (px,py) using the shared FreeType
 * helper (replaces main.c's sdl_text, which draws to an SDL texture). */
static void buf_text(unsigned char *fb, int W, int H, int px, int py,
                     unsigned char r, unsigned char g, unsigned char b,
                     const char *text){
    if (!text || !*text) return;
    wuos_font_draw(text, px, py, 0, r, g, b, fb, W, H);
}
static void buf_fill(unsigned char *fb, int W, int H, int x, int y, int w, int h,
                     unsigned char r, unsigned char g, unsigned char b){
    for (int yy=y; yy<y+h; yy++) for (int xx=x; xx<x+w; xx++){
        if (xx<0||yy<0||xx>=W||yy>=H) continue;
        size_t i=((size_t)yy*W+xx)*4; fb[i]=r; fb[i+1]=g; fb[i+2]=b; fb[i+3]=255;
    }
}

int main(int argc, char **argv){
    const char *out = (argc>1)? argv[1] : NULL;
    const char *want = (argc>2)? argv[2] : "editor";
    const char *file = (argc>3)? argv[3] : NULL;
    if (!out){ fprintf(stderr,"usage: viewshot <out.png> <tab> [file]\n"); return 2; }

    const char *auto_tab = NULL;
    if (file && !want){
        const char *dot = strrchr(file,'.');
        if (dot && (!strcasecmp(dot,".md")||!strcasecmp(dot,".txt")||!strcasecmp(dot,".html")||!strcasecmp(dot,".htm")))
            auto_tab="doc"; else auto_tab="editor";
    }
    const char *file_for_doc   = (want && !strcmp(want,"doc"))? file
                              : (auto_tab && !strcmp(auto_tab,"doc"))? file : NULL;
    const char *file_for_editor= (want && !strcmp(want,"editor"))? file
                              : (auto_tab && !strcmp(auto_tab,"editor"))? file : NULL;
    const char *file_for_ocr   = (want && !strcmp(want,"ocr"))? file : NULL;
    const char *file_for_cell  = (want && !strcmp(want,"cell"))? file : NULL;

    if (SDL_Init(SDL_INIT_VIDEO)!=0){ fprintf(stderr,"SDL init: %s\n",SDL_GetError()); return 1; }
    if (wuos_font_init()!=0){ fprintf(stderr,"font init failed\n"); SDL_Quit(); return 1; }

    WuView *views[8]; int nviews=0, active=0;
    #define ADD(v) do{ if((v) && nviews<8) views[nviews++]=(v); }while(0)
    ADD(wuos_doc_create(file_for_doc));
    ADD(wuos_cell_create(file_for_cell));
    ADD(wuos_slide_create(NULL));
    ADD(wuos_ocr_create(file_for_ocr));
    ADD(wuos_editor_create(file_for_editor));
    ADD(wuos_compare_create(argc>3?argv[3]:NULL, argc>4?argv[4]:NULL));
    ADD(wuos_settings_create());
    #undef ADD
    if (nviews==0){ fprintf(stderr,"no views\n"); return 1; }

    for (int i=0;i<nviews;i++){
        const char *t = want? want : auto_tab;
        int hit = 0;
        if (t){
            /* exact (case-insensitive) name match, plus friendly aliases */
            if (strcasecmp(views[i]->name, t)==0) hit=1;
            else if (strcasecmp(t,"doc")==0    && strcasecmp(views[i]->name,"document")==0)    hit=1;
            else if (strcasecmp(t,"cell")==0   && strcasecmp(views[i]->name,"spreadsheet")==0) hit=1;
            else if (strcasecmp(t,"document")==0 && strcasecmp(views[i]->name,"document")==0)  hit=1;
            else if (strcasecmp(t,"spreadsheet")==0 && strcasecmp(views[i]->name,"spreadsheet")==0) hit=1;
            else if (strcasecmp(t,"editor")==0 && strcasecmp(views[i]->name,"editor")==0)      hit=1;
            else if (strcasecmp(t,"compare")==0 && strcasecmp(views[i]->name,"compare")==0)    hit=1;
        }
        if (hit){ active=i; break; }
    }

    /* ---- composite the window into an RGBA buffer (mirrors main.c) ---- */
    unsigned char *fb = calloc((size_t)WIN_W*WIN_H*4, 1);
    if (!fb){ fprintf(stderr,"oom\n"); return 1; }
    buf_fill(fb, WIN_W, WIN_H, 0,0, WIN_W,WIN_H, 235,237,240);  /* base bg */

    /* active view (placed below the tab strip AND the menu bar) */
    unsigned char *rgba=NULL; int rw=0, rh=0;
    int scroll = 0;
    int view_top = TAB_H + VSHOT_MENU_H;
    if (views[active]->render(views[active], WIN_W, WIN_H-view_top-STATUS_H, scroll, &rgba, &rw, &rh)==0 && rgba){
        int draw_w = rw, draw_h = rh;            /* zoom = 1.0 for screenshots */
        int maxscroll = (rh > (WIN_H-view_top-STATUS_H))? rh-(WIN_H-view_top-STATUS_H):0;
        if (scroll>maxscroll) scroll=maxscroll;
        /* blit (zoom 1: direct row copy of the view's RGBA) */
        int src_h = (draw_h < (WIN_H-view_top-STATUS_H))? draw_h : (WIN_H-view_top-STATUS_H);
        int src_w = (draw_w < WIN_W)? draw_w : WIN_W;
        for (int y=0; y<src_h; y++) for (int x=0; x<src_w; x++){
            size_t s=((size_t)y*src_w+x)*4;
            size_t d=((size_t)(view_top+y)*WIN_W+x)*4;
            fb[d]=rgba[s]; fb[d+1]=rgba[s+1]; fb[d+2]=rgba[s+2]; fb[d+3]=255;
        }
        free(rgba);
    }

    /* tab bar: neutral chrome ground; active tab rises + accent underline
     * (spec §5). Mirrors main.c exactly so screenshots match the live GUI. */
    WuosRGB tbb = (WuosRGB)WUOS_DARK_TAB_BAR;
    WuosRGB tt  = (WuosRGB)WUOS_DARK_TAB;
    WuosRGB tto = (WuosRGB)WUOS_DARK_TAB_ON;
    WuosRGB ttx = (WuosRGB)WUOS_DARK_TABTEXT;
    WuosRGB ttxo= (WuosRGB)WUOS_DARK_TABTEXT_ON;
    WuosRGB bd  = (WuosRGB)WUOS_DARK_BORDER;
    WuosRGB ac  = (WuosRGB)WUOS_DARK_ACCENT;
    buf_fill(fb, WIN_W, WIN_H, 0,0, WIN_W,TAB_H, tbb.r,tbb.g,tbb.b);
    int x=0;
    for (int i=0;i<nviews;i++){
        int tw=(int)strlen(views[i]->name)*14+24;
        int on = (i==active);
        WuosRGB seg = on ? tto : tt;
        buf_fill(fb, WIN_W, WIN_H, x,0, tw,TAB_H, seg.r,seg.g,seg.b);
        buf_fill(fb, WIN_W, WIN_H, x+tw-1,0, 1,TAB_H, bd.r,bd.g,bd.b);
        if (on) buf_fill(fb, WIN_W, WIN_H, x,TAB_H-2, tw,2, ac.r,ac.g,ac.b);
        buf_text(fb, WIN_W, WIN_H, x+12, (TAB_H-wuos_font_height())/2 + 2,
                 on?ttxo.r:ttx.r, on?ttxo.g:ttx.g, on?ttxo.b:ttx.b, views[i]->name);
        x+=tw;
    }

    /* menu bar (UI-43): parity with main.c's live render. Top-level items only
     * (dropdowns are live-interaction only; headless shots show the bar). */
    static const char *vmenus[4] = { "File", "Edit", "View", "Help" };
    buf_fill(fb, WIN_W, WIN_H, 0, TAB_H, WIN_W, VSHOT_MENU_H, tbb.r,tbb.g,tbb.b);
    { int mx=0;
      for (int mi=0; mi<4; mi++){
          int mw=(int)strlen(vmenus[mi])*14+22;
          buf_text(fb, WIN_W, WIN_H, mx+11, TAB_H + (VSHOT_MENU_H-wuos_font_height())/2 + 1,
                   ttx.r,ttx.g,ttx.b, vmenus[mi]);
          mx+=mw;
      } }

    /* status bar */
    WuosRGB sb = (WuosRGB)WUOS_DARK_STATUS;
    WuosRGB stx= (WuosRGB)WUOS_DARK_STATUSTX;
    buf_fill(fb, WIN_W, WIN_H, 0,WIN_H-STATUS_H, WIN_W,STATUS_H, sb.r,sb.g,sb.b);
    buf_fill(fb, WIN_W, WIN_H, 0,WIN_H-STATUS_H, WIN_W,1, bd.r,bd.g,bd.b);
    char *st = views[active]->status? views[active]->status(views[active]) : NULL;
    if (st){
        buf_text(fb, WIN_W, WIN_H, 8, WIN_H-STATUS_H + (STATUS_H-wuos_font_height())/2 + 1,
                 stx.r,stx.g,stx.b, st);
        free(st);
    }

    if (wubupng_write_file(out, WUBUPNG_RGBA, fb, WIN_W, WIN_H)!=0){
        fprintf(stderr,"png write failed: %s\n", out);
        free(fb); return 1;
    }
    fprintf(stderr,"wrote %s [tab=%s] %dx%d\n", out, views[active]->name, WIN_W, WIN_H);

    for (int i=0;i<nviews;i++) views[i]->destroy(views[i]);
    free(fb);
    wuos_font_quit();
    SDL_Quit();
    return 0;
}
