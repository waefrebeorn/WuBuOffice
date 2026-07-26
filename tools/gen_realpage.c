/* gen_realpage.c -- synthesize a multi-line "document" page as a PNG with
 * photo-like distortions (rotation/skew + salt-and-pepper noise + contrast
 * jitter) so we can validate the CRNN->doc pipeline on non-pristine input.
 *
 *   gen_realpage <font.ttf> <out.png> [out_gt.txt]
 *
 * The ground-truth line text is printed to stdout (and optionally to the
 * second file) as "line N: TEXT" for easy diffing against the OCR output.
 *
 * Build: cmake target gen_realpage (links crnnocr wubuimage wubufont m)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "image.h"
#include "wubufont.h"
#include "ocr_render.h"
#include "png.h"
#include "png_encode.h"

#define STRIP 20
#define PPM   16
#define GAP   10
#define NLINES 6
#define MAXW  12      /* keep within the trained MAXLEN */
#define PGM   16

/* crude deterministic RNG so runs are reproducible */
static uint32_t s = 123456789u;
static float rndf(void){ s ^= s<<13; s ^= s>>17; s ^= s<<5; return (float)s/(float)0xFFFFFFFFu; }

static void paint_letter(OcrImage *im, Font *f, int x0, int y0, char ch){
    uint8_t *bits=NULL; int w=0,h=0;
    if(!font_rasterize(f,(uint32_t)ch,PPM,&bits,&w,&h)){ if(bits)free(bits); return; }
    int ox=(STRIP-w)/2, oy=(STRIP-h)/2;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){
        int px=x0+ox+x, py=y0+oy+y;
        if(bits[y*w+x] && px>=0 && px<(int)ocr_image_width(im) && py>=0 && py<(int)ocr_image_height(im))
            ocr_image_set(im,(size_t)px,(size_t)py,30); /* dark text on light page */
    }
    free(bits);
}

int main(int argc,char**argv){
    if(argc<3){ printf("usage: %s <font.ttf> <out.png> [out_gt.txt] [--cols N]\n",argv[0]); return 1; }
    int cols=1;
    for(int a=3;a<argc;a++){ if(strcmp(argv[a],"--cols")==0 && a+1<argc){ cols=atoi(argv[++a]); } }
    if(cols<1) cols=1;
    size_t fn; uint8_t*fb=ocr_readf(argv[1],&fn);
    Font *f = fb? font_open(fb,fn):NULL;
    if(!f){ printf("font open failed\n"); return 1; }

    srand((unsigned)time(NULL));
    const char *CH = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'-";
    int nch=(int)strlen(CH);

    int colgap=60;
    int W=cols*520 + (cols-1)*colgap, H=NLINES*(STRIP+GAP)+GAP;
    OcrImage *im=ocr_image_create(W,H);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) ocr_image_set(im,(size_t)x,(size_t)y,235); /* white page */

    char lines[8][NLINES][MAXW+1];
    for(int c=0;c<cols;c++){
        int xoff=c*(520+colgap);
        for(int l=0;l<NLINES;l++){
            int L=4+(int)(rndf()* (MAXW-3));
            int y0=GAP+l*(STRIP+GAP);
            for(int i=0;i<L;i++){ char ch=CH[(int)(rndf()*nch)]; lines[c][l][i]=ch; paint_letter(im,f,xoff+GAP+i*STRIP,y0,ch); }
            lines[c][l][L]=0;
        }
    }

    /* --- photo-like distortion --- */
    /* 1) whole-page rotation by a few degrees around center */
    double ang=(rndf()*2-1)*0.06; /* ~ +-3.4 deg */
    double ca=cos(ang), sa=sin(ang);
    int cx=W/2, cy=H/2;
    OcrImage *rot=ocr_image_create(W,H);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) ocr_image_set(rot,(size_t)x,(size_t)y,235);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int sx=(int)(cx+(x-cx)*ca+(y-cy)*sa);
        int sy=(int)(cy-(x-cx)*sa+(y-cy)*ca);
        if(sx>=0&&sx<W&&sy>=0&&sy<H)
            ocr_image_set(rot,(size_t)x,(size_t)y,ocr_image_get(im,(size_t)sx,(size_t)sy));
    }
    OcrImage *tmp=im; im=rot; ocr_image_free(tmp);

    /* 2) salt-and-pepper noise + slight contrast jitter */
    int nn=(int)(rndf()*W*H*0.03f);
    for(int i=0;i<nn;i++){
        int x=(int)(rndf()*W), y=(int)(rndf()*H);
        int r=(int)(rndf()*100);
        uint8_t g=ocr_image_get(im,(size_t)x,(size_t)y);
        uint8_t ng = r<8? 0 : (r>92? 255 : g);
        ocr_image_set(im,(size_t)x,(size_t)y,ng);
    }

    /* write PNG */
    uint8_t *png=NULL; size_t pl=0;
    if(png_encode_gray(ocr_image_pixels(im),(uint32_t)W,(uint32_t)H,&png,&pl)){
        printf("png encode failed\n"); ocr_image_free(im); font_free(f); free(fb); return 1;
    }
    FILE *of=fopen(argv[2],"wb");
    if(!of){ printf("cannot write %s\n",argv[2]); free(png); ocr_image_free(im); font_free(f); free(fb); return 1; }
    fwrite(png,1,pl,of); fclose(of); free(png);
    printf("wrote %s (%zu bytes)\n",argv[2],pl);

    /* ground truth (row-major across columns: line r, columns 0..cols-1) */
    FILE *gf = argc>3? fopen(argv[3],"w") : stdout;
    for(int l=0;l<NLINES;l++){
        fprintf(gf,"line %d:", l);
        for(int c=0;c<cols;c++) fprintf(gf,"\t%s", lines[c][l]);
        fprintf(gf,"\n");
    }
    if(gf!=stdout) fclose(gf);

    ocr_image_free(im); font_free(f); free(fb);
    return 0;
}
