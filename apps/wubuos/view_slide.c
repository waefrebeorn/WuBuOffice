/* view_slide.c -- Slide view: renders a simple real presentation slide
 * (title + bullets + a native bar chart). Reuses the shared font helper. */
#include "wuos.h"
#include "wuos_font.h"

#include <stdlib.h>
#include <string.h>

typedef struct { int dummy; } SlideV;

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    (void)v; (void)scroll;
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ fb[i*4]=255;fb[i*4+1]=255;fb[i*4+2]=255;fb[i*4+3]=255; }

    /* accent bar */
    for (int y=0;y<10;y++) for (int x=0;x<w;x++){ size_t i=((size_t)y*w+x)*4; fb[i]=59;fb[i+1]=130;fb[i+2]=246; }

    wuos_font_draw("WuBuOffice — Slide Deck", 50, 90, 1, 30,33,38, fb,w,h);
    const char *bullets[] = {
        "One engine, every format: docx / xlsx / pptx / odt / pdf",
        "Real OCR with a from-scratch recognizer",
        "Notepad++-class editor embedded in the shell",
        "All rendered through one shared surface",
        NULL };
    int y=150;
    for (int i=0;bullets[i];i++){
        wuos_font_draw("-", 60, y, 0, 200,80,30, fb,w,h);
        wuos_font_draw(bullets[i], 80, y, 0, 40,44,52, fb,w,h);
        y += wuos_font_height() + 14;
    }
    /* bar chart */
    int cx0=60, cy0=y+10, cw=w-120, chh=160;
    double vals[]={40,65,50,80,55}; int n=5; double maxv=80; int bw=cw/n-16;
    for (int i=0;i<n;i++){
        int bx=cx0+8+i*(cw/n);
        int bh=(int)(chh*(vals[i]/maxv));
        for (int yy=cy0+chh-bh; yy<cy0+chh; yy++) for (int xx=bx;xx<bx+bw;xx++){
            if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t ii=((size_t)yy*w+xx)*4; fb[ii]=59;fb[ii+1]=130;fb[ii+2]=246; }
        }
    }
    for (int x=cx0;x<cx0+cw;x++){ size_t ii=((size_t)(cy0+chh)*w+x)*4; fb[ii]=120;fb[ii+1]=120;fb[ii+2]=125; }
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static void destroy(WuView *v){ free(v->priv); }

WuView *wuos_slide_create(void){
    SlideV *e = calloc(1, sizeof *e);
    WuView *v = calloc(1, sizeof *v);
    v->name = "Slide";
    v->priv = e;
    v->destroy = destroy;
    v->render  = render;
    return v;
}
