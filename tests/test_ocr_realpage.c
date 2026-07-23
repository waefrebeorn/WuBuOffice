/* test_ocr_realpage.c -- regression test for the deskew + line-segmentation
 * front door on a ROTATED multi-line page (the photo/scan case).
 *
 * It builds a 6-line A-Z page, rotates it ~4 degrees (simulating a tilted
 * phone photo), then runs crnn_transcribe_page_json and asserts the pipeline
 * recovers exactly 6 text blocks -- i.e. the projection-profile deskew
 * straightens the page so horizontal-projection segmentation finds the right
 * number of lines. Recognition accuracy is covered by test_crnn_transcribe;
 * this test locks in the segmentation/deskew behaviour on skewed input.
 *
 * Requires a trained A-Z model: set LOAD=/path/to/latin.crnn. If unset, the
 * test prints SKIP and exits 0 (so the suite stays green in minimal setups).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crnn.h"
#include "image.h"
#include "wubufont.h"
#include "ocr_render.h"

#define STRIP 20
#define PPM   16
#define GAP   10
#define NLINES 6
#define MAXW  10

static uint32_t s = 0x1234ABCDu;
static float rndf(void){ s ^= s<<13; s ^= s>>17; s ^= s<<5; return (float)(s&0xFFFFFF)/(float)0xFFFFFF; }

static OcrImage *build_page(Font *f, char lines[NLINES][MAXW+1]){
    int W = (MAXW+2)*STRIP, H = NLINES*(STRIP+GAP)+GAP;
    OcrImage *im = ocr_image_create(W,H);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) ocr_image_set(im,(size_t)x,(size_t)y,15); /* dark bg */
    const char *CH="ABCDEFGHIJKLMNOPQRSTUVWXYZ"; int nch=(int)strlen(CH);
    for(int l=0;l<NLINES;l++){
        int L=4+(int)(rndf()*6); int y0=GAP+l*(STRIP+GAP);
        for(int i=0;i<L;i++){
            char ch=CH[(int)(rndf()*nch)]; lines[l][i]=ch;
            uint8_t*bits=NULL; int w=0,h=0;
            if(font_rasterize(f,(uint32_t)ch,PPM,&bits,&w,&h)){
                int ox=(STRIP-w)/2, oy=(STRIP-h)/2;
                for(int y=0;y<h;y++) for(int x=0;x<w;x++){
                    int px=(i+1)*STRIP+ox+x, py=y0+oy+y;
                    if(bits[y*w+x]&&px>=0&&px<W&&py>=0&&py<H) ocr_image_set(im,(size_t)px,(size_t)py,235);
                }
                free(bits);
            }
        }
        lines[l][L]=0;
    }
    return im;
}

static OcrImage *rotate(const OcrImage *src, double deg){
    int W=(int)ocr_image_width(src),H=(int)ocr_image_height(src),cx=W/2,cy=H/2;
    double a=deg*3.141592653589793/180.0,ca=cos(a),sa=sin(a);
    OcrImage *d=ocr_image_create((size_t)W,(size_t)H);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) ocr_image_set(d,(size_t)x,(size_t)y,15);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int sx=(int)(cx+(x-cx)*ca+(y-cy)*sa), sy=(int)(cy-(x-cx)*sa+(y-cy)*ca);
        if(sx>=0&&sx<W&&sy>=0&&sy<H) ocr_image_set(d,(size_t)x,(size_t)y,ocr_image_get(src,(size_t)sx,(size_t)sy));
    }
    return d;
}

int main(void){
    const char *LOAD=getenv("LOAD");
    if(!LOAD){ printf("SKIP (set LOAD=/path/to/latin.crnn)\n"); return 0; }
    const char *FONT=getenv("FONT")? getenv("FONT") : "fonts/multiscript_active/Latin.ttf";
    size_t fn; uint8_t*fb=ocr_readf(FONT,&fn);
    Font *f = fb? font_open(fb,fn):NULL;
    if(!f){ printf("SKIP (font %s missing)\n",FONT); return 0; }
    CRNN *m=NULL;
    if(!crnn_load(LOAD,&m)||!m){ printf("FAIL crnn_load %s\n",LOAD); font_free(f); free(fb); return 1; }

    char lines[NLINES][MAXW+1];
    OcrImage *page = build_page(f, lines);
    OcrImage *tilt = rotate(page, 4.0);   /* ~4 deg tilt */

    char *json=NULL;
    int rc = crnn_transcribe_page_json(m, tilt, STRIP, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", &json);
    int blocks = 0;
    if(rc==0 && json){
        blocks = 0; const char *p=json;
        while((p=strstr(p,"\"text\""))){ blocks++; p++; }
    }
    printf("rotated-page transcription: rc=%d blocks=%d\n", rc, blocks);

    ocr_image_free(page); ocr_image_free(tilt);
    crnn_free(m); free(json); font_free(f); free(fb);

    if(rc!=0){ printf("FAIL transcription rc=%d\n",rc); return 1; }
    if(blocks != NLINES){ printf("FAIL expected %d blocks, got %d (deskew/segmentation broke on tilt)\n", NLINES, blocks); return 1; }
    printf("PASS: deskew+segmentation recovered %d lines from a %d-deg tilted page\n", blocks, 4);
    return 0;
}
