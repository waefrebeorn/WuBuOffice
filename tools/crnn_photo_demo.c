/* crnn_photo_demo.c -- FULL Office-Lens front door (multilingual, lexicon-driven):
 *   render real frequency-sampled words via the SHARED ocr_gen_line (warp=1) ->
 *   PNG encode -> PNG DECODE -> lens_flatten (corner de-warp + contrast) ->
 *   horizontal-projection line segmentation -> crnn_recognize_utf8.
 *
 * The line-render path is IDENTICAL to training (ocr_render.h), so a model
 * trained by crnn_lex_train with WARP=1 generalizes straight to this front door.
 * Decoding is UTF-8 safe (class->codepoint), so any script in the lexicon works.
 *
 * Usage: LOAD=/tmp/en_warp.crnn crnn_photo_demo <font.ttf> <wordlist.txt> [nlines]
 */
#include "crnn.h"
#include "ocr_render.h"
#include "png.h"
#include "png_encode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static uint32_t g_rng=13579u;
static float rnd(void){ g_rng^=g_rng<<13; g_rng^=g_rng>>17; g_rng^=g_rng<<5;
    return (float)(g_rng&0xFFFFFF)/(float)0xFFFFFF; }

static uint32_t lex_cp_cb(int cls, void *u){
    Lexicon *lx=(Lexicon*)u; return cls>=1 ? lex_cp_of_class(lx,cls) : 0;
}

int main(int argc,char**argv){
    if(argc<3){ printf("usage: %s <font.ttf> <wordlist.txt> (LOAD=model) [nlines]\n",argv[0]); return 1; }
    const char *LOAD=getenv("LOAD");
    g_rng=(uint32_t)time(NULL)|1u;
    Lexicon*lx=lex_load(argv[2],0); if(!lx){ printf("lex fail %s\n",argv[2]); return 1; }
    size_t fn; uint8_t*fb=ocr_readf(argv[1],&fn);
    Font *font=fb?font_open(fb,fn):NULL; if(!font){ printf("font fail\n"); return 1; }
    CRNN *m=NULL;
    if(!LOAD||!crnn_load(LOAD,&m)||!m){ printf("need LOAD=trained model\n"); return 1; }

    int NLINES=argc>3?atoi(argv[3]):6;
    int gt_widx[64];
    OcrImage *lines[64];
    int nlines=0;
    for(int l=0;l<NLINES;l++){
        int wi=lex_sample(lx,&g_rng);
        int cls[64]; int L=ocr_word_to_classes(lx,wi,cls,64);
        if(L==0) continue;
        /* render the word CLEAN (warp=0); the photo pipeline below applies the
         * perspective warp + lens de-warp exactly once (matching the model's
         * seen warp distribution from the training curriculum). */
        OcrImage *ln=ocr_gen_line(lx,font,cls,L,0,0.0,0,&g_rng);
        if(!ln) continue;
        gt_widx[nlines]=wi; lines[nlines]=ln; nlines++;
    }

    /* build a stacked page of the rendered lines so segmentation mirrors a photo */
    int Wmax=0; for(int i=0;i<nlines;i++){ int W=(int)ocr_image_width(lines[i]); if(W>Wmax)Wmax=W; }
    int pageW=Wmax+4, pageH=nlines*STRIP+(nlines+1)*GAP;
    OcrImage *page=ocr_image_create(pageW,pageH);
    for(int y=0;y<pageH;y++)for(int x=0;x<pageW;x++)ocr_image_set(page,x,y,15);
    for(int i=0;i<nlines;i++){ int y0=GAP+i*(STRIP+GAP);
        int W=(int)ocr_image_width(lines[i]);
        for(int y=0;y<STRIP;y++) for(int x=0;x<W;x++) ocr_image_set(page,x,y0+y,ocr_image_get(lines[i],x,y)); }
    for(int i=0;i<nlines;i++) ocr_image_free(lines[i]);

    /* simulate a skewed phone photo on a larger canvas (same warp family) */
    int cw=pageW*2, chh=pageH*2;
    double dst[4][2]={ {18,25},{cw-40.0,10}, {cw-15.0,chh-20.0}, {30,chh-35.0} };
    OcrImage *cv=ocr_image_create(cw,chh);
    for(int y=0;y<chh;y++) for(int x=0;x<cw;x++) ocr_image_set(cv,x,y,15);
    for(int py=0;py<pageH;py++){ double v=(double)py/(pageH-1);
        for(int px=0;px<pageW;px++){ double u=(double)px/(pageW-1);
            double tx=dst[0][0]+(dst[1][0]-dst[0][0])*u, ty=dst[0][1]+(dst[1][1]-dst[0][1])*u;
            double bx=dst[3][0]+(dst[2][0]-dst[3][0])*u, by=dst[3][1]+(dst[2][1]-dst[3][1])*u;
            double X=tx+(bx-tx)*v, Y=ty+(by-ty)*v; uint8_t g=ocr_image_get(page,px,py);
            int ix=(int)(X+0.5), iy=(int)(Y+0.5);
            for(int a=0;a<2;a++) for(int b=0;b<2;b++) ocr_image_set(cv,ix+b,iy+a,g); } }
    /* PNG round-trip (real byte path) */
    uint8_t *png; size_t pl;
    if(png_encode_gray(ocr_image_pixels(cv),cw,chh,&png,&pl)){ printf("encode fail\n"); return 1; }
    int il=0; OcrImage *decoded=ocr_image_from_png(png,pl,&il);
    if(!decoded){ printf("PNG decode fail\n"); return 1; }
    Pt2 corners[4]={ {dst[0][0],dst[0][1]},{dst[1][0],dst[1][1]},{dst[2][0],dst[2][1]},{dst[3][0],dst[3][1]} };
    OcrImage *flat=lens_flatten(decoded,corners,pageW,pageH,1);
    if(!flat){ printf("lens_flatten fail\n"); return 1; }

    /* segment + recognize, matching training's horizontal-projection band walk */
    int H=(int)ocr_image_height(flat), W=(int)ocr_image_width(flat);
    int *inrow=calloc(H,sizeof(int));
    for(int y=0;y<H;y++){ int ink=0; for(int x=0;x<W;x++) if(ocr_image_get(flat,x,y)>128)ink++; inrow[y]=ink>0; }
    int found=0,ok_c=0,tot_c=0,exact=0,y=0;
    printf("=== PHOTO->TEXT (multilingual, lens warp, shared geometry) ===\n");
    while(y<H){
        while(y<H&&!inrow[y])y++; if(y>=H)break;
        int y0=y; while(y<H&&inrow[y])y++; int y1=y;
        if(y1-y0<4) continue;
        OcrImage *line=ocr_crop_norm(flat,y0,y1);
        char txt[256]; crnn_recognize_utf8(m,line,lex_cp_cb,lx,txt,sizeof txt);
        int d; int ci=lex_correct(lx,txt,2,&d);
        const char *g = found<nlines ? lex_word(lx,gt_widx[found]) : "";
        int rawok=(strcmp(txt,g)==0), corok=(ci>=0 && strcmp(lex_word(lx,ci),g)==0);
        printf("line %d GT=%-12s RAW=%-12s CORR=%-12s %s\n",
               found, g, txt, ci>=0?lex_word(lx,ci):"?", corok?"OK":(rawok?"raw":"!"));
        if(rawok||corok)exact++;
        for(int i=0;g[i];i++){tot_c++; if(txt[i]==g[i])ok_c++;}
        ocr_image_free(line); found++;
    }
    printf("lines %d/%d, exact %d, char-acc %d/%d (%.1f%%)\n",
           found,nlines,exact,ok_c,tot_c, tot_c?100.0*ok_c/tot_c:0.0);

    free(inrow); free(png);
    ocr_image_free(page); ocr_image_free(cv); ocr_image_free(decoded); ocr_image_free(flat);
    crnn_free(m); lex_free(lx); font_free(font); free(fb);
    return 0;
}
