/* crnn_lex_train.c -- REAL-WORD OCR training driven by a certifiable frequency
 * lexicon (data/wordlists/<lang>/<lang>_top{1k,10k}.txt).
 *
 * Tokenization methodology applied to 2011-era hardware: instead of training on
 * random gibberish, we sample REAL words by their corpus frequency (Zipf) via the
 * lexicon's Vose alias sampler. Common words dominate the mix, so the tiny CRNN
 * spends capacity on what actually appears in documents -- the same bias BPE and
 * friends exploit, but with zero transformer bloat. Charset (CTC classes) is
 * derived from the lexicon itself: class 0 = blank, 1..K = sorted codepoints.
 *
 * The line-rendering geometry (paint/crop/warp) is imported from ocr_render.h so
 * TRAINING and the photo front door share ONE code path (no train/infer drift).
 *
 * Usage:
 *   STRIDE=10 WARP=1 SAVE=/tmp/en.crnn crnn_lex_train <font.ttf> <wordlist.txt> [epochs] [lr]
 * Env: WARP(0) FRACMAX(0.5) JITMAX(6) WARM(0.5) NTR(400) MAXCH(12)
 */
#include "crnn.h"
#include "ocr_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define HID   48

/* lexicon class->codepoint callback for crnn_recognize_utf8 */
static uint32_t lex_cp_cb(int cls, void *u){
    Lexicon *lx=(Lexicon*)u;
    return cls>=1 ? lex_cp_of_class(lx,cls) : 0; /* cls 0 = blank */
}

int main(int argc,char**argv){
    if(argc<3){ printf("usage: %s <font.ttf> <wordlist.txt> [epochs] [lr]\n",argv[0]); return 1; }
    int EPOCHS=argc>3?atoi(argv[3]):120;
    float LR=argc>4?(float)atof(argv[4]):0.0015f;
    const char*SAVE=getenv("SAVE");
    int WARP=getenv("WARP")?atoi(getenv("WARP")):0;
    int NTR=getenv("NTR")?atoi(getenv("NTR")):400;
    int MAXCH=getenv("MAXCH")?atoi(getenv("MAXCH")):12;
    float FRACMAX=getenv("FRACMAX")?atof(getenv("FRACMAX")):0.5f;
    float JITMAX=getenv("JITMAX")?atof(getenv("JITMAX")):6.0f;
    float WARM=getenv("WARM")?atof(getenv("WARM")):0.5f;
    uint32_t rng=(uint32_t)time(NULL)|1u;

    Lexicon*lx=lex_load(argv[2], 0);
    if(!lx){ printf("lex load fail: %s\n",argv[2]); return 1; }
    int K=lex_charset_size(lx);
    int NCLASS=K+1; /* +blank */
    printf("lexicon: %d words, charset K=%d -> NCLASS=%d\n", lex_size(lx), K, NCLASS);

    size_t fn; uint8_t*fb=ocr_readf(argv[1],&fn); Font*font=fb?font_open(fb,fn):NULL;
    if(!font){ printf("font fail\n"); return 1; }

    ConvConfig3 cfg={STRIP,STRIP,4,2,2,8,2,2,16,1,1};
    CRNN*m=crnn_create(&cfg,STRIP,HID,NCLASS,1,4242);
    if(!m){ printf("create fail\n"); return 1; }
    if(getenv("STRIDE")) crnn_set_stride(m, atoi(getenv("STRIDE")));
    /* LOAD: reuse a previously trained model for eval/fine-tune instead of
     * creating a fresh random one. */
    const char *loadp=getenv("LOAD");
    if(loadp){
        CRNN *lm=NULL;
        if(crnn_load(loadp,&lm) && lm){ crnn_free(m); m=lm; printf("loaded %s\n",loadp); }
        else { printf("LOAD FAIL %s\n",loadp); return 1; }
    }
    /* optional font mixing: FONTS=f1:f2:... uses a random font per sample */
    Font **fonts=NULL; int nf=0;
    const char *fenv=getenv("FONTS");
    if(fenv && *fenv){
        int cnt=1; for(const char*p=fenv;*p;p++) if(*p==':') cnt++;
        fonts=calloc(cnt,sizeof(Font*));
        char buf[1024]; strncpy(buf,fenv,sizeof buf-1); buf[sizeof buf-1]=0;
        char *tok=strtok(buf,":");
        while(tok){ uint8_t*fb=ocr_readf(tok,&fn); Font*ff=fb?font_open(fb,fn):NULL; if(ff) fonts[nf++]=ff; tok=strtok(NULL,":"); }
        printf("fontmix: %d fonts loaded\n",nf);
    }
    int STRAUG = getenv("STRAUG")? atoi(getenv("STRAUG")):0;

    /* fixed pool of Zipf-sampled words; regen pixels (warp/aug) per epoch */
    int *widx=malloc(NTR*sizeof(int));
    int *tgts=malloc((size_t)NTR*MAXCH*sizeof(int));
    int *lens=malloc(NTR*sizeof(int));
    uint32_t *seed=malloc(NTR*sizeof(uint32_t));
    OcrImage **imgs=calloc(NTR,sizeof(OcrImage*));
    for(int n=0;n<NTR;n++){
        int L=0,tries=0;
        while(L==0 && tries<20){ widx[n]=lex_sample(lx,&rng); L=ocr_word_to_classes(lx,widx[n],tgts+n*MAXCH,MAXCH); tries++; }
        if(L==0){ tgts[n*MAXCH]=1; L=1; }
        lens[n]=L; seed[n]=(rng^(0x9E3779B9u*(uint32_t)(n+1)))|1u;
    }

    float prev=1e30f,best=1e30f; int step=0;
    for(int epoch=0;epoch<EPOCHS;epoch++){
        float lr=0.5f*LR*(1.0f+cosf(3.14159265f*epoch/(float)EPOCHS));
        float ramp=WARM>0?(float)epoch/(WARM*EPOCHS):1.0f; if(ramp>1)ramp=1;
        float wfrac=WARP?FRACMAX*ramp:0.0f; double jit=JITMAX*ramp;
        for(int n=0;n<NTR;n++){ if(imgs[n]) ocr_image_free(imgs[n]);
            int dw=(ocr_rndf(&seed[n])<wfrac);
            OcrImage *g = (nf>0)? ocr_gen_line_f(lx,fonts,nf,tgts+n*MAXCH,lens[n],dw,jit,WARP?1:0,&seed[n])
                               : ocr_gen_line(lx,font,tgts+n*MAXCH,lens[n],dw,jit,WARP?1:0,&seed[n]);
            if(STRAUG && g){ OcrImage*a=ocr_straug(g,&seed[n],1); if(a){ ocr_image_free(g); g=a; } }
            imgs[n]=g; }
        float tot=0;
        for(int n=0;n<NTR;n++){ if(!imgs[n]) continue;
            tot+=crnn_train_step(m,0,NULL,lens[n],tgts+n*MAXCH,imgs[n]);
            crnn_adam(m,lr,++step); }
        float mean=tot/NTR; if(mean<best)best=mean;
        if(epoch%10==0||epoch==EPOCHS-1) printf("epoch %3d mean_loss=%.4f (wfrac=%.2f jit=%.1f)\n",epoch,mean,wfrac,jit);
        prev=mean;
    }
    printf("final=%.4f best=%.4f\n",prev,best);
    if(SAVE){ if(crnn_save(m,SAVE)) printf("saved %s\n",SAVE); else printf("SAVE FAIL\n"); }

    /* eval: fresh Zipf-sampled words (clean), report raw + lexicon-corrected acc */
    int seq_ok=0,seq_ok_corr=0,ch_ok=0,ch_tot=0; int Neval=NTR;
    uint32_t er=rng|1u;
    for(int n=0;n<Neval;n++){
        int cls[64]; int L=0,tries=0,wi=0;
        while(L==0&&tries<20){ wi=lex_sample(lx,&er); L=ocr_word_to_classes(lx,wi,cls,MAXCH); tries++; }
        if(L==0) continue;
        OcrImage*ev=ocr_gen_line(lx,font,cls,L,0,0,0,&er); if(!ev) continue;
        char pred[256]; crnn_recognize_utf8(m,ev,lex_cp_cb,lx,pred,256);
        const char*gt=lex_word(lx,wi);
        int exact=(strcmp(pred,gt)==0); if(exact)seq_ok++;
        int d; int ci=lex_correct(lx,pred,2,&d);
        if(ci>=0 && strcmp(lex_word(lx,ci),gt)==0) seq_ok_corr++;
        uint32_t ga[64],pa[64]; int gl=0,pl=0,k2; const char*gp=gt,*pp=pred; uint32_t cp;
        while((k2=utf8_decode(gp,&cp))>0&&gl<64){gp+=k2;ga[gl++]=cp;}
        while((k2=utf8_decode(pp,&cp))>0&&pl<64){pp+=k2;pa[pl++]=cp;}
        for(int i=0;i<gl;i++){ ch_tot++; if(i<pl&&pa[i]==ga[i])ch_ok++; }
        ocr_image_free(ev);
    }
    printf("EVAL(clean): exact %d/%d (%.1f%%)  +lexicon-correct %d/%d (%.1f%%)  char-acc %d/%d (%.1f%%)\n",
        seq_ok,Neval,100.0*seq_ok/Neval, seq_ok_corr,Neval,100.0*seq_ok_corr/Neval, ch_ok,ch_tot,100.0*ch_ok/ch_tot);
    for(int n=0;n<6;n++){ int cls[64]; int L=0,wi=0,tries=0;
        while(L==0&&tries<20){ wi=lex_sample(lx,&er); L=ocr_word_to_classes(lx,wi,cls,MAXCH); tries++; }
        OcrImage*ev=ocr_gen_line(lx,font,cls,L,0,0,0,&er); char pred[256];
        crnn_recognize_utf8(m,ev,lex_cp_cb,lx,pred,256);
        int d; int ci=lex_correct(lx,pred,2,&d);
        printf("  GT=%-12s RAW=%-12s CORRECTED=%s\n", lex_word(lx,wi), pred, ci>=0?lex_word(lx,ci):"?");
        ocr_image_free(ev); }

    for(int n=0;n<NTR;n++) if(imgs[n]) ocr_image_free(imgs[n]);
    crnn_free(m); free(imgs); free(widx); free(tgts); free(lens); free(seed);
    lex_free(lx); font_free(font); free(fb);
    return 0;
}
