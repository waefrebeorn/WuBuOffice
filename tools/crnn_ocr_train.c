/* crnn_ocr_train.c -- REAL glyph OCR: render text via wubufont, train CRNN+CTC.
 *
 * Charset: 'A'..'Z' -> classes 1..26 (0 = CTC blank). Each training sample is a
 * random 3-6 letter word rendered into a fixed-height line image (one glyph per
 * vertical strip of width STRIP), then conv->BiLSTM->head->CTC end-to-end.
 * Reports greedy-decode char accuracy on the training set.
 *
 * Usage: crnn_ocr_train <font.ttf> [epochs=60] [LR=0.0015]
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
static uint32_t rng=12345;
static float rnd(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return (float)(rng&0xFFFFFF)/(float)0xFFFFFF; }
/* stateful variant for per-epoch online augmentation */
static float rndf(uint32_t *s){ uint32_t r=*s; r^=r<<13; r^=r>>17; r^=r<<5; *s=r; return (float)(r&0xFFFFFF)/(float)0xFFFFFF; }

#define STRIP  20      /* glyph cell width & image height */
#define MAXLEN 12
#define PPM    16

/* Charset is configurable via the CHARS env var (default A..Z). Class k in
 * 1..nclass-1 maps to g_chars[k-1]; class 0 is the CTC blank. */
static const char *g_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/* Render letter `ch` centered into strip [x0..x0+STRIP), with augmentation:
 * dx/dy = pixel jitter, ppm = per-glyph size (varies scale). Background 15. */
static void paint_letter_aug(OcrImage *im, Font *f, int x0, char ch, int dx, int dy, int ppm){
    uint8_t *bits=NULL; int w=0,h=0;
    int ok = font_rasterize(f,(uint32_t)ch,ppm,&bits,&w,&h);
    for(int y=0;y<STRIP;y++) for(int x=0;x<STRIP;x++) ocr_image_set(im,x0+x,y,15);
    if(!ok||!bits){ if(bits)free(bits); return; }
    int ox=(STRIP-w)/2+dx, oy=(STRIP-h)/2+dy;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){
        int px=x0+ox+x, py=oy+y;
        if(bits[y*w+x] && px>=x0 && px<x0+STRIP && py>=0 && py<STRIP)
            ocr_image_set(im, px, py, 235);
    }
    free(bits);
}
/* Clean centered render (default PPM, no jitter). */
static void paint_letter(OcrImage *im, Font *f, int x0, char ch){
    paint_letter_aug(im,f,x0,ch,0,0,PPM);
}

/* (Re)generate one word sample into imgs[n] with target/len. If aug!=0, apply
 * fresh per-glyph jitter/scale so each epoch sees new distortions (online aug).
 * The word content (classes) is chosen from a per-sample seed so the target
 * stays STABLE across epochs while only the pixels re-randomize. */
static void gen_sample(OcrImage *im, Font *font, int *tgt, int L, int aug, uint32_t *rs){
    for(int y=0;y<STRIP;y++) for(int x=0;x<MAXLEN*STRIP;x++) ocr_image_set(im,x,y,15);
    for(int i=0;i<L;i++){
        char ch = g_chars[tgt[i]-1];
        if(aug){
            int dx=(int)(rndf(rs)*3)-1;      /* +-1 px horizontal */
            int dy=(int)(rndf(rs)*5)-2;      /* +-2 px vertical  */
            int ppm=PPM-1+(int)(rndf(rs)*3); /* PPM in [15..17]  */
            paint_letter_aug(im,font,i*STRIP,ch,dx,dy,ppm);
        } else {
            paint_letter(im,font,i*STRIP,ch);
        }
    }
}

/* Photo-style distortion applied in place to a rendered line: a gentle
 * horizontal shear (slant) plus light salt-and-pepper noise, so the model
 * learns to survive scanned/photographed text instead of only pristine font
 * renders. Keep it subtle: on a 20px-tall line, a large shear just smears the
 * glyphs into noise (the model collapses to a constant prediction). */
static void photo_aug(OcrImage *im, uint32_t *rs){
    int H=(int)ocr_image_height(im), W=(int)ocr_image_width(im);
    /* slant: displace the top vs bottom of the line by at most ~2px total. */
    double slant = (rndf(rs)*2.0f-1.0f) * 2.0f;   /* -2..2 px across the height */
    OcrImage *tmp=ocr_image_create(W,H);
    for(int y=0;y<H;y++){
        double f = H>1 ? (double)(y-(H/2))/(H/2) : 0.0;   /* -1..1 down the line */
        int off = (int)(slant*f + (f>=0?0.5:-0.5));
        for(int x=0;x<W;x++){
            int sx=x-off;
            uint8_t g = (sx>=0&&sx<W)? ocr_image_get(im,(size_t)sx,(size_t)y) : 15;
            ocr_image_set(tmp,(size_t)x,(size_t)y,g);
        }
    }
    int nn=(int)(rndf(rs)*(size_t)W*H*0.02f);  /* up to ~2% salt/pepper */
    for(int i=0;i<nn;i++){
        int x=(int)(rndf(rs)*W), y=(int)(rndf(rs)*H);
        uint8_t g=ocr_image_get(tmp,(size_t)x,(size_t)y);
        int r=(int)(rndf(rs)*100);
        uint8_t ng = r<10? 235 : (r>90? 15 : g);
        ocr_image_set(tmp,(size_t)x,(size_t)y,ng);
    }
    for(int y=0;y<H;y++) for(int x=0;x<W;x++)
        ocr_image_set(im,(size_t)x,(size_t)y, ocr_image_get(tmp,(size_t)x,(size_t)y));
    ocr_image_free(tmp);
}

int main(int argc,char**argv){
    if(argc<2){ printf("usage: %s <font.ttf> [epochs] [LR]\n",argv[0]); return 1; }
    int EPOCHS = argc>2? atoi(argv[2]) : 60;
    float LR   = argc>3? (float)atof(argv[3]) : 0.0015f;
    const char *SAVE = getenv("SAVE");   /* path to save trained model */
    const char *LOAD = getenv("LOAD");   /* path to load model (skip training) */
    rng = (uint32_t)time(NULL) | 1u;
    const char *CHARS = getenv("CHARS");
    g_chars = CHARS ? CHARS : "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int nclass = (int)strlen(g_chars) + 1;   /* blank + each char */

    size_t fn; uint8_t*fb=readf(argv[1],&fn);
    Font *font = fb? font_open(fb,fn):NULL;
    if(!font){ printf("font open failed: %s\n",argv[1]); return 1; }

    ConvConfig3 cfg={STRIP,STRIP,4,2,2,8,2,2,16,1,1};
    CRNN *m = NULL;
    if(LOAD){
        if(!crnn_load(LOAD,&m)||!m){ printf("crnn_load failed: %s\n",LOAD); return 1; }
        printf("loaded model from %s (skipping training)\n",LOAD);
        EPOCHS=0;
    } else {
        int hid = getenv("HID") ? atoi(getenv("HID")) : 32;
        m = crnn_create(&cfg, STRIP, hid, nclass, 1, 4242);
        if(!m){ printf("crnn_create failed\n"); return 1; }
        if(getenv("STRIDE")) crnn_set_stride(m, atoi(getenv("STRIDE")));
    }
    printf("stride=%d strip=%d\n", crnn_get_stride(m), STRIP);

    #define NTR_DFLT 400
    int NTR = getenv("NTR")? atoi(getenv("NTR")) : NTR_DFLT;
    int AUG = getenv("AUG")? atoi(getenv("AUG")) : 0;   /* 1 = online glyph jitter */
    int PHOTO = getenv("PHOTO")? atoi(getenv("PHOTO")) : 0; /* 1 = + line shear & salt-pepper noise */
    OcrImage **imgs = malloc(NTR*sizeof(OcrImage*));
    int *tgts = malloc((size_t)NTR*MAXLEN*sizeof(int));
    int *lens = malloc(NTR*sizeof(int));
    uint32_t *seed = malloc(NTR*sizeof(uint32_t));   /* per-sample aug RNG state */
    /* fix each word's CONTENT + length once; pixels (re)rendered per epoch */
    for(int n=0;n<NTR;n++){
        int L = 3 + (int)(rnd()*3.99f); if(L>MAXLEN)L=MAXLEN; lens[n]=L;
        int nch = nclass - 1;
        for(int i=0;i<L;i++){ int li=(int)(rnd()*nch); if(li>nch-1)li=nch-1; tgts[n*MAXLEN+i]=li+1; }
        seed[n] = (rng ^ (0x9E3779B9u*(uint32_t)(n+1))) | 1u;
        imgs[n] = ocr_image_create(MAXLEN*STRIP, STRIP);
        gen_sample(imgs[n], font, tgts+n*MAXLEN, L, AUG, &seed[n]);
        if(PHOTO) photo_aug(imgs[n], &seed[n]);
    }

    float prev=1e30f, best=1e30f; int step=0;
    for(int epoch=0; epoch<EPOCHS; epoch++){
        float lr = 0.5f*LR*(1.0f+cosf(3.14159265f*epoch/(float)EPOCHS)); /* cosine LR->0 */
        /* ONLINE AUGMENTATION: re-render every sample with fresh distortion so the
         * model sees new jitter each epoch instead of memorizing a fixed noisy set. */
        if(AUG && epoch>0){
            for(int n=0;n<NTR;n++){ gen_sample(imgs[n], font, tgts+n*MAXLEN, lens[n], AUG, &seed[n]); if(PHOTO) photo_aug(imgs[n], &seed[n]); }
        }
        float tot=0;
        for(int n=0;n<NTR;n++){
            tot += crnn_train_step(m, 0, NULL, lens[n], tgts+n*MAXLEN, imgs[n]);
            crnn_adam(m, lr, ++step);
        }
        float mean=tot/NTR; if(mean<best) best=mean;
        if(epoch%5==0||epoch==EPOCHS-1) printf("epoch %2d  mean_loss=%.4f\n", epoch, mean);
        prev=mean;
    }
    printf("final mean_loss=%.4f  best=%.4f\n", prev, best);
    if(SAVE){ if(crnn_save(m,SAVE)) printf("saved model to %s\n",SAVE); else printf("SAVE FAILED: %s\n",SAVE); }

    /* evaluate */
    int seq_ok=0,char_ok=0,char_tot=0,pred[64];
    for(int n=0;n<NTR;n++){
        int pl=crnn_predict(m,imgs[n],pred);
        int L=lens[n]; int ok=(pl==L);
        for(int i=0;i<L;i++){ char_tot++; if(i<pl&&pred[i]==tgts[n*MAXLEN+i])char_ok++; else ok=0; }
        if(ok)seq_ok++;
    }
    printf("DECODE: exact-word %d/%d (%.1f%%)  char-acc %d/%d (%.1f%%)\n",
           seq_ok,NTR,100.0*seq_ok/NTR,char_ok,char_tot,100.0*char_ok/char_tot);

    /* show 3 examples via the line-level string API */
    for(int n=0;n<3;n++){
        char txt[64];
        crnn_recognize(m, imgs[n], g_chars, txt, sizeof txt);
        printf("  GT="); for(int i=0;i<lens[n];i++) putchar(g_chars[tgts[n*MAXLEN+i]-1]);
        printf("  PRED=%s\n", txt);
    }

    for(int n=0;n<NTR;n++) ocr_image_free(imgs[n]);
    crnn_free(m); free(imgs); free(tgts); free(lens); free(seed); font_free(font); free(fb);
    return 0;
}
