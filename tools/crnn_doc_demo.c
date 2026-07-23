/* crnn_doc_demo.c -- END-TO-END document transcription:
 *   multi-line page image  ->  horizontal-projection line segmentation
 *   ->  per-line crop (normalized to STRIP height)  ->  crnn_recognize  ->  text.
 *
 * Proves the full document path: a page (not a pre-cropped line) becomes text.
 * Renders its own synthetic page from a font so it is self-checking (prints
 * ground-truth beside the transcription). Load a trained model via LOAD=path.
 *
 * Usage: LOAD=/tmp/latin.crnn crnn_doc_demo <font.ttf>
 */
#include "crnn.h"
#include "image.h"
#include "wubufont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static uint8_t* readf(const char*p,size_t*n){
    FILE*f=fopen(p,"rb"); if(!f) return 0;
    fseek(f,0,SEEK_END); *n=ftell(f); fseek(f,0,SEEK_SET);
    uint8_t*b=malloc(*n); if(fread(b,1,*n,f)!=*n){} fclose(f); return b;
}
static uint32_t rng=987654321u;
static float rnd(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return (float)(rng&0xFFFFFF)/(float)0xFFFFFF; }

#define STRIP 20
#define PPM   16
#define GAP   8      /* blank rows between text lines on the page */
#define NLINES 5
#define MAXW  7      /* max glyph cells per line */

static const char *CHARSET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/* paint letter ch into cell [x0..x0+STRIP) at row band [y0..y0+STRIP) */
static void paint_letter(OcrImage *im, Font *f, int x0, int y0, char ch){
    uint8_t *bits=NULL; int w=0,h=0;
    int ok = font_rasterize(f,(uint32_t)ch,PPM,&bits,&w,&h);
    if(!ok||!bits){ if(bits)free(bits); return; }
    int ox=(STRIP-w)/2; if(ox<0)ox=0;
    int oy=(STRIP-h)/2; if(oy<0)oy=0;
    for(int y=0;y<h && y+oy<STRIP;y++) for(int x=0;x<w && x+ox<STRIP;x++)
        if(bits[y*w+x]) ocr_image_set(im, x0+ox+x, y0+oy+y, 235);
    free(bits);
}

/* Extract a STRIP-tall band centered on the detected ink band [ry0..ry1),
 * preserving vertical geometry to match training (glyph centered in a STRIP cell). */
static OcrImage *crop_line(const OcrImage *page, int ry0, int ry1){
    int W=(int)ocr_image_width(page), H=(int)ocr_image_height(page);
    int cy=(ry0+ry1)/2;
    int top=cy-STRIP/2;                 /* top of the STRIP window on the page */
    OcrImage *line=ocr_image_create(W,STRIP);
    for(int y=0;y<STRIP;y++){
        int py=top+y;
        for(int x=0;x<W;x++){
            uint8_t v = (py>=0 && py<H)? ocr_image_get(page,x,py) : 15;
            ocr_image_set(line,x,y,v);
        }
    }
    return line;
}

int main(int argc,char**argv){
    if(argc<2){ printf("usage: %s <font.ttf>  (LOAD=model)\n",argv[0]); return 1; }
    const char *LOAD=getenv("LOAD");
    rng=(uint32_t)time(NULL)|1u;

    size_t fn; uint8_t*fb=readf(argv[1],&fn);
    Font *font=fb?font_open(fb,fn):NULL;
    if(!font){ printf("font open failed\n"); return 1; }

    CRNN *m=NULL;
    if(LOAD){ if(!crnn_load(LOAD,&m)||!m){ printf("crnn_load failed: %s\n",LOAD); return 1; } }
    else { printf("no LOAD=model given; need a trained model\n"); return 1; }

    /* --- build a synthetic page: NLINES lines, each a random word --- */
    int pageW=MAXW*STRIP;
    int pageH=NLINES*STRIP + (NLINES+1)*GAP;
    OcrImage *page=ocr_image_create(pageW,pageH);
    for(int y=0;y<pageH;y++) for(int x=0;x<pageW;x++) ocr_image_set(page,x,y,15);
    char gt[NLINES][MAXW+1];
    for(int l=0;l<NLINES;l++){
        int L=3+(int)(rnd()*4.99f); if(L>MAXW)L=MAXW;
        int y0=GAP + l*(STRIP+GAP);
        for(int i=0;i<L;i++){ int li=(int)(rnd()*25.99f); if(li>25)li=25;
            gt[l][i]=CHARSET[li]; paint_letter(page,font,i*STRIP,y0,CHARSET[li]); }
        gt[l][L]='\0';
    }

    /* --- horizontal-projection line segmentation --- */
    int inrow[4096]={0};
    for(int y=0;y<pageH && y<4096;y++){ int ink=0;
        for(int x=0;x<pageW;x++) if(ocr_image_get(page,x,y)>128) ink++;
        inrow[y]=ink>0;
    }
    int found=0, correct_lines=0, tot_chars=0, ok_chars=0;
    int y=0;
    printf("=== DOCUMENT TRANSCRIPTION (%d lines) ===\n",NLINES);
    while(y<pageH){
        while(y<pageH && !inrow[y]) y++;
        if(y>=pageH) break;
        int y0=y; while(y<pageH && inrow[y]) y++;
        int y1=y;
        OcrImage *line=crop_line(page,y0,y1);
        char txt[64]; crnn_recognize(m,line,CHARSET,txt,sizeof txt);
        const char *g = found<NLINES? gt[found] : "";
        printf("line %d [y %d..%d]  GT=%-8s PRED=%-8s %s\n",
               found,y0,y1,g,txt, strcmp(g,txt)==0?"OK":"");
        if(strcmp(g,txt)==0) correct_lines++;
        for(int i=0;g[i];i++){ tot_chars++; if(txt[i]==g[i]) ok_chars++; }
        ocr_image_free(line);
        found++;
    }
    printf("segmented %d lines (expected %d); exact-line %d/%d; char-acc %d/%d (%.1f%%)\n",
           found,NLINES,correct_lines,NLINES,ok_chars,tot_chars,
           tot_chars?100.0*ok_chars/tot_chars:0.0);

    ocr_image_free(page); crnn_free(m); font_free(font); free(fb);
    return 0;
}
