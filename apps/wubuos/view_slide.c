/* view_slide.c -- Slide view: renders a simple real presentation slide
 * (title + bullets + a native bar chart). Reuses the shared font helper. */
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_theme.h"

#include <stdlib.h>
#include <string.h>

typedef struct { int dummy; } SlideV;
static int dark_mode(void){ return wubusettings_dark(wubusettings_shared()); }

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    (void)v; (void)scroll;
    int dark = dark_mode();
    WuosRGB sld_bg = dark ? WUOS_DARK(TAB_BAR) : WUOS_LIGHT(TAB_BAR);
    WuosRGB sld_accent = dark ? WUOS_DARK(ACCENT) : WUOS_LIGHT(ACCENT);
    WuosRGB sld_body = dark ? WUOS_DARK(OVERLAY_TEXT) : WUOS_LIGHT(OVERLAY_TEXT);
    WuosRGB sld_hd = dark ? WUOS_DARK(OVERLINE_TEXT) : WUOS_LIGHT(OVERLINE_TEXT);
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=sld_bg.r;fb[k+1]=sld_bg.g;fb[k+2]=sld_bg.b;fb[k+3]=255; }

    /* accent bar */
    for (int y=0;y<10;y++) for (int x=0;x<w;x++){ size_t i=((size_t)y*w+x)*4; fb[i]=sld_accent.r;fb[i+1]=sld_accent.g;fb[i+2]=sld_accent.b; }

    wuos_font_draw("WuBuOffice — Slide Deck", WUOS_SPACE_8*6, WUOS_SPACE_8*11, 1, sld_hd.r,sld_hd.g,sld_hd.b, fb,w,h);
    const char *bullets[] = {
        "One engine, every format: docx / xlsx / pptx / odt / pdf",
        "Real OCR with a from-scratch recognizer",
        "Notepad++-class editor embedded in the shell",
        "All rendered through one shared surface",
        NULL };
    int y=150;
    for (int i=0;bullets[i];i++){
        wuos_font_draw("-", WUOS_SPACE_8*7, y, 0, sld_accent.r,sld_accent.g,sld_accent.b, fb,w,h);
        wuos_font_draw(bullets[i], WUOS_SPACE_8*8, y, 0, sld_body.r,sld_body.g,sld_body.b, fb,w,h);
        y += wuos_font_height() + WUOS_SPACE_4;
    }
    /* bar chart */
    int cx0=60, cy0=y+10, cw=w-120, chh=160;
    double vals[]={40,65,50,80,55}; int n=5; double maxv=80; int bw=cw/n-16;
    for (int i=0;i<n;i++){
        int bx=cx0+8+i*(cw/n);
        int bh=(int)(chh*(vals[i]/maxv));
        for (int yy=cy0+chh-bh; yy<cy0+chh; yy++) for (int xx=bx;xx<bx+bw;xx++){
            if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t ii=((size_t)yy*w+xx)*4; fb[ii]=sld_accent.r;fb[ii+1]=sld_accent.g;fb[ii+2]=sld_accent.b; }
        }
    }
    for (int x=cx0;x<cx0+cw;x++){ size_t ii=((size_t)(cy0+chh)*w+x)*4; fb[ii]=sld_body.r;fb[ii+1]=sld_body.g;fb[ii+2]=sld_body.b; }
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static void destroy(WuView *v){ free(v->priv); free(v); }

WuView *wuos_slide_create(const char *path){
    SlideV *e = calloc(1, sizeof *e);
    WuView *v = calloc(1, sizeof *v);
    v->name = "Slide";
    v->priv = e;
    v->destroy = destroy;
    v->render  = render;
    return v;
}
