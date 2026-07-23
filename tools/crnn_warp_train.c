/* crnn_warp_train.c -- MATCHED-GEOMETRY OCR training.
 *
 * The photo inference path is: render -> perspective warp -> lens_flatten ->
 * scale-normalized line crop -> recognize. Training on perfectly-centered clean
 * glyphs therefore sees a DIFFERENT distribution than inference (train/test gap).
 *
 * This trainer generates each sample through the SAME warp+lens+crop chain, with
 * a fresh RANDOM quad every epoch (online). The recognizer thus learns on exactly
 * the residual distortion lens_flatten produces. STRIDE overlapping strips give
 * off-grid tolerance. Same CRNN core, save/load, cosine LR, per-sample update.
 *
 * Usage: STRIDE=10 SAVE=/tmp/latin_warp.crnn crnn_warp_train <font.ttf> [epochs] [lr]
 */
#include "crnn.h"
#include "image.h"
#include "lens.h"
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
static float rndf(uint32_t *s){ uint32_t r=*s; r^=r<<13; r^=r>>17; r^=r<<5; *s=r; return (float)(r&0xFFFFFF)/(float)0xFFFFFF; }

#define STRIP  20
#define HID    48
#define MAXLEN 6
#define PPM    16
#define GAP    8
#define NCLASS 27      /* blank + A..Z */
#define NTR    200

/* render letter centered into a STRIP cell of a page at (x0,y0) */
static void paint(OcrImage *im, Font *f, int x0, int y0, char ch){
    uint8_t *bits=NULL; int w=0,h=0;
    if(!font_rasterize(f,(uint32_t)ch,PPM,&bits,&w,&h)||!bits){ if(bits)free(bits); return; }
    int ox=(STRIP-w)/2; if(ox<0)ox=0; int oy=(STRIP-h)/2; if(oy<0)oy=0;
    for(int y=0;y<h&&y+oy<STRIP;y++) for(int x=0;x<w&&x+ox<STRIP;x++)
        if(bits[y*w+x]) ocr_image_set(im,x0+ox+x,y0+oy+y,235);
    free(bits);
}

/* scale-normalizing line crop -- IDENTICAL to crnn_photo_demo.c crop_line so
 * training geometry == inference geometry. */
static OcrImage *crop_norm(const OcrImage *page, int ry0, int ry1){
    int W=(int)ocr_image_width(page),H=(int)ocr_image_height(page);
    int top=-1,bot=-1;
    for(int y=ry0;y<ry1 && y<H;y++){ int ink=0; for(int x=0;x<W;x++) if(ocr_image_get(page,x,y)>128)ink++;
        if(ink>0){ if(top<0)top=y; bot=y; } }
    if(top<0){ top=ry0; bot=ry1-1; }
    int bh=bot-top+1; if(bh<1)bh=1;
    int target=16; double sy=(double)target/bh; int oy=(STRIP-target)/2;
    OcrImage *line=ocr_image_create(W,STRIP);
    for(int y=0;y<STRIP;y++) for(int x=0;x<W;x++) ocr_image_set(line,x,y,15);
    for(int dy=0;dy<target;dy++){ int srcy=top+(int)(dy/sy);
        if(srcy<0||srcy>=H) continue;
        for(int x=0;x<W;x++) ocr_image_set(line,x,oy+dy, ocr_image_get(page,x,srcy)); }
    return line;
}

/* Generate one sample line image via render->warp->lens->crop. word classes in
 * tgt[0..L). warp!=0 applies a random quad; else clean (control). Returns a new
 * OcrImage the caller frees. */
static OcrImage *gen_warp_line(Font *font, const int *tgt, int L, int warp, double jit, uint32_t *rs){
    int pw=MAXLEN*STRIP, ph=STRIP+2*GAP;
    OcrImage *pg=ocr_image_create(pw,ph);
    for(int y=0;y<ph;y++) for(int x=0;x<pw;x++) ocr_image_set(pg,x,y,15);
    for(int i=0;i<L;i++) paint(pg,font,i*STRIP,GAP,'A'+(tgt[i]-1));
    if(!warp){
        /* clean control: just crop the band */
        OcrImage *ln=crop_norm(pg,0,ph); ocr_image_free(pg); return ln;
    }
    /* perspective warp into a 2x canvas with a random quad, then lens de-warp */
    int cw=pw*2, chh=ph*2;
    OcrImage *cv=ocr_image_create(cw,chh);
    for(int y=0;y<chh;y++) for(int x=0;x<cw;x++) ocr_image_set(cv,x,y,15);
    /* random corner jitter around a nominal inset rectangle */
    double j = jit;
    double d[4][2]={
        {10+rndf(rs)*j,        12+rndf(rs)*j},                 /* TL */
        {cw-20-rndf(rs)*j,     8+rndf(rs)*j},                  /* TR */
        {cw-12-rndf(rs)*j,     chh-14-rndf(rs)*j},             /* BR */
        {14+rndf(rs)*j,        chh-18-rndf(rs)*j} };           /* BL */
    for(int py=0;py<ph;py++){ double v=(double)py/(ph-1);
        for(int px=0;px<pw;px++){ double u=(double)px/(pw-1);
            double tx=d[0][0]+(d[1][0]-d[0][0])*u, ty=d[0][1]+(d[1][1]-d[0][1])*u;
            double bx=d[3][0]+(d[2][0]-d[3][0])*u, by=d[3][1]+(d[2][1]-d[3][1])*u;
            double X=tx+(bx-tx)*v, Y=ty+(by-ty)*v;
            uint8_t g=ocr_image_get(pg,px,py);
            int ix=(int)(X+0.5), iy=(int)(Y+0.5);
            for(int a=0;a<2;a++) for(int b=0;b<2;b++) ocr_image_set(cv,ix+b,iy+a,g);
        } }
    Pt2 cor[4]={{d[0][0],d[0][1]},{d[1][0],d[1][1]},{d[2][0],d[2][1]},{d[3][0],d[3][1]}};
    OcrImage *fl=lens_flatten(cv,cor,pw,ph,1);
    ocr_image_free(pg); ocr_image_free(cv);
    if(!fl) return NULL;
    /* find ink band + crop-normalize (same as inference) */
    int H=(int)ocr_image_height(fl),W=(int)ocr_image_width(fl),y0=-1,y1=-1;
    for(int y=0;y<H;y++){ int ink=0; for(int x=0;x<W;x++) if(ocr_image_get(fl,x,y)>128)ink++;
        if(ink){ if(y0<0)y0=y; y1=y; } }
    if(y0<0){ y0=0; y1=H-1; }
    OcrImage *ln=crop_norm(fl,y0,y1+1);
    ocr_image_free(fl);
    return ln;
}

int main(int argc,char**argv){
    if(argc<2){ printf("usage: %s <font.ttf> [epochs] [lr]\n",argv[0]); return 1; }
    int EPOCHS = argc>2? atoi(argv[2]) : 120;
    float LR   = argc>3? (float)atof(argv[3]) : 0.0015f;
    const char *SAVE=getenv("SAVE");
    int WARP = getenv("WARP")? atoi(getenv("WARP")) : 1;   /* 1 = matched-geometry warp */
    uint32_t rng=(uint32_t)time(NULL)|1u;

    size_t fn; uint8_t*fb=readf(argv[1],&fn);
    Font *font=fb?font_open(fb,fn):NULL; if(!font){ printf("font fail\n"); return 1; }

    ConvConfig3 cfg={STRIP,STRIP,4,2,2,8,2,2,16,1,1};
    CRNN *m=crnn_create(&cfg,STRIP,HID,NCLASS,1,4242);
    if(!m){ printf("create fail\n"); return 1; }
    if(getenv("STRIDE")) crnn_set_stride(m, atoi(getenv("STRIDE")));
    printf("stride=%d warp=%d epochs=%d lr=%.4f\n", crnn_get_stride(m), WARP, EPOCHS, LR);

    /* fix word content/length + per-sample RNG once; pixels regenerate per epoch */
    int *tgts=malloc((size_t)NTR*MAXLEN*sizeof(int));
    int *lens=malloc(NTR*sizeof(int));
    uint32_t *seed=malloc(NTR*sizeof(uint32_t));
    OcrImage **imgs=calloc(NTR,sizeof(OcrImage*));
    for(int n=0;n<NTR;n++){
        int L=3+(int)(rndf(&rng)*3.99f); if(L>MAXLEN)L=MAXLEN; lens[n]=L;
        for(int i=0;i<L;i++){ int li=(int)(rndf(&rng)*25.99f); if(li>25)li=25; tgts[n*MAXLEN+i]=li+1; }
        seed[n]=(rng ^ (0x9E3779B9u*(uint32_t)(n+1)))|1u;
    }

    float prev=1e30f,best=1e30f; int step=0;
    /* MIXED CURRICULUM: warp fraction and jitter severity both ramp up over the
     * first WARMFRAC of training, so the model first learns clean glyphs then is
     * gradually exposed to harder warped ones. Env knobs:
     *   FRACMAX (default 0.7) = final fraction of samples that are warped
     *   JITMAX  (default 8)   = final corner-jitter magnitude in px
     *   WARM    (default 0.6) = fraction of epochs over which to ramp to max     */
    float FRACMAX = getenv("FRACMAX")? atof(getenv("FRACMAX")) : 0.70f;
    float JITMAX  = getenv("JITMAX")?  atof(getenv("JITMAX"))  : 8.0f;
    float WARM    = getenv("WARM")?    atof(getenv("WARM"))    : 0.60f;
    for(int epoch=0;epoch<EPOCHS;epoch++){
        float lr=0.5f*LR*(1.0f+cosf(3.14159265f*epoch/(float)EPOCHS));
        float ramp = WARM>0? (float)epoch/(WARM*EPOCHS) : 1.0f; if(ramp>1)ramp=1;
        float warp_frac = WARP? FRACMAX*ramp : 0.0f;
        double jit_now  = JITMAX*ramp;
        /* online: regenerate every sample; per-sample coin decides clean vs warp */
        int nwarp=0;
        for(int n=0;n<NTR;n++){
            if(imgs[n]) ocr_image_free(imgs[n]);
            int do_warp = (rndf(&seed[n]) < warp_frac);
            if(do_warp) nwarp++;
            imgs[n]=gen_warp_line(font,tgts+n*MAXLEN,lens[n],do_warp,jit_now,&seed[n]);
        }
        float tot=0;
        for(int n=0;n<NTR;n++){
            if(!imgs[n]) continue;
            tot += crnn_train_step(m,0,NULL,lens[n],tgts+n*MAXLEN,imgs[n]);
            crnn_adam(m,lr,++step);
        }
        float mean=tot/NTR; if(mean<best)best=mean;
        if(epoch%10==0||epoch==EPOCHS-1)
            printf("epoch %3d mean_loss=%.4f  (warp_frac=%.2f jit=%.1f nwarp=%d)\n",
                   epoch,mean,warp_frac,jit_now,nwarp);
        prev=mean;
    }
    printf("final=%.4f best=%.4f\n",prev,best);
    if(SAVE){ if(crnn_save(m,SAVE)) printf("saved %s\n",SAVE); else printf("SAVE FAIL\n"); }

    /* eval separately on CLEAN and full-severity WARP batches */
    for(int mode=0;mode<2;mode++){
        int seq_ok=0,ch_ok=0,ch_tot=0,pred[64];
        for(int n=0;n<NTR;n++){
            OcrImage *ev=gen_warp_line(font,tgts+n*MAXLEN,lens[n],mode,JITMAX,&seed[n]);
            if(!ev) continue;
            int pl=crnn_predict(m,ev,pred); int L=lens[n]; int ok=(pl==L);
            for(int i=0;i<L;i++){ ch_tot++; if(i<pl&&pred[i]==tgts[n*MAXLEN+i])ch_ok++; else ok=0; }
            if(ok)seq_ok++;
            ocr_image_free(ev);
        }
        printf("EVAL(%s): exact %d/%d (%.1f%%) char-acc %d/%d (%.1f%%)\n",
               mode?"warp":"clean", seq_ok,NTR,100.0*seq_ok/NTR,ch_ok,ch_tot,100.0*ch_ok/ch_tot);
    }
    /* show 3 warped samples */
    for(int n=0;n<3;n++){
        OcrImage *ev=gen_warp_line(font,tgts+n*MAXLEN,lens[n],1,JITMAX,&seed[n]);
        char txt[64]; crnn_recognize(m,ev,"ABCDEFGHIJKLMNOPQRSTUVWXYZ",txt,64);
        printf("  GT="); for(int i=0;i<lens[n];i++) putchar('A'+tgts[n*MAXLEN+i]-1);
        printf(" PRED=%s\n",txt); ocr_image_free(ev);
    }

    for(int n=0;n<NTR;n++) if(imgs[n]) ocr_image_free(imgs[n]);
    crnn_free(m); free(imgs); free(tgts); free(lens); free(seed); font_free(font); free(fb);
    return 0;
}
