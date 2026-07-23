#ifndef WUBU_OCR_RENDER_H
#define WUBU_OCR_RENDER_H
/* ocr_render.h -- SHARED line-rendering geometry for WuBuOCR.
 *
 * Provides paint_cp / crop_norm / gen_line / word_to_classes so the TRAINER
 * (crnn_lex_train) and the INFERENCE front door (crnn_photo_demo) render lines
 * with IDENTICAL geometry. If these ever diverge, train/inference drift and the
 * model reads garbage on real photos -- so keep both paths importing THIS file.
 *
 * All functions are `static inline` (header-only) to avoid extra link objects;
 * including in multiple .c files is fine (each gets its own copy).
 *
 * Needs: crnn.h's image.h, lens.h, lexicon.h, wubufont.h already included by the
 * includer. STRIP/PPM/GAP are defined by the includer (defaults here if absent).
 */
#include "image.h"
#include "lens.h"
#include "lexicon.h"
#include "wubufont.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef STRIP
#define STRIP 20
#endif
#ifndef PPM
#define PPM 16
#endif
#ifndef GAP
#define GAP 8
#endif

static inline uint8_t* ocr_readf(const char*p,size_t*n){ FILE*f=fopen(p,"rb"); if(!f)return 0;
    fseek(f,0,SEEK_END);*n=ftell(f);fseek(f,0,SEEK_SET);
    uint8_t*b=malloc(*n); if(fread(b,1,*n,f)!=*n){} fclose(f); return b; }
static inline float ocr_rndf(uint32_t*s){ uint32_t r=*s; r^=r<<13; r^=r>>17; r^=r<<5; *s=r; return (float)(r&0xFFFFFF)/(float)0x1000000; }

/* render one codepoint centered into a STRIP cell at (x0,y0) with optional jitter */
static inline void ocr_paint_cp(OcrImage*im,Font*f,int x0,int y0,uint32_t cp,int dx,int dy,int ppm){
    uint8_t*bits=NULL; int w=0,h=0;
    if(!font_rasterize(f,cp,ppm,&bits,&w,&h)||!bits){ if(bits)free(bits); return; }
    int ox=(STRIP-w)/2+dx, oy=(STRIP-h)/2+dy; if(ox<0)ox=0; if(oy<0)oy=0;
    for(int y=0;y<h&&y+oy<STRIP;y++) for(int x=0;x<w&&x+ox<STRIP;x++)
        if(bits[y*w+x]) ocr_image_set(im,x0+ox+x,y0+oy+y,235);
    free(bits);
}

/* scale-normalizing line crop: ink band -> ~PPM px tall, centered in STRIP.
 * Also X-trims to the ink extent (padded by 1 cell each side) so right-margin
 * blank/artifacts from de-warp aren't read as trailing glyphs. */
static inline OcrImage *ocr_crop_norm(const OcrImage *page,int ry0,int ry1){
    int W=(int)ocr_image_width(page),H=(int)ocr_image_height(page);
    int top=-1,bot=-1, lft=W, rgt=0;
    for(int y=ry0;y<ry1&&y<H;y++) for(int x=0;x<W;x++) if(ocr_image_get(page,x,y)>128){
        if(top<0)top=y; bot=y; if(x<lft)lft=x; if(x>rgt)rgt=x; }
    if(top<0){ top=ry0; bot=ry1-1; lft=0; rgt=W-1; }
    int bh=bot-top+1; if(bh<1)bh=1;
    int x0=(lft>=STRIP)? lft-STRIP : 0;
    int x1=(rgt+STRIP<W)? rgt+STRIP : W-1;
    int cw=x1-x0+1; if(cw<1)cw=1;
    int target=16; double sy=(double)target/bh; int oy=(STRIP-target)/2;
    OcrImage *ln=ocr_image_create(cw,STRIP);
    for(int y=0;y<STRIP;y++) for(int x=0;x<cw;x++) ocr_image_set(ln,x,y,15);
    for(int dy=0;dy<target;dy++){ int sry=top+(int)(dy/sy); if(sry<0||sry>=H) continue;
        for(int x=0;x<cw;x++) ocr_image_set(ln,x,oy+dy, ocr_image_get(page,x0+x,sry)); }
    return ln;
}

/* Render class sequence cls[0..L) as a line image; optional warp+lens dewarp.
 * `warp`!=0 forward-warps into a 2x canvas with a random perspective quad and
 * de-warps via lens_flatten (the SAME chain the photo front door uses). `aug`!=0
 * adds per-glyph dx/dy/scale jitter. rs is the per-sample RNG state. */
static inline OcrImage *ocr_gen_line(const Lexicon*lx,Font*font,const int*cls,int L,int warp,double jit,int aug,uint32_t*rs){
    int pw=L*STRIP, ph=STRIP+2*GAP;
    OcrImage*pg=ocr_image_create(pw,ph);
    for(int y=0;y<ph;y++) for(int x=0;x<pw;x++) ocr_image_set(pg,x,y,15);
    for(int i=0;i<L;i++){
        uint32_t cp=lex_cp_of_class(lx,cls[i]);
        int dx=0,dy=0,ppm=PPM;
        if(aug){ dx=(int)(ocr_rndf(rs)*3)-1; dy=(int)(ocr_rndf(rs)*5)-2; ppm=PPM-1+(int)(ocr_rndf(rs)*3); }
        ocr_paint_cp(pg,font,i*STRIP,GAP,cp,dx,dy,ppm);
    }
    if(!warp){ OcrImage*ln=ocr_crop_norm(pg,0,ph); ocr_image_free(pg); return ln; }
    int cw=pw*2,chh=ph*2; OcrImage*cv=ocr_image_create(cw,chh);
    for(int y=0;y<chh;y++) for(int x=0;x<cw;x++) ocr_image_set(cv,x,y,15);
    double j=jit;
    double d[4][2]={ {10+ocr_rndf(rs)*j,12+ocr_rndf(rs)*j},{cw-20-ocr_rndf(rs)*j,8+ocr_rndf(rs)*j},
                     {cw-12-ocr_rndf(rs)*j,chh-14-ocr_rndf(rs)*j},{14+ocr_rndf(rs)*j,chh-18-ocr_rndf(rs)*j} };
    for(int py=0;py<ph;py++){ double v=(double)py/(ph-1);
        for(int px=0;px<pw;px++){ double u=(double)px/(pw-1);
            double tx=d[0][0]+(d[1][0]-d[0][0])*u,ty=d[0][1]+(d[1][1]-d[0][1])*u;
            double bx=d[3][0]+(d[2][0]-d[3][0])*u,by=d[3][1]+(d[2][1]-d[3][1])*u;
            double X=tx+(bx-tx)*v,Y=ty+(by-ty)*v; uint8_t g=ocr_image_get(pg,px,py);
            int ix=(int)(X+0.5),iy=(int)(Y+0.5);
            for(int a=0;a<2;a++) for(int b=0;b<2;b++) ocr_image_set(cv,ix+b,iy+a,g); } }
    Pt2 cor[4]={{d[0][0],d[0][1]},{d[1][0],d[1][1]},{d[2][0],d[2][1]},{d[3][0],d[3][1]}};
    OcrImage*fl=lens_flatten(cv,cor,pw,ph,1); ocr_image_free(pg); ocr_image_free(cv);
    if(!fl) return NULL;
    int H=(int)ocr_image_height(fl),W=(int)ocr_image_width(fl),y0=-1,y1=-1;
    for(int y=0;y<H;y++){ int ink=0; for(int x=0;x<W;x++) if(ocr_image_get(fl,x,y)>128)ink++;
        if(ink){ if(y0<0)y0=y; y1=y; } }
    if(y0<0){ y0=0; y1=H-1; }
    OcrImage*ln=ocr_crop_norm(fl,y0,y1+1); ocr_image_free(fl); return ln;
}

/* Build a class sequence from a lexicon word (skips codepoints absent from charset). */
static inline int ocr_word_to_classes(const Lexicon*lx,int widx,int*cls,int maxch){
    const char*w=lex_word(lx,widx); if(!w) return 0;
    int L=0; uint32_t cp; int k;
    while((k=utf8_decode(w,&cp))>0 && L<maxch){ w+=k; int c=lex_class_of(lx,cp); if(c>0) cls[L++]=c; }
    return L;
}

/* STRAug (Spatial Transformer Augmentation, severe set — Cheng 2023): apply
 * mild geometric + intensity perturbations to an already-rendered line image.
 * Gated by `on`. Keeps text readable (no character-breaking moves) so the CTC
 * target still matches. Adds the kind of warp/rotation/shear/bleed real phone
 * photos have, which is what lets the model transfer off clean render. */
static inline OcrImage *ocr_straug(const OcrImage*src,uint32_t*rs,int on){
    if(!on) return NULL;
    int H=(int)ocr_image_height(src),W=(int)ocr_image_width(src);
    /* random small rotation about center (+-6 deg) */
    double ang=(ocr_rndf(rs)-0.5)*0.21; /* ~+-6deg */
    double ca=cos(ang),sa=sin(ang);
    OcrImage*dst=ocr_image_create(W,H);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) ocr_image_set(dst,x,y,15);
    int cx=W/2,cy=H/2;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        double dx=x-cx,dy=y-cy;
        int sx=(int)(cx+dx*ca+dy*sa+0.5), sy=(int)(cy-dx*sa+dy*ca+0.5);
        if(sx>=0&&sx<W&&sy>=0&&sy<H) ocr_image_set(dst,x,y,ocr_image_get(src,sx,sy));
    }
    /* intensity: contrast jitter + salt/pepper bleed */
    float contr=0.85f+ocr_rndf(rs)*0.3f;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        float v=ocr_image_get(dst,x,y); v=(v-128)*contr+128;
        if(ocr_rndf(rs)<0.02f) v = ocr_rndf(rs)<0.5f?255:0; /* 2% salt/pepper */
        if(v<0)v=0; if(v>255)v=255;
        ocr_image_set(dst,x,y,(uint8_t)v);
    }
    return dst;
}

/* Font-mixing variant: pick a font from a list (length nf) by index or random. */
static inline OcrImage *ocr_gen_line_f(const Lexicon*lx,Font**fonts,int nf,const int*cls,int L,int warp,double jit,int aug,uint32_t*rs){
    Font*f = (nf>0)? fonts[(int)(ocr_rndf(rs)*(double)nf)%nf] : NULL;
    return ocr_gen_line(lx,f,cls,L,warp,jit,aug,rs);
}

#endif
