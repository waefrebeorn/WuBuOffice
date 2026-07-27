/* view_ocr.c -- OCR view: runs the real wubuocr pipeline and is interactive.
 * The recognized blocks are listed in a right-hand panel; Up/Down move a
 * selection; the selected block's text is shown in a detail strip and Enter
 * "copies" it (recorded in sel_text so other code / tests can read it). */
#include "wuos.h"
#include "wuos_font.h"
#include "png.h"          /* ocr_image_from_png */
#include "image.h"        /* OcrImage */
#include "wubuocr.h"      /* ocr_page_analyze / OcrPage */
#include "fontbank.h"     /* ocr_fontbank_recognizer */
#include "page_compose.h" /* ocr_compose_line */
#include "wubufont.h"     /* font_open */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint8_t *read_file(const char *p, size_t *len){
    FILE *f = fopen(p,"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    uint8_t *b=malloc(n?n:1); if(fread(b,1,n,f)!=(size_t)n){ free(b); fclose(f); return NULL; }
    fclose(f); *len=(size_t)n; return b;
}

typedef struct { OcrImage *im; OcrPage *pg; int sel; char *sel_text; } OcrV;

static OcrImage *make_sample(void){
    static const char *path="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    size_t fl=0; uint8_t *fb=read_file(path,&fl); if(!fb) return NULL;
    Font *font = font_open(fb, fl); if(!font){ free(fb); return NULL; }
    OcrImage *im = ocr_compose_line(font, "The quick brown fox 0123456789 jumps", 90);
    font_free(font); free(fb);
    return im;
}

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    OcrV *e = v->priv;
    (void)scroll;
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ fb[i*4]=245;fb[i*4+1]=246;fb[i*4+2]=248;fb[i*4+3]=255; }

    int panel_x = w - 300; if (panel_x < w/2) panel_x = w/2;

    if (!e->im){ wuos_font_draw("No OCR input", 20, 40, 1, 120,30,30, fb,w,h); *rgba=fb;*rw=w;*rh=h; return 0; }
    size_t iw=ocr_image_width(e->im), ih=ocr_image_height(e->im);
    int scale = (iw>0)? (panel_x-40)/ (int)iw : 1; if (scale<1) scale=1;
    int ox=20, oy=30;
    for (size_t y=0;y<ih && oy+(int)y*scale<h-70;y++)
        for (size_t x=0;x<iw && ox+(int)x*scale<panel_x-10;x++){
            uint8_t g = ocr_image_get(e->im,x,y);
            for (int dy=0;dy<scale;dy++) for (int dx=0;dx<scale;dx++){
                int xx=ox+(int)x*scale+dx, yy=oy+(int)y*scale+dy;
                if (xx>=0&&yy>=0&&xx<w&&yy<h){ size_t i=((size_t)yy*w+xx)*4;
                    fb[i]=g;fb[i+1]=g;fb[i+2]=g; }
            }
        }

    /* panel header */
    wuos_font_draw("Recognized (Up/Down, Enter copies):", panel_x+8, 30, 1, 40,44,52, fb,w,h);
    if (e->pg){
        size_t n = ocr_page_block_count(e->pg);
        int ty = 54; int idx=0;
        for (size_t i=0;i<n && ty<h-60;i++){
            const char *t = ocr_page_block_text(e->pg,i);
            if (!t || !*t){ continue; }
            int on = (idx==e->sel);
            int ry = ty;
            if (on){ for (int xx=panel_x+4; xx<w-6; xx++) for(int yy=ry-2; yy<ry+wuos_font_height()+4; yy++)
                        if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t ii=((size_t)yy*w+xx)*4; fb[ii]=210;fb[ii+1]=232;fb[ii+2]=255; } }
            wuos_font_draw(t, panel_x+10, ry, 0, on?20:30, on?30:33, on?40:38, fb,w,h);
            ty += wuos_font_height()+6; idx++;
        }
        /* detail strip */
        int dy = h-26;
        for (int xx=0; xx<w; xx++) for(int yy=dy; yy<h; yy++)
            if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t ii=((size_t)yy*w+xx)*4; fb[ii]=30;fb[ii+1]=33;fb[ii+2]=40; }
        if (e->sel_text) wuos_font_draw(e->sel_text, 8, dy+5, 0, 200,203,210, fb,w,h);
    }
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static char *status(WuView *v){
    OcrV *e = v->priv;
    size_t n = e->pg ? ocr_page_block_count(e->pg) : 0;
    char *s = malloc(96); if(!s) return NULL;
    snprintf(s,96,"OCR — %zu block(s), selected %d", n, e->sel);
    return s;
}

static void on_key(WuView *v, int key, int down){
    OcrV *e = v->priv;
    if (!down || !e->pg) return;
    size_t n = ocr_page_block_count(e->pg);
    int visible = 0;
    for (size_t i=0;i<n;i++){ const char *t=ocr_page_block_text(e->pg,i); if(t&&*t) visible++; }
    if (key==WUOS_KEY_DOWN){ if (e->sel < visible-1) e->sel++; }
    else if (key==WUOS_KEY_UP){ if (e->sel>0) e->sel--; }
    else if (key==WUOS_KEY_RETURN){
        free(e->sel_text); e->sel_text=NULL;
        /* map selection to the i-th non-empty block */
        int idx=0;
        for (size_t i=0;i<n;i++){ const char *t=ocr_page_block_text(e->pg,i);
            if(!t||!*t) continue;
            if (idx==e->sel){ size_t L=strlen(t); e->sel_text=malloc(L+1); if(e->sel_text) memcpy(e->sel_text,t,L+1); break; }
            idx++; }
    }
}

static void destroy(WuView *v){
    OcrV *e = v->priv;
    if(e->pg) ocr_page_free(e->pg);
    if(e->im) ocr_image_free(e->im);
    free(e->sel_text);
    free(e);
}

WuView *wuos_ocr_create(const char *path){
    OcrV *e = calloc(1, sizeof *e);
    const char *src = path ? path : "/tmp/ocr_in.png";
    size_t pl=0; uint8_t *pb=read_file(src,&pl);
    if (!pb && path){ pb = read_file("/tmp/ocr_in.png",&pl); }
    if (pb){ int interlaced=0; e->im = ocr_image_from_png(pb, pl, &interlaced); free(pb); }
    if (!e->im) e->im = make_sample();
    if (e->im){
        OcrLayoutParams pr; memset(&pr,0,sizeof pr);
        pr.min_block_w=4; pr.min_block_h=4; pr.min_gutter_v=6; pr.min_gutter_h=6;
        e->pg = ocr_page_analyze(e->im, &pr, ocr_fontbank_recognizer(), NULL);
    }
    e->sel = 0; e->sel_text = NULL;
    WuView *v = calloc(1, sizeof *v);
    v->name = "OCR";
    v->priv = e;
    v->destroy = destroy;
    v->render  = render;
    v->status  = status;
    v->on_key  = on_key;
    return v;
}

/* ---- test accessors ---- */
int wuos_ocr_blocks(WuView *v){
    OcrV *e = v->priv; return e->pg ? (int)ocr_page_block_count(e->pg) : 0;
}
char *wuos_ocr_selected(WuView *v){
    OcrV *e = v->priv; if(!e->sel_text) return NULL;
    size_t L=strlen(e->sel_text); char *r=malloc(L+1); if(r) memcpy(r,e->sel_text,L+1); return r;
}
