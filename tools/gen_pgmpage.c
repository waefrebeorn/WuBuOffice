/* gen_pgmpage.c -- emit a synthetic multi-line Latin page as PGM (Netpbm),
 * painted with the wubufont rasterizer, for end-to-end image2doc testing.
 * Usage: gen_pgmpage FONT.ttf OUT.pgm
 *   Lines are random A..Z words; ground truth printed to stdout.
 */
#include "wubufont.h"
#include "image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STRIP 20
#define PPM   16
#define GAP   10
#define NLINES 6
#define MAXW  12

static uint8_t* readf(const char*p,size_t*n){
    FILE*f=fopen(p,"rb"); if(!f) return 0;
    fseek(f,0,SEEK_END); *n=ftell(f); fseek(f,0,SEEK_SET);
    uint8_t*b=malloc(*n); if(fread(b,1,*n,f)!=*n){} fclose(f); return b;
}
static uint32_t rng=12345u;
static float rnd(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return (float)(rng&0xFFFFFF)/(float)0xFFFFFF; }

static void paint_letter(OcrImage *im, Font *f, int x0, int y0, char ch){
    uint8_t *bits=NULL; int w=0,h=0;
    if(!font_rasterize(f,(uint32_t)ch,PPM,&bits,&w,&h)){ if(bits)free(bits); return; }
    int ox=(STRIP-w)/2; if(ox<0)ox=0;
    int oy=(STRIP-h)/2; if(oy<0)oy=0;
    for(int y=0;y<h && y+oy<STRIP;y++) for(int x=0;x<w && x+ox<STRIP;x++)
        if(bits[y*w+x]) ocr_image_set(im, x0+ox+x, y0+oy+y, 235);
    free(bits);
}

/* Document alphabet: lowercase + uppercase + digits + space + punctuation.
 * Override with CHARS=... to match the model the page is meant to test. */
static const char *DOC_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'-";

int main(int argc,char**argv){
    if(argc<3){ printf("usage: %s FONT.ttf OUT.pgm\n",argv[0]); return 1; }
    const char *CH = getenv("CHARS") ? getenv("CHARS") : DOC_CHARS;
    size_t fn; uint8_t*fb=readf(argv[1],&fn);
    Font *font=fb?font_open(fb,fn):NULL;
    if(!font){ printf("font open failed\n"); return 1; }
    rng=(uint32_t)time(NULL)|1u;

    int W=MAXW*STRIP, H=NLINES*STRIP+(NLINES+1)*GAP;
    OcrImage *page=ocr_image_create(W,H);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) ocr_image_set(page,x,y,15);

    int nch = (int)strlen(CH);
    srand((unsigned)time(NULL));
    for(int l=0;l<NLINES;l++){
        int L=4+(int)(rnd()*6.99f); if(L>MAXW)L=MAXW;
        int y0=GAP+l*(STRIP+GAP);
        char line[MAXW+1];
        for(int i=0;i<L;i++){
            int li=(int)(rnd()*nch); if(li>nch-1)li=nch-1;
            char c=CH[li];
            line[i]=c;
            if(c!=' ') paint_letter(page,font,i*STRIP,y0,c);  /* space -> blank cell */
        }
        line[L]='\0';
        printf("GT line %d: %s\n", l, line);
    }

    uint8_t *pgm; size_t pl;
    ocr_image_to_pgm(page,&pgm,&pl);
    FILE*o=fopen(argv[2],"wb"); fwrite(pgm,1,pl,o); fclose(o);
    printf("wrote %s (%zu bytes)\n", argv[2], pl);

    free(pgm); ocr_image_free(page); font_free(font); free(fb);
    return 0;
}
