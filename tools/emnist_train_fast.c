/* emnist_train_fast.c -- Training harness for conv3 + MLP on EMNIST letters.
 *
 * Fixes applied (2026-07-22) to cure the prior ~random underfit:
 *   1. Deeper net: CN_ARCH selects CONV_2STAGE / CONV_MED / CONV_WIDE (3-stage)
 *      instead of the toy 2-stage FAST_CFG. Capacity is the #1 lever.
 *   2. Adam optimizer (per-param adaptive LR) -- directly fixes the ~30x conv/MLP
 *      gradient-magnitude imbalance that made SGD crawl. Plain SGD still available.
 *   3. LR warmup + cosine decay schedule (research: warmup + decay is near-optimal).
 *   4. Mild augmentation (random +/-shift, L/R flip) -- cited +~20% on basic CNNs.
 *   5. All gradient math already verified correct (convnet3_backward / mlp_backward
 *      / mlp_input_grad gradchecks PASS). This file only wires training.
 *
 * Usage: ./emnist_train_fast <data_dir> [epochs] [batch] [cap]
 * Env: CN_THREADS CN_LR CN_OPT=adam|sgd CN_ARCH=2stage|med|wide CN_LEAK CN_INORM
 *      CN_AUG=0|1 CN_WARMUP=epochs CN_DO_NORM=0|1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include "convnet3.h"
#include "mlp.h"

#define TW(i) ((float)(i)/255.0f)
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static uint32_t thr_rng(uint32_t *s) { *s^=*s<<13; *s^=*s>>17; *s^=*s<<5; return *s; }
static float frnd(uint32_t *s){ *s^=*s<<13; *s^=*s>>17; *s^=*s<<5; return (float)(*s&0xFFFFFF)/8388608.0f - 1.0f; }

static long idx_count(const unsigned char *hdr) {
    return ((long)hdr[4]<<24)|((long)hdr[5]<<16)|((long)hdr[6]<<8)|(long)hdr[7];
}
static int load_idx(const char *p, unsigned char **d, long *c) {
    FILE *f=fopen(p,"rb"); if(!f) return -1;
    unsigned char h[16]; if(fread(h,1,16,f)!=16){fclose(f);return -1;}
    long n=idx_count(h); int pic=1;
    if(h[3]==3){ int r=((int)h[8]<<24)|((int)h[9]<<16)|((int)h[10]<<8)|h[11];
                 int cc=((int)h[12]<<24)|((int)h[13]<<16)|((int)h[14]<<8)|h[15]; pic=r*cc; if(pic<1)pic=1; }
    unsigned char *b=malloc((size_t)n*pic);
    size_t g=fread(b,1,(size_t)n*pic,f); *c=(long)(g/pic); *d=b; fclose(f); return 0;
}

/* ---- Adam state (per parameter group) ---- */
typedef struct { float *m, *v; int n; } AdamBuf;
static AdamBuf *adam_new(int n){
    AdamBuf *a=malloc(sizeof(AdamBuf));
    a->m=calloc((size_t)n,sizeof(float)); a->v=calloc((size_t)n,sizeof(float)); a->n=n; return a;
}
static void adam_free(AdamBuf *a){ if(!a)return; free(a->m); free(a->v); free(a); }
/* Adam step: p -= lr * mhat/(sqrt(vhat)+eps); unbiased moments, b1=0.9 b2=0.999 */
static void adam_step(AdamBuf *a, float *p, const float *g, float lr, float b1, float b2, float eps, int t){
    float bc1 = 1.0f - powf(b1,(float)t);
    float bc2 = 1.0f - powf(b2,(float)t);
    for(int i=0;i<a->n;i++){
        float gi=g[i];
        a->m[i] = b1*a->m[i] + (1.0f-b1)*gi;
        a->v[i] = b2*a->v[i] + (1.0f-b2)*gi*gi;
        float mhat=a->m[i]/bc1, vhat=a->v[i]/bc2;
        p[i] -= lr * mhat/(sqrtf(vhat)+eps);
    }
}

/* augmentation: in-place shift by (dy,dx) within 28x28. NOTE: EMNIST letters
 * are orientation-locked, so NO horizontal/vertical flip -- flipping corrupts
 * the label (b<->d, p<->q, E<->mirrored E) while the target stays put, which
 * caps accuracy. The verified harness uses shift+jitter only. */
static void augment(const float *in, float *out, int dy, int dx){
    for(int y=0;y<28;y++) for(int x=0;x<28;x++){
        int sy=y-dy, sx=x-dx;
        float v=0;
        if(sy>=0&&sy<28&&sx>=0&&sx<28) v=in[(size_t)sy*28+sx];
        out[(size_t)y*28+x]=v;
    }
}

typedef struct { int tid; ConvNet3 *cn; MLP *m; const unsigned char *img,*lab; int label_off,D,nclass;
                 const float *zmean,*zstd; int do_norm,aug,pad_sz; long *idx,start,count; long valid; uint32_t rng; } TArg;

#define PAD_SZ 32
static void pad28(const float *im28, float *out, int S) {
    int off=(S-28)/2;
    for(int y=0;y<S;y++) for(int x=0;x<S;x++) {
        int sy=y-off, sx=x-off;
        out[(size_t)y*S+x] = (sy>=0&&sy<28&&sx>=0&&sx<28) ? im28[(size_t)sy*28+sx] : 0.0f;
    }
}

static void* worker(void* p) {
    TArg *a=(TArg*)p; long valid=0;
    convnet3_zero_grad(a->cn); mlp_zero_grad(a->m);
    for(long j=0;j<a->count;j++) {
        long n=a->idx[a->start+j]; int lab=a->lab[n]-a->label_off;
        if(lab<0||lab>=a->nclass) continue;
        const unsigned char *raw=a->img+(size_t)n*784;
        float im28[784], ima[784], imp[32*32];
        for(int q=0;q<784;q++) im28[q]=TW(raw[q]);
        if(a->aug){
            int dy=(int)(frnd(&a->rng)*2.0f), dx=(int)(frnd(&a->rng)*2.0f);
            augment(im28,ima,dy,dx);
        } else memcpy(ima,im28,sizeof(ima));
        pad28(ima,imp,a->pad_sz);
        float *feat=malloc((size_t)a->D*sizeof(float)); convnet3_forward(a->cn,imp,feat);
        if(a->do_norm) for(int d=0;d<a->D;d++) feat[d]=(feat[d]-a->zmean[d])/a->zstd[d];
        float sc[26]; mlp_forward(a->m,feat,sc);
        mlp_backward(a->m,feat,lab);
        float *df=malloc((size_t)a->D*sizeof(float)); mlp_input_grad(a->m,feat,df);
        /* Back out the z-norm transform before handing the gradient to the
         * conv: the loss gradient w.r.t. the conv output is dLoss/d(feat_norm)
         * but convnet3_backward expects dLoss/d(feat_raw). df_raw = df_norm /
         * zstd. (Cancels exactly under Adam, but REQUIRED for SGD to be correct.) */
        if(a->do_norm) for(int d=0;d<a->D;d++) df[d]/=a->zstd[d];
        convnet3_backward(a->cn,imp,feat,df);
        free(df); free(feat);
        valid++;
    }
    a->valid=valid; return NULL;
}

int main(int argc, char **argv) {
    const char *dir = (argc>1)? argv[1] : "data/emnist";
    int epochs = (argc>2)? atoi(argv[2]) : 20;
    long batch = (argc>3)? atol(argv[3]) : 128;
    long cap = (argc>4)? atol(argv[4]) : 0;  /* 0 = full train set */

    float base_lr = getenv("CN_LR") ? (float)atof(getenv("CN_LR")) : 0.001f;
    const char *opt = getenv("CN_OPT") ? getenv("CN_OPT") : "adam";
    int use_adam = (opt[0]=='a' || opt[0]=='A');
    const char *arch = getenv("CN_ARCH") ? getenv("CN_ARCH") : "med";
    int nth = getenv("CN_THREADS") ? atoi(getenv("CN_THREADS")) : 4;
    int nprocs = (int)sysconf(_SC_NPROCESSORS_ONLN); if(nprocs<1) nprocs=1;
    if(nth>nprocs) nth=nprocs; if(nth<1) nth=1;
    int warmup = getenv("CN_WARMUP") ? atoi(getenv("CN_WARMUP")) : 3;
    int do_aug = getenv("CN_AUG") ? atoi(getenv("CN_AUG")) : 1;
    int do_norm = getenv("CN_DO_NORM") ? atoi(getenv("CN_DO_NORM")) : 1;
    /* Conv front-end needs a STRONGER effective step than the MLP: its weight
     * gradients are ~30x smaller in magnitude (fewer params spread over the
     * spatial map + tiny input grad), so per-param Adam alone barely moves it.
     * conv_fac boosts the conv's step. The verified harness uses conv_fac~0.1
     * relative to a *large* MLP lr; here we instead scale the conv UP (the MLP
     * is already fine at base_lr), so conv_fac>1. Default 8.0. */
    float conv_fac = getenv("CN_CONVF") ? (float)atof(getenv("CN_CONVF")) : 8.0f;

    /* network config by arch name. The input size MUST match the pad size
     * fed to the conv (pad28 -- we pad 28x28 digits to SxS before forward).
     * A 28x28 net must be fed 28x28; a 32x32 (MED_PAD) net must be fed 32x32.
     * The previous version created 28x28 nets but padded to 32x32, silently
     * cropping the bottom/right 2 rows/cols of every digit (= bad data I/O). */
    ConvConfig3 CFG; int pad_sz;
    if(!strcmp(arch,"fast"))       { CFG = (ConvConfig3){28,28, 16,5,2,  32,5,2,  0,1,1}; pad_sz = 28; } /* 2-stage, 512 feats */
    else if(!strcmp(arch,"2stage")){ CFG = (ConvConfig3){28,28, 32,3,2,  64,3,2,  0,1,1}; pad_sz = 28; }
    else if(!strcmp(arch,"wide"))   { CFG = (ConvConfig3){28,28, 32,3,2,  64,3,2,  128,3,1}; pad_sz = 28; }
    else if(!strcmp(arch,"medpad")){ CFG = (ConvConfig3){32,32, 16,5,2,  32,5,2,  64,3,1}; pad_sz = 32; } /* MED_PAD: matches PAD_SZ */
    else                            { CFG = (ConvConfig3){32,32, 16,5,2,  32,5,2,  64,3,1}; pad_sz = 32; } /* med(default): now 32x32 to match pad */

    fprintf(stderr, "[fast] arch=%s opt=%s lr=%.4f threads=%d epochs=%d batch=%ld aug=%d norm=%d warmup=%d\n",
            arch, use_adam?"adam":"sgd", base_lr, nth, epochs, batch, do_aug, do_norm, warmup);

    char ptr[512], pte[512], ptw[512], ptel[512];
    snprintf(ptr,512,"%s/emnist-letters-train-images-idx3-ubyte",dir);
    snprintf(pte,512,"%s/emnist-letters-test-images-idx3-ubyte",dir);
    snprintf(ptw,512,"%s/emnist-letters-train-labels-idx1-ubyte",dir);
    snprintf(ptel,512,"%s/emnist-letters-test-labels-idx1-ubyte",dir);

    unsigned char *tr_img,*te_img,*tr_lab,*te_lab;
    long ntr,nte,ntw,nte_l;
    if(load_idx(ptr,&tr_img,&ntr)) return 1;
    if(load_idx(pte,&te_img,&nte)) return 1;
    if(load_idx(ptw,&tr_lab,&ntw)) return 1;
    if(load_idx(ptel,&te_lab,&nte_l)) return 1;

    if(cap>0 && cap<ntr) ntr=cap;

    ConvNet3 *cn = convnet3_create(&CFG);
    int use_in = getenv("CN_INORM") ? atoi(getenv("CN_INORM")) : 0;  /* research: simple CNNs skip INORM */
    if(use_in) convnet3_enable_inorm(cn);
    convnet3_set_leak(cn, 0.1f);
    int D = convnet3_dim(cn);
    MLP *m = mlp_create(D, 128, 64, 26, 0x1234ABCDu);

    /* z-norm stats (optional) */
    float *zmean=calloc((size_t)D,sizeof(float)), *zstd=malloc((size_t)D*sizeof(float));
    if(do_norm){
        double *sum=calloc((size_t)D,sizeof(double)), *sum2=calloc((size_t)D,sizeof(double));
        long nz = ntr<500 ? ntr : 500;
        for(long i=0;i<nz;i++){
            const unsigned char *raw=tr_img+i*784; float im[784],imp[32*32];
            for(int q=0;q<784;q++) im[q]=TW(raw[q]); pad28(im,imp,pad_sz);
            float *feat=malloc((size_t)D*sizeof(float)); convnet3_forward(cn,imp,feat);
            for(int d=0;d<D;d++){ sum[d]+=feat[d]; sum2[d]+=(double)feat[d]*feat[d]; } free(feat);
        }
        for(int d=0;d<D;d++){ double mu=sum[d]/nz, va=sum2[d]/nz-mu*mu; float sd=sqrtf((float)va); if(sd<1e-2f)sd=1; zmean[d]=(float)mu; zstd[d]=sd; }
        free(sum); free(sum2);
    } else { for(int d=0;d<D;d++){ zmean[d]=0; zstd[d]=1; } }

    /* Thread replicas */
    ConvNet3 **cnT = calloc((size_t)nth, sizeof(ConvNet3*));
    MLP      **mT  = calloc((size_t)nth, sizeof(MLP*));
    for(int t=0;t<nth;t++){ cnT[t]=convnet3_gradbuf(cn); mT[t]=mlp_gradbuf(m); }

    /* Adam buffers for shared model (conv + mlp param groups) */
    int mlp_ng=6; AdamBuf **mA=calloc((size_t)mlp_ng,sizeof(AdamBuf*));
    for(int g=0;g<mlp_ng;g++){ MLPLayer L=mlp_layer(m,g); mA[g]=adam_new(L.n); }
    int cn_ng=convnet3_layer_count(cn); AdamBuf **cA=calloc((size_t)cn_ng,sizeof(AdamBuf*));
    for(int g=0;g<cn_ng;g++){ ConvLayer3 L=convnet3_layer(cn,g); cA[g]=adam_new(L.n); }

    long nbatch = (ntr+batch-1)/batch;
    long *idx=malloc((size_t)ntr*sizeof(long));
    for(long i=0;i<ntr;i++) idx[i]=i;
    uint32_t s_rng=0x1234ABCDu;
    for(long i=ntr-1;i>0;i--){ long j=(long)((s_rng=(s_rng*1103515245u+12345u))%((uint32_t)i+1)); long t=idx[i]; idx[i]=idx[j]; idx[j]=t; }

    int tstep=0;
    for(int ep=0;ep<epochs;ep++) {
        /* cosine LR with linear warmup */
        float lr;
        if(ep < warmup) lr = base_lr * (float)(ep+1)/(float)warmup;
        else { float p=(float)(ep-warmup)/(float)(epochs-warmup>1?epochs-warmup:1); lr = base_lr*0.5f*(1.0f+cosf((float)M_PI*p)); }
        float conv_lr = lr * conv_fac;  /* boost conv step so features actually move */

        for(long b=0;b<nbatch;b++) {
            convnet3_zero_grad(cn); mlp_zero_grad(m);
            long base=b*batch, remain=ntr-base; if(remain>batch)remain=batch;
            long per=remain/nth, extra=remain%nth, pos=base;
            pthread_t th[16]; TArg ta[16];
            for(int t=0;t<nth;t++) {
                long c=per+(t<extra?1:0);
                ta[t]=(TArg){.tid=t,.cn=cnT[t],.m=mT[t],.img=tr_img,.lab=tr_lab,.label_off=1,
                    .D=D,.do_norm=do_norm,.aug=do_aug,.pad_sz=pad_sz,.nclass=26,.zmean=zmean,.zstd=zstd,.idx=idx,.start=pos,.count=c,
                    .rng=0x1234ABCDu+1u+(uint32_t)t*2654435761u + (uint32_t)(ep*131+b)};
                pos+=c; pthread_create(&th[t],NULL,worker,&ta[t]);
            }
            long cnt=0; for(int t=0;t<nth;t++){ pthread_join(th[t],NULL); cnt+=ta[t].valid; }
            if(cnt==0) continue;
            for(int t=0;t<nth;t++){ convnet3_add_grad(cn,cnT[t]); mlp_add_grad(m,mT[t]); }
            mlp_scale_grad(m,1.0f/(float)cnt);
            convnet3_scale_grad(cn,1.0f/(float)cnt);

            tstep++;
            if(use_adam){
                for(int g=0;g<mlp_ng;g++){ MLPLayer L=mlp_layer(m,g); adam_step(mA[g],L.param,L.grad,lr,0.9f,0.999f,1e-8f,tstep); }
                for(int g=0;g<cn_ng;g++){ ConvLayer3 L=convnet3_layer(cn,g); adam_step(cA[g],L.param,L.grad,conv_lr,0.9f,0.999f,1e-8f,tstep); }
            } else {
                for(int g=0;g<mlp_ng;g++){ MLPLayer L=mlp_layer(m,g); for(int i=0;i<L.n;i++) L.param[i]-=lr*L.grad[i]; }
                for(int g=0;g<cn_ng;g++){ ConvLayer3 L=convnet3_layer(cn,g); for(int i=0;i<L.n;i++) L.param[i]-=conv_lr*L.grad[i]; }
            }
        }
        /* train accuracy + loss */
        long chk=ntr<2000?ntr:2000, cor=0; double lossum=0; int nloss=0;
        for(long i=0;i<chk;i++){
            const unsigned char *raw=tr_img+i*784; float im[784],imp[32*32];
            for(int q=0;q<784;q++) im[q]=TW(raw[q]); pad28(im,imp,pad_sz);
            float *feat=malloc((size_t)D*sizeof(float)); convnet3_forward(cn,imp,feat);
            for(int d=0;d<D;d++) feat[d]=(feat[d]-zmean[d])/zstd[d];
            float sc[26]; mlp_forward(m,feat,sc);
            float mx=sc[0]; for(int c=1;c<26;c++) if(sc[c]>mx)mx=sc[c];
            float sm[26],s2=0; for(int c=0;c<26;c++){ sm[c]=expf(sc[c]-mx); s2+=sm[c]; }
            int tgt=tr_lab[i]-1; if(tgt>=0&&tgt<26){ lossum+=-logf(sm[tgt]/s2); nloss++; }
            int best=0; for(int c=1;c<26;c++) if(sc[c]>sc[best]) best=c; if(best==tgt) cor++;
            free(feat);
        }
        printf("  epoch %d: train_acc=%.2f%% loss=%.3f lr=%.5f\n", ep+1, 100.0f*(float)cor/(float)chk, (float)(lossum/nloss), lr);
        fflush(stdout);
    }

    /* Test accuracy */
    long correct=0;
    for(long i=0;i<nte;i++){
        const unsigned char *raw=te_img+i*784; float im[784],imp[32*32];
        for(int q=0;q<784;q++) im[q]=TW(raw[q]); pad28(im,imp,pad_sz);
        float *feat=malloc((size_t)D*sizeof(float)); convnet3_forward(cn,imp,feat);
        for(int d=0;d<D;d++) feat[d]=(feat[d]-zmean[d])/zstd[d];
        float sc[26]; mlp_forward(m,feat,sc);
        int best=0; for(int c=1;c<26;c++) if(sc[c]>sc[best]) best=c;
        if(best==te_lab[i]-1) correct++;
        free(feat);
    }
    printf("\n=== RESULT (arch=%s opt=%s lr=%.4f aug=%d) ===\n", arch, use_adam?"adam":"sgd", base_lr, do_aug);
    printf("OVERALL ACCURACY: %.2f%% (%ld/%ld)\n", 100.0f*(float)correct/(float)nte, correct, nte);

    for(int g=0;g<mlp_ng;g++) adam_free(mA[g]); free(mA);
    for(int g=0;g<cn_ng;g++) adam_free(cA[g]); free(cA);
    for(int t=0;t<nth;t++){ convnet3_destroy(cnT[t]); mlp_destroy(mT[t]); }
    free(cnT); free(mT); free(idx); free(zmean); free(zstd);
    free(tr_img); free(te_img); free(tr_lab); free(te_lab);
    convnet3_destroy(cn); mlp_destroy(m);
    return 0;
}
