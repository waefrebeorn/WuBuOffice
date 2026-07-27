/* view_ocr.c -- OCR view: loads a page (PNG or synthesized), runs the real
 * wubuocr pipeline (ocr_page_analyze + fontbank recognizer) and renders the
 * grayscale image with recognized text overlays. */
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

typedef struct { OcrImage *im; OcrPage *pg; } OcrV;

static OcrImage *make_sample(void){
    /* synthesize a real page from the system font so the tab works headless */
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

    if (!e->im){ wuos_font_draw("No OCR input", 20, 40, 1, 120,30,30, fb,w,h); *rgba=fb;*rw=w;*rh=h; return 0; }
    size_t iw=ocr_image_width(e->im), ih=ocr_image_height(e->im);
    int scale = (iw>0)? (w-40)/ (int)iw : 1; if (scale<1) scale=1;
    int ox=20, oy=30;
    for (size_t y=0;y<ih && oy+(int)y*scale<h;y++)
        for (size_t x=0;x<iw && ox+(int)x*scale<w;x++){
            uint8_t g = ocr_image_get(e->im,x,y);
            for (int dy=0;dy<scale;dy++) for (int dx=0;dx<scale;dx++){
                int xx=ox+(int)x*scale+dx, yy=oy+(int)y*scale+dy;
                if (xx>=0&&yy>=0&&xx<w&&yy<h){ size_t i=((size_t)yy*w+xx)*4;
                    fb[i]=g;fb[i+1]=g;fb[i+2]=g; }
            }
        }
    /* recognized text */
    if (e->pg){
        size_t n = ocr_page_block_count(e->pg);
        int ty = oy + (int)ih*scale + 16;
        wuos_font_draw("Recognized:", 20, ty, 1, 40,44,52, fb,w,h);
        ty += wuos_font_height()+6;
        for (size_t i=0;i<n && ty<h-20;i++){
            const char *t = ocr_page_block_text(e->pg,i);
            if (t && *t){ wuos_font_draw(t, 20, ty, 0, 30,33,38, fb,w,h); }
            ty += wuos_font_height()+4;
        }
    }
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static void destroy(WuView *v){ OcrV *e = v->priv; if(e->pg) ocr_page_free(e->pg); if(e->im) ocr_image_free(e->im); free(e); }

WuView *wuos_ocr_create(void){
    OcrV *e = calloc(1, sizeof *e);
    /* prefer an external page, else synthesize */
    size_t pl=0; uint8_t *pb=read_file("/tmp/ocr_in.png",&pl);
    if (pb){ int interlaced=0; e->im = ocr_image_from_png(pb, pl, &interlaced); free(pb); }
    if (!e->im) e->im = make_sample();
    if (e->im){
        OcrLayoutParams pr; memset(&pr,0,sizeof pr);
        pr.min_block_w=4; pr.min_block_h=4; pr.min_gutter_v=6; pr.min_gutter_h=6;
        e->pg = ocr_page_analyze(e->im, &pr, ocr_fontbank_recognizer(), NULL);
    }
    WuView *v = calloc(1, sizeof *v);
    v->name = "OCR";
    v->priv = e;
    v->destroy = destroy;
    v->render  = render;
    return v;
}
