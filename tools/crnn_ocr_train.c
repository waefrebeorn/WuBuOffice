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

/* Charset: class k in 1..nclass-1 maps to g_cp[k-1] (a Unicode codepoint);
 * class 0 is the CTC blank. Configurable via CHARS env in two forms:
 *   - a literal string of (BMP) characters, e.g. "ABΓΔЀЯ"  -> each char is one class
 *   - a comma/space list of U+XXXX codepoints, e.g. "U+0391,U+0410"
 * so multilingual (Greek, Cyrillic, ...) training is first-class. */
static uint32_t *g_cp = NULL;   /* codepoints, length nclass-1 */
static int       g_ncp = 0;

/* Parse the CHARS env into g_cp. Returns number of codepoints (0 on failure). */
static int parse_charset(const char *s){
    if(g_cp) free(g_cp);
    g_cp = NULL; g_ncp = 0;
    if(!s || !*s) return 0;
    /* count capacity (over-estimate) */
    int cap = (int)strlen(s) + 1;
    g_cp = malloc((size_t)cap * sizeof(uint32_t));
    if(!g_cp) return 0;
    const char *p = s;
    uint32_t acc = 0; int inhex = 0;
    while(*p){
        if(*p == 'U' && (p[1]=='+' || p[1]=='u')){
            /* hex codepoint follows */
            p += 2; uint32_t v = 0; int dig = 0;
            while(*p && ((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F'))){
                v = v*16 + (uint32_t)(*p<='9'? *p-'0' : (*p|0x20)-'a'+10); p++; dig++;
            }
            if(dig) g_cp[g_ncp++] = v;
            if(*p==','||*p==' ') p++;
            continue;
        }
        if(inhex){
            if((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F')){ acc = acc*16 + (uint32_t)(*p<='9'?*p-'0':(*p|0x20)-'a'+10); p++; continue; }
            else { g_cp[g_ncp++] = acc; acc=0; inhex=0; }
        }
        if(*p=='U' && p[1]=='+'){ continue; } /* handled above */
        if(*p==' '||*p==','){ p++; continue; }
        /* literal BMP char: decode minimal UTF-8 */
        unsigned char c = (unsigned char)*p++;
        uint32_t cp = c;
        if(c < 0x80) cp = c;
        else if((c>>5)==0x6){ cp = ((c&0x1F)<<6)|((unsigned char)*p&0x3F); p++; }
        else if((c>>4)==0xE){ cp = ((c&0xF)<<12)|(((unsigned char)*p&0x3F)<<6)|((unsigned char)(p[1])&0x3F); p+=2; }
        else { p++; continue; } /* skip 4-byte for now */
        g_cp[g_ncp++] = cp;
    }
    if(inhex) g_cp[g_ncp++] = acc;
    if(g_ncp==0){ free(g_cp); g_cp=NULL; }
    return g_ncp;
}

/* map a CRNN class (0=blank) to its Unicode codepoint for UTF-8 output */
static uint32_t cp_of_class(int cls, void *u){
    (void)u;
    if(cls<=0 || cls>g_ncp) return 0;   /* blank / out of range -> skip */
    return g_cp[cls-1];
}

/* Render letter `ch` centered into strip [x0..x0+STRIP), with augmentation:
 * dx/dy = pixel jitter, ppm = per-glyph size (varies scale). Background 15. */
static void paint_letter_aug(OcrImage *im, Font *f, int x0, uint32_t ch, int dx, int dy, int ppm){
    uint8_t *bits=NULL; int w=0,h=0;
    int ok = font_rasterize(f,ch,ppm,&bits,&w,&h);
    for(int y=0;y<STRIP;y++) for(int x=0;x<STRIP;x++) ocr_image_set(im,x0+x,y,15);
    if(!ok||!bits){ if(bits)free(bits); return; }
    int ox=(STRIP-w)/2+dx, oy=(STRIP-h)/2+dy;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) if(bits[y*w+x]){
        int px=x0+ox+x, py=oy+y;
        if(px>=0&&px<MAXLEN*STRIP&&py>=0&&py<STRIP) ocr_image_set(im,px,py,235);
    }
    free(bits);
}
/* Clean centered render (default PPM, no jitter). */
static void paint_letter(OcrImage *im, Font *f, int x0, uint32_t ch){
    paint_letter_aug(im,f,x0,ch,0,0,PPM);
}

/* (Re)generate one word sample into imgs[n] with target/len. If aug!=0, apply
 * fresh per-glyph jitter/scale so each epoch sees new distortions (online aug).
 * The word content (classes) is chosen from a per-sample seed so the target
 * stays STABLE across epochs while only the pixels re-randomize. */
static void gen_sample(OcrImage *im, Font *font, int *tgt, int L, int aug, uint32_t *rs){
    for(int y=0;y<STRIP;y++) for(int x=0;x<MAXLEN*STRIP;x++) ocr_image_set(im,x,y,15);
    for(int i=0;i<L;i++){
        uint32_t cp = (tgt[i]-1 < g_ncp) ? g_cp[tgt[i]-1] : '?';
        if(aug){
            int dx=(int)(rndf(rs)*3)-1;      /* +-1 px horizontal */
            int dy=(int)(rndf(rs)*5)-2;      /* +-2 px vertical  */
            int ppm=PPM-1+(int)(rndf(rs)*3); /* PPM in [15..17]  */
            paint_letter_aug(im,font,i*STRIP,cp,dx,dy,ppm);
        } else {
            paint_letter(im,font,i*STRIP,cp);
        }
    }
}

/* Rotate line `im` (width W, height H) by `deg` degrees about its center and
 * return a new image of the same W x H size. The canvas is padded vertically
 * (by the horizontal shift the rotation induces) before cropping back to H, so
 * glyph tops/bottoms aren't clipped. Used to teach the model to survive the
 * small rotations real scanned/photographed pages have. */
static OcrImage *rot_line(const OcrImage *im, double deg) {
    int W = (int)ocr_image_width(im), H = (int)ocr_image_height(im);
    double a = deg * 3.141592653589793 / 180.0, ca = cos(a), sa = sin(a);
    int pad = (int)(W * fabs(sa)) + 2;
    int ch = H + 2 * pad;
    OcrImage *pad_im = ocr_image_create((size_t)W, (size_t)ch);
    if (!pad_im) return NULL;
    for (int y = 0; y < ch; y++) for (int x = 0; x < W; x++) ocr_image_set(pad_im,(size_t)x,(size_t)y,15);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        ocr_image_set(pad_im,(size_t)x,(size_t)(y+pad), ocr_image_get(im,(size_t)x,(size_t)y));
    OcrImage *dst = ocr_image_create((size_t)W, (size_t)H);
    if (!dst) { ocr_image_free(pad_im); return NULL; }
    int cx = W/2, cy = ch/2;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int sx = (int)(cx + (x - cx)*ca + (y - H/2)*sa);
        int sy = (int)(cy - (x - cx)*sa + (y - H/2)*ca);
        uint8_t g = (sx>=0&&sx<W&&sy>=0&&sy<ch) ? ocr_image_get(pad_im,(size_t)sx,(size_t)sy) : 15;
        ocr_image_set(dst,(size_t)x,(size_t)y,g);
    }
    ocr_image_free(pad_im);
    return dst;
}

/* Photo-style distortion applied in place to a rendered line: a gentle
 * rotation (to match real page skew) + horizontal shear (slant) + light
 * salt-and-pepper noise, so the model learns to survive scanned/photographed
 * text instead of only pristine font renders. Keep each effect subtle: on a
 * 20px-tall line a large distortion just smears the glyphs into noise (the
 * model collapses to a constant prediction). */
static void photo_aug(OcrImage *im, uint32_t *rs){
    int H=(int)ocr_image_height(im), W=(int)ocr_image_width(im);
    /* rotation: up to ~ +-4 deg, matching real scanned/photo page skew */
    double rot = (rndf(rs)*2.0f-1.0f) * 4.0f;
    OcrImage *rim = rot_line(im, rot);
    if (!rim) return;
    /* slant: displace the top vs bottom of the line by at most ~2px total. */
    double slant = (rndf(rs)*2.0f-1.0f) * 2.0f;   /* -2..2 px across the height */
    OcrImage *tmp=ocr_image_create(W,H);
    for(int y=0;y<H;y++){
        double f = H>1 ? (double)(y-(H/2))/(H/2) : 0.0;   /* -1..1 down the line */
        int off = (int)(slant*f + (f>=0?0.5:-0.5));
        for(int x=0;x<W;x++){
            int sx=x-off;
            uint8_t g = (sx>=0&&sx<W)? ocr_image_get(rim,(size_t)sx,(size_t)y) : 15;
            ocr_image_set(tmp,(size_t)x,(size_t)y,g);
        }
    }
    ocr_image_free(rim);
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
    g_ncp = parse_charset(CHARS ? CHARS : "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if(g_ncp <= 0){ printf("charset parse failed\n"); return 1; }
    int nclass = g_ncp + 1;   /* blank + each codepoint */

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
    int *tgts = calloc((size_t)NTR*MAXLEN, sizeof(int));   /* zero-fill: CTC only reads target[0..L-1], but keep tail clean */
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

    /* evaluate --- IN-SAMPLE (the same samples we trained on; measures
     * memorization, always flatters) and HELD-OUT (freshly rendered, unseen
     * word content; this is the number that means anything). */
    int seq_ok=0,char_ok=0,char_tot=0,pred[64];
    for(int n=0;n<NTR;n++){
        int pl=crnn_predict(m,imgs[n],pred);
        int L=lens[n]; int ok=(pl==L);
        for(int i=0;i<L;i++){ char_tot++; if(i<pl&&pred[i]==tgts[n*MAXLEN+i])char_ok++; else ok=0; }
        if(ok)seq_ok++;
    }
    printf("DECODE(in-sample): exact-word %d/%d (%.1f%%)  char-acc %d/%d (%.1f%%)\n",
           seq_ok,NTR,100.0*seq_ok/NTR,char_ok,char_tot,100.0*char_ok/char_tot);

    /* held-out: a fresh set of words (new content + new pixels) */
    {
        int NH = NTR;   /* same count as a fair comparison */
        int *htg = calloc((size_t)NH*MAXLEN, sizeof(int));
        int *hlen = malloc(NH*sizeof(int));
        OcrImage **himgs = malloc(NH*sizeof(OcrImage*));
        uint32_t *hseed = malloc(NH*sizeof(uint32_t));
        for(int n=0;n<NH;n++){
            int L = 3 + (int)(rnd()*3.99f); if(L>MAXLEN)L=MAXLEN; hlen[n]=L;
            int nch = nclass - 1;
            for(int i=0;i<L;i++){ int li=(int)(rnd()*nch); if(li>nch-1)li=nch-1; htg[n*MAXLEN+i]=li+1; }
            hseed[n] = (rng ^ (0x85EBCA6Bu*(uint32_t)(n+7919))) | 1u;
            himgs[n] = ocr_image_create(MAXLEN*STRIP, STRIP);
            gen_sample(himgs[n], font, htg+n*MAXLEN, L, AUG, &hseed[n]);
            if(PHOTO) photo_aug(himgs[n], &hseed[n]);
        }
        int h_ok=0,hc_ok=0,hc_tot=0;
        for(int n=0;n<NH;n++){
            int pl=crnn_predict(m,himgs[n],pred);
            int L=hlen[n]; int ok=(pl==L);
            for(int i=0;i<L;i++){ hc_tot++; if(i<pl&&pred[i]==htg[n*MAXLEN+i])hc_ok++; else ok=0; }
            if(ok)h_ok++;
        }
        printf("DECODE(held-out): exact-word %d/%d (%.1f%%)  char-acc %d/%d (%.1f%%)\n",
               h_ok,NH,100.0*h_ok/NH,hc_ok,hc_tot,100.0*hc_ok/hc_tot);
        for(int n=0;n<NH;n++) ocr_image_free(himgs[n]);
        free(himgs); free(htg); free(hlen); free(hseed);
    }

    /* show 3 examples via the line-level string API */
    for(int n=0;n<3;n++){
        char txt[256];
        crnn_recognize_utf8(m, imgs[n], cp_of_class, NULL, txt, sizeof txt);
        printf("  GT="); for(int i=0;i<lens[n];i++){ uint32_t cp=g_cp[tgts[n*MAXLEN+i]-1]; if(cp<0x80) putchar((int)cp); else if(cp<0x800) printf("%c%c",0xC0|(cp>>6),0x80|(cp&0x3F)); else printf("%c%c%c",0xE0|(cp>>12),0x80|((cp>>6)&0x3F),0x80|(cp&0x3F)); }
        printf("  PRED=%s\n", txt);
    }

    for(int n=0;n<NTR;n++) ocr_image_free(imgs[n]);
    crnn_free(m); free(imgs); free(tgts); free(lens); free(seed); font_free(font); free(fb);
    return 0;
}
