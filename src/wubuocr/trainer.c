/* trainer.c -- training orchestration for conv3+MLP. C11, no deps. */

#include "trainer.h"
#include "convnet3.h"
#include "mlp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define PAD_SZ 32
#define MAX_THREADS 16

/* -------- Data loading -------- */

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

/* -------- Augmentation -------- */

static uint32_t xorshift(uint32_t *s) { *s^=*s<<13; *s^=*s>>17; *s^=*s<<5; return *s; }

static void rotate28(float *img, float *tmp, float deg) {
    float a=deg*(float)M_PI/180.0f, ca=cosf(a), sa=sinf(a), c=13.5f;
    for(int y=0;y<28;y++) for(int x=0;x<28;x++) {
        float dx=(float)x-c, dy=(float)y-c;
        float sx=dx*ca-dy*sa+c, sy=dx*sa+dy*ca+c;
        int x0=(int)floorf(sx), y0=(int)floorf(sy);
        float fx=sx-x0, fy=sy-y0; float v=0;
        for(int oy=0;oy<=1;oy++) for(int ox=0;ox<=1;ox++) {
            int xx=x0+ox, yy=y0+oy; if(xx<0||xx>27||yy<0||yy>27) continue;
            float w=(ox?fx:1-fx)*(oy?fy:1-fy); v+=w*img[yy*28+xx];
        }
        tmp[y*28+x]=v;
    }
    memcpy(img,tmp,28*28*sizeof(float));
}

static void jitter28(const unsigned char *src, float *dst, int dx, int dy) {
    for(int y=0;y<28;y++) for(int x=0;x<28;x++) {
        int sx=x-dx, sy=y-dy;
        float v = (sx>=0&&sx<28&&sy>=0&&sy<28) ? (float)src[(size_t)sy*28+sx] : 0.0f;
        dst[y*28+x]=v/255.0f;
    }
}

static void pad28(const float *im28, float *out, int S) {
    int off=(S-28)/2;
    for(int y=0;y<S;y++) for(int x=0;x<S;x++) {
        int sy=y-off, sx=x-off;
        out[(size_t)y*S+x] = (sy>=0&&sy<28&&sx>=0&&sx<28) ? im28[(size_t)sy*28+sx] : 0.0f;
    }
}

/* -------- Thread args -------- */

typedef struct {
    int tid;
    ConvNet3 *cn;
    MLP *m;
    const unsigned char *img, *lab;
    int label_off, D, nclass, do_norm, pad_sz;
    const float *zmean, *zstd;
    long *idx; long start, count;
    long valid;
    uint32_t rng;
    float aug_deg; int jit_px; float smooth;
} TArg;

/* -------- Worker -------- */

static void* worker(void* p) {
    TArg *a=(TArg*)p; long valid=0; static int printed_once=0;
    convnet3_zero_grad(a->cn); mlp_zero_grad(a->m);
    int ps = a->pad_sz;
    for(long j=0;j<a->count;j++) {
        long n=a->idx[a->start+j]; int lab=a->lab[n]-a->label_off;
        if(lab<0||lab>=a->nclass) continue;
        const unsigned char *raw=a->img+(size_t)n*784;
        float im[784], imp[1024];
        if(a->jit_px>0) {
            int dx=(int)(xorshift(&a->rng)%((unsigned)(a->jit_px*2+1)))-a->jit_px;
            int dy=(int)(xorshift(&a->rng)%((unsigned)(a->jit_px*2+1)))-a->jit_px;
            jitter28(raw,im,dx,dy);
        } else {
            for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
        }
        if(a->aug_deg>0.0f) {
            float imt[784]; float deg=((float)(xorshift(&a->rng)%1000)/1000.0f*2.0f-1.0f)*a->aug_deg;
            rotate28(im,imt,deg); for(int q=0;q<784;q++) im[q]=imt[q];
        }
        pad28(im,imp,ps);
        float *feat=malloc(sizeof(float)*(size_t)a->D);
        convnet3_forward(a->cn,imp,feat);
        if(a->do_norm) for(int d=0;d<a->D;d++) feat[d]=(feat[d]-a->zmean[d])/a->zstd[d];
        float sc[26]; mlp_forward(a->m,feat,sc);
        if(a->smooth>0.0f) mlp_backward_smooth(a->m,feat,lab,a->smooth);
        else mlp_backward(a->m,feat,lab);
        float *df=malloc(sizeof(float)*(size_t)a->D); mlp_input_grad(a->m,feat,df);
        if(!printed_once){ printed_once=1; double dz=0; for(long i=0;i<(long)a->D;i++) dz+=(double)df[i]*df[i];
          ConvLayer3 _L=convnet3_layer(a->cn,0); double gl=0; for(int i=0;i<_L.n;i++) gl+=(double)_L.grad[i]*_L.grad[i];
          fprintf(stderr,"[DBG] dz_L2=%.4f D=%d  per-sample conv_gw1_L2=%.4f n=%d\n", sqrtf((float)dz), a->D, sqrtf((float)gl), _L.n); }
        convnet3_backward(a->cn,imp,feat,df);
        free(feat); free(df);
        valid++;
    }
    a->valid=valid; return NULL;
}

/* -------- Trainer struct -------- */

struct Trainer {
    TrainConfig cfg;
    unsigned char *tr_img,*te_img,*tr_lab,*te_lab;
    long ntr, nte;
    long ntr_full;              /* true loaded train count (before traincap) */
    ConvNet3 *cn;
    MLP *m;
    float *zmean, *zstd;
    int D, in_sz;               /* in_sz: conv input size (28 or 32) */
    ConvNet3 **cnT;
    MLP **mT;
    long *idx;
    /* optimizer state */
    float *mvel[6], *mmsq[6];
    float **cvel, **cmsq;
    long tstep;
    int warm_eff;
    long total_steps;
};

/* -------- Optimizer helpers -------- */

static void opt_sgd_step(MLP *m, float lr, float mom, float clip, float *mvel[6]) {
    for(int g=0;g<6;g++){
        MLPLayer L=mlp_layer(m,g);
        float gn=0; for(int i=0;i<L.n;i++) gn+=L.grad[i]*L.grad[i]; gn=sqrtf(gn);
        float sc = (clip>0 && gn>clip) ? clip/gn : 1.0f;
        for(int i=0;i<L.n;i++){ float gv=L.grad[i]*sc; mvel[g][i]=mom*mvel[g][i]+gv; L.param[i]-=lr*mvel[g][i]; }
    }
}

static void opt_adam_step(MLP *m, float lr, long *tstep, float *mvel[6], float *mmsq[6]) {
    /* tstep is advanced once per batch by the caller (trainer_epoch), not here */
    float corr1=1.0f-powf(0.9f,(float)*tstep);
    float corr2=1.0f-powf(0.999f,(float)*tstep);
    float corr=sqrtf(corr2)/corr1;
    for(int g=0;g<6;g++){
        MLPLayer L=mlp_layer(m,g);
        for(int i=0;i<L.n;i++){
            float gv=L.grad[i];
            mvel[g][i]=0.9f*mvel[g][i]+0.1f*gv;
            mmsq[g][i]=0.999f*mmsq[g][i]+0.001f*gv*gv;
            float uh=mvel[g][i]/(sqrtf(mmsq[g][i])+1e-8f);
            L.param[i]-=lr*corr*uh;
        }
    }
}

static void opt_conv_sgd(ConvNet3 *cn, float lr, float mom, float clip, float **cvel) {
    int nlayers=convnet3_layer_count(cn);
    for(int g=0;g<nlayers;g++){
        ConvLayer3 L=convnet3_layer(cn,g);
        float gn=0; for(int i=0;i<L.n;i++) gn+=L.grad[i]*L.grad[i]; gn=sqrtf(gn);
        float sc = (clip>0 && gn>clip) ? clip/gn : 1.0f;
        for(int i=0;i<L.n;i++){ float gv=L.grad[i]*sc; cvel[g][i]=mom*cvel[g][i]+gv; L.param[i]-=lr*cvel[g][i]; }
    }
}

static void opt_conv_adam(ConvNet3 *cn, float lr, long *tstep, float **cvel, float **cmsq) {
    /* tstep is advanced once per batch by the caller (trainer_epoch), not here */
    float corr1=1.0f-powf(0.9f,(float)*tstep);
    float corr2=1.0f-powf(0.999f,(float)*tstep);
    float corr=sqrtf(corr2)/corr1;
    int nlayers=convnet3_layer_count(cn);
    for(int g=0;g<nlayers;g++){
        ConvLayer3 L=convnet3_layer(cn,g);
        for(int i=0;i<L.n;i++){
            float gv=L.grad[i];
            cvel[g][i]=0.9f*cvel[g][i]+0.1f*gv;
            cmsq[g][i]=0.999f*cmsq[g][i]+0.001f*gv*gv;
            float uh=cvel[g][i]/(sqrtf(cmsq[g][i])+1e-8f);
            L.param[i]-=lr*corr*uh;
        }
    }
}

/* -------- Trainer API -------- */

Trainer *trainer_create(const TrainConfig *cfg) {
    Trainer *tr=calloc(1,sizeof(*tr)); if(!tr) return NULL;
    tr->cfg=*cfg;

    /* Build paths */
    char ptr[512], pte[512], ptw[512], ptel[512];
    snprintf(ptr,512,"%s/%s-images-idx3-ubyte",cfg->data_dir,cfg->train_stem);
    snprintf(pte,512,"%s/%s-images-idx3-ubyte",cfg->data_dir,cfg->test_stem);
    snprintf(ptw,512,"%s/%s-labels-idx1-ubyte",cfg->data_dir,cfg->train_stem);
    snprintf(ptel,512,"%s/%s-labels-idx1-ubyte",cfg->data_dir,cfg->test_stem);
    if(load_idx(ptr,&tr->tr_img,&tr->ntr)||load_idx(pte,&tr->te_img,&tr->nte)||
       load_idx(ptw,&tr->tr_lab,&tr->ntr)||load_idx(ptel,&tr->te_lab,&tr->nte)) { trainer_destroy(tr); return NULL; }

    tr->ntr_full = tr->ntr;     /* keep true count before cap */
    if(cfg->traincap>0 && cfg->traincap<tr->ntr) tr->ntr=cfg->traincap;

    /* Model */
    ConvConfig3 med;
    int pad_sz;
    switch(cfg->arch) {
        case 1: /* WIDE: 28x28 input, 32→64→128, 1152 feats */
            med = CONV_WIDE;
            pad_sz = 28;
            break;
        case 2: /* XL: 28x28 input, 64→128→256, 2304 feats */
            med = CONV_XL;
            pad_sz = 28;
            break;
        case 3: /* BIGMAP: stride-1 convs, single pool -> ~7x7x128 (larger map) */
            med = CONV_BIGMAP;
            pad_sz = 32;
            break;
        case 4: /* 2-STAGE: gradcheck-verified correct backward (no stage3) */
            med = CONV_2STAGE;
            pad_sz = 28;
            break;
        default: /* MED_PAD: 32x32 padded, 16→32→64, 576 feats */
            med = CONV_MED_PAD;
            pad_sz = 32;
            break;
    }
    tr->cn=convnet3_create(&med);
    tr->in_sz = pad_sz;
    if(cfg->inorm) convnet3_enable_inorm(tr->cn);
    if(cfg->leak>0) convnet3_set_leak(tr->cn, cfg->leak);
    if(cfg->cbam) { convnet3_enable_cbam(tr->cn); }
    tr->D=convnet3_dim(tr->cn);
    tr->m=mlp_create(tr->D, cfg->h1, cfg->h2, cfg->nclass, 0x1234ABCDu);
    if(!tr->cn||!tr->m){ trainer_destroy(tr); return NULL; }

    /* z-norm */
    tr->zmean=malloc((size_t)tr->D*sizeof(float));
    tr->zstd=malloc((size_t)tr->D*sizeof(float));
    if(cfg->do_norm) {
        double *sum=calloc((size_t)tr->D,sizeof(double)), *sum2=calloc((size_t)tr->D,sizeof(double));
        long nz=tr->ntr<500?tr->ntr:500;
        int ps = tr->in_sz;
        float *feat=malloc(sizeof(float)*(size_t)tr->D);
        for(long i=0;i<nz;i++) {
            const unsigned char *raw=tr->tr_img+i*784;
            float im[784], imp[1024];
            for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
            pad28(im,imp,ps);
            convnet3_forward(tr->cn,imp,feat);
            for(int d=0;d<tr->D;d++){ sum[d]+=feat[d]; sum2[d]+=(double)feat[d]*feat[d]; }
        }
        free(feat);
        for(int d=0;d<tr->D;d++){ double mu=sum[d]/nz, va=sum2[d]/nz-mu*mu; float sd=sqrtf((float)va); if(sd<1e-2f)sd=1; tr->zmean[d]=(float)mu; tr->zstd[d]=sd; }
        free(sum); free(sum2);
    } else { for(int d=0;d<tr->D;d++){ tr->zmean[d]=0; tr->zstd[d]=1; } }

    /* Thread replicas */
    int nprocs=(int)sysconf(_SC_NPROCESSORS_ONLN); if(nprocs<1)nprocs=1;
    int nth=cfg->nthreads?cfg->nthreads:nprocs; if(nth>MAX_THREADS) nth=MAX_THREADS;
    if(nth<1) nth=1;
    fprintf(stderr,"[trainer] threads=%d epochs=%d batch=%ld lr=%.4f conv_fac=%.2f\n", nth, cfg->epochs, cfg->batch, cfg->lr, cfg->conv_fac);

    tr->cnT=calloc((size_t)nth,sizeof(ConvNet3*));
    tr->mT=calloc((size_t)nth,sizeof(MLP*));
    for(int t=0;t<nth;t++){ tr->cnT[t]=convnet3_gradbuf(tr->cn); tr->mT[t]=mlp_gradbuf(tr->m); }

    /* Optimizer state */
    for(int g=0;g<6;g++){
        MLPLayer L=mlp_layer(tr->m,g);
        tr->mvel[g]=calloc((size_t)L.n,sizeof(float));
        tr->mmsq[g]=calloc((size_t)L.n,sizeof(float));
    }
    int cnlayers=convnet3_layer_count(tr->cn);
    tr->cvel=calloc((size_t)cnlayers,sizeof(float*));
    tr->cmsq=calloc((size_t)cnlayers,sizeof(float*));
    for(int g=0;g<cnlayers;g++){
        ConvLayer3 L=convnet3_layer(tr->cn,g);
        tr->cvel[g]=calloc((size_t)L.n,sizeof(float));
        tr->cmsq[g]=calloc((size_t)L.n,sizeof(float));
    }

    /* Index shuffle */
    tr->idx=malloc((size_t)tr->ntr*sizeof(long));
    for(long i=0;i<tr->ntr;i++) tr->idx[i]=i;
    uint32_t rng=0x1234ABCDu;
    for(long i=tr->ntr-1;i>0;i--){ long j=(long)(rng=(rng*1103515245u+12345u))%(i+1); long t=tr->idx[i]; tr->idx[i]=tr->idx[j]; tr->idx[j]=t; }

    /* Schedule */
    long nbatch=(tr->ntr+cfg->batch-1)/cfg->batch;
    int total_epochs=cfg->epochs+cfg->phase2;
    tr->total_steps=(long)total_epochs*nbatch;
    tr->warm_eff = (cfg->warmup>0) ? (cfg->warmup < tr->total_steps/10 ? cfg->warmup : (int)(tr->total_steps/10)) : 0;
    tr->tstep=0;
    return tr;
}

static float lr_schedule(Trainer *tr, int ep) {
    float lr=tr->cfg.lr;
    if(tr->cfg.cos){
        if(tr->tstep < tr->warm_eff && tr->warm_eff>0)
            lr = tr->cfg.lr * (0.1f + 0.9f * ((float)tr->tstep / (float)tr->warm_eff));
        else {
            float frac = (float)tr->tstep / (float)tr->total_steps;
            if(frac<0) frac=0;
            if(frac>1) frac=1;
            lr = tr->cfg.lr * (0.1f + 0.9f * 0.5f * (1.0f + cosf((float)M_PI * frac)));
        }
    }
    return lr;
}

int trainer_epoch(Trainer *tr, TrainState *out) {
    long batch=tr->cfg.batch;
    long nbatch=(tr->ntr+batch-1)/batch;
    int nth = tr->cfg.nthreads ? tr->cfg.nthreads : (int)sysconf(_SC_NPROCESSORS_ONLN);
    if(nth>MAX_THREADS) nth=MAX_THREADS;
    if(nth<1) nth=1;
    int freeze_conv = tr->cfg.freeze_conv;
    int freeze_mlp = tr->cfg.freeze_mlp;

    for(long b=0;b<nbatch;b++) {
        tr->tstep++;
        float lr = lr_schedule(tr, 0);

        convnet3_zero_grad(tr->cn); mlp_zero_grad(tr->m);
        long base=b*batch, remain=tr->ntr-base; if(remain>batch)remain=batch;
        long per=remain/nth, extra=remain%nth, pos=base;
        pthread_t th[MAX_THREADS]; TArg ta[MAX_THREADS];
        for(int t=0;t<nth;t++) {
            long c=per+(t<extra?1:0);
            ta[t]=(TArg){.tid=t,.cn=tr->cnT[t],.m=tr->mT[t],.img=tr->tr_img,.lab=tr->tr_lab,
                .label_off=tr->cfg.label_off,.D=tr->D,.do_norm=tr->cfg.do_norm,.nclass=tr->cfg.nclass,
                .pad_sz=tr->in_sz,
                .zmean=tr->zmean,.zstd=tr->zstd,.idx=tr->idx,.start=pos,.count=c,
                .rng=0x1234ABCDu+1u+(uint32_t)t*2654435761u,
                .aug_deg=tr->cfg.aug_deg,.jit_px=tr->cfg.jit_px,.smooth=tr->cfg.smooth};
            pos+=c; pthread_create(&th[t],NULL,worker,&ta[t]);
        }
        long cnt=0; for(int t=0;t<nth;t++){ pthread_join(th[t],NULL); cnt+=ta[t].valid; }
        if(cnt==0) continue;
        for(int t=0;t<nth;t++){ convnet3_add_grad(tr->cn,tr->cnT[t]); mlp_add_grad(tr->m,tr->mT[t]); }
        mlp_scale_grad(tr->m,1.0f/(float)cnt);
        convnet3_scale_grad(tr->cn,1.0f/(float)cnt);
        { ConvLayer3 _L=convnet3_layer(tr->cn,0); double gn=0; for(int i=0;i<_L.n;i++) gn+=(double)_L.grad[i]*_L.grad[i];
          if((tr->tstep&255)==0) fprintf(stderr,"[DBG] tstep=%ld conv_gw1_L2=%.4f lr=%.4f\n", tr->tstep, sqrtf((float)gn), lr); }

        /* Optimizer step */
        if(!freeze_mlp){
            if(tr->cfg.opt==1) opt_adam_step(tr->m, lr, &tr->tstep, tr->mvel, tr->mmsq);
            else opt_sgd_step(tr->m, lr, tr->cfg.mom, tr->cfg.clip, tr->mvel);
        }
        if(!freeze_conv){
            float clr = lr * tr->cfg.conv_fac;
            if(tr->cfg.opt==1) opt_conv_adam(tr->cn, clr, &tr->tstep, tr->cvel, tr->cmsq);
            else opt_conv_sgd(tr->cn, clr, tr->cfg.mom, tr->cfg.clip, tr->cvel);
        }
    }

    /* Quick train accuracy -- use SHUFFLED indices (tr->idx) same as training batches */
    long chk=tr->ntr<1000?tr->ntr:1000, cor=0;
    int ps = tr->in_sz;
    for(long i=0;i<chk;i++) {
        long n = tr->idx[i];  /* use shuffled index! */
        const unsigned char *raw=tr->tr_img+n*784;
        float im[784], imp[1024];
        for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
        pad28(im,imp,ps);
        float *feat=malloc(sizeof(float)*(size_t)tr->D); convnet3_forward(tr->cn,imp,feat);
        if(tr->cfg.do_norm) for(int d=0;d<tr->D;d++) feat[d]=(feat[d]-tr->zmean[d])/tr->zstd[d];
        float sc[26]; mlp_forward(tr->m,feat,sc);
        int best=0; for(int c=1;c<tr->cfg.nclass;c++) if(sc[c]>sc[best]) best=c;
        if(best==tr->tr_lab[n]-tr->cfg.label_off) cor++;
    }
    if(out){
        out->epoch++; out->step=tr->tstep; out->lr=lr_schedule(tr,0);
        out->train_acc=100.0f*(float)cor/(float)chk;
    }
    return 0;
}

float trainer_run(Trainer *tr) {
    int total_epochs = tr->cfg.epochs + tr->cfg.phase2;
    TrainState st={0};
    for(int ep=0; ep<total_epochs; ep++) {
        if(ep==tr->cfg.epochs && tr->cfg.phase2>0){
            tr->cfg.freeze_conv=1; tr->cfg.freeze_mlp=0;
            fprintf(stderr,"  -- phase2: conv frozen, MLP-only for %d epochs --\n", tr->cfg.phase2);
        }
        trainer_epoch(tr,&st);
        printf("  epoch %2d: train_acc=%.2f%%  lr=%.4f\n", ep+1, st.train_acc, st.lr);
    }
    /* Test accuracy */
    long correct=0;
    int ps = tr->in_sz;
    float *feat=malloc(sizeof(float)*(size_t)tr->D);
    for(long i=0;i<tr->nte;i++) {
        const unsigned char *raw=tr->te_img+i*784;
        float im[784], imp[1024];
        for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
        pad28(im,imp,ps);
        convnet3_forward(tr->cn,imp,feat);
        if(tr->cfg.do_norm) for(int d=0;d<tr->D;d++) feat[d]=(feat[d]-tr->zmean[d])/tr->zstd[d];
        float sc[26]; mlp_forward(tr->m,feat,sc);
        int best=0; for(int c=1;c<tr->cfg.nclass;c++) if(sc[c]>sc[best]) best=c;
        if(best==tr->te_lab[i]-tr->cfg.label_off) correct++;
    }
    free(feat);
    float acc=100.0f*(float)correct/(float)tr->nte;
    st.test_acc=acc;

    /* DEBUG: report weight L2 norms to confirm weights actually move. */
    {
        double nw1=0; ConvLayer3 L0=convnet3_layer(tr->cn,0);
        for(int i=0;i<L0.n;i++) nw1+=(double)L0.param[i]*(double)L0.param[i];
        double nW1=0; MLPLayer M0=mlp_layer(tr->m,0);
        for(int i=0;i<M0.n;i++) nW1+=(double)M0.param[i]*(double)M0.param[i];
        printf("  [dbg] conv w1 L2=%.3f  mlp W1 L2=%.3f  tstep=%ld\n",
               sqrt(nw1), sqrt(nW1), tr->tstep);
    }

    /* HELDOUT...
     * for training (indices [ntr, ntr+1000) into the full buffer). Same pixel
     * distribution as training data but unseen -> measures real generalization,
     * not memorization of the trained head. If this ~= test ~= random while the
     * trained-head "train_acc" is high, the net memorizes instead of learning. */
    long hstart = tr->ntr;                 /* capped training size */
    long hend   = hstart + 1000;
    if(hend > tr->ntr_full) hend = tr->ntr_full;
    long hcor=0, htot=0;
    float *hfeat=malloc(sizeof(float)*(size_t)tr->D);
    for(long i=hstart; i<hend; i++){
        const unsigned char *raw=tr->tr_img+i*784;
        float im[784], imp[1024];
        for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
        pad28(im,imp,ps);
        convnet3_forward(tr->cn,imp,hfeat);
        if(tr->cfg.do_norm) for(int d=0;d<tr->D;d++) hfeat[d]=(hfeat[d]-tr->zmean[d])/tr->zstd[d];
        float sc[26]; mlp_forward(tr->m,hfeat,sc);
        int best=0; for(int c=1;c<tr->cfg.nclass;c++) if(sc[c]>sc[best]) best=c;
        if(best==tr->tr_lab[i]-tr->cfg.label_off) hcor++;
        htot++;
    }
    free(hfeat);
    if(htot>0) printf("  heldout-train acc=%.2f%% (%ld/%ld)\n",
                      100.0f*(float)hcor/(float)htot, hcor, htot);
    return acc;
}

void trainer_destroy(Trainer *tr) {
    if(!tr) return;
    int nth = tr->cfg.nthreads ? tr->cfg.nthreads : (int)sysconf(_SC_NPROCESSORS_ONLN);
    if(nth>MAX_THREADS) nth=MAX_THREADS;
    if(nth<1) nth=1;
    for(int t=0;t<nth;t++){ if(tr->cnT) convnet3_destroy(tr->cnT[t]); if(tr->mT) mlp_destroy(tr->mT[t]); }
    free(tr->cnT); free(tr->mT);
    if(tr->cn) convnet3_destroy(tr->cn);
    if(tr->m) mlp_destroy(tr->m);
    free(tr->zmean); free(tr->zstd);
    free(tr->idx);
    free(tr->tr_img); free(tr->te_img); free(tr->tr_lab); free(tr->te_lab);
    for(int g=0;g<6;g++){ free(tr->mvel[g]); free(tr->mmsq[g]); }
    int cnlayers = tr->cn ? convnet3_layer_count(tr->cn) : 0;
    for(int g=0;g<cnlayers;g++){ free(tr->cvel[g]); free(tr->cmsq[g]); }
    free(tr->cvel); free(tr->cmsq);
    free(tr);
}

int trainer_save(const Trainer *tr, const char *conv_path, const char *mlp_path) {
    if(conv_path && convnet3_save(tr->cn, conv_path)!=0) return -1;
    if(mlp_path && mlp_save(tr->m, tr->zmean, tr->zstd, tr->D, mlp_path)!=0) return -1;
    return 0;
}

float trainer_eval(const Trainer *tr) {
    long correct=0;
    int ps = tr->in_sz;
    float *feat=malloc(sizeof(float)*(size_t)tr->D);
    for(long i=0;i<tr->nte;i++) {
        const unsigned char *raw=tr->te_img+i*784;
        float im[784], imp[1024];
        for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
        pad28(im,imp,ps);
        convnet3_forward(tr->cn,imp,feat);
        if(tr->cfg.do_norm) for(int d=0;d<tr->D;d++) feat[d]=(feat[d]-tr->zmean[d])/tr->zstd[d];
        float sc[26]; mlp_forward(tr->m,feat,sc);
        int best=0; for(int c=1;c<tr->cfg.nclass;c++) if(sc[c]>sc[best]) best=c;
        if(best==tr->te_lab[i]-tr->cfg.label_off) correct++;
    }
    free(feat);
    float acc=100.0f*(float)correct/(float)tr->nte;
    return acc;
}