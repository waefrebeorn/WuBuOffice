/* emnist_train_conv3.c -- end-to-end training of conv3 (3-stage MED
 * front-end) + MLP on any 28x28 IDX dataset (EMNIST letters,
 * Fashion-MNIST, KMNIST, ...). Plain C11, no deps. Single core.
 *
 * Usage: ./emnist_train_conv3 <data_dir> [h1 h2 epochs cap]
 *   data_dir : dir holding <STEM>-train-images-idx3-ubyte etc.
 * Env (all optional):
 *   CN_TRAIN / CN_TEST : file stems (default emnist/emnist-letters-train|test)
 *   CN_CLASS  : #classes            (default 26)
 *   CN_LABOFF: label value offset  (default 1; MNIST/Fashion/KMNIST=0)
 *   CN_OPT    : sgd | adam       (default sgd)
 *   CN_LR     : learning rate     (default 0.05 sgd / 0.002 adam)
 *   CN_MOM    : momentum         (default 0.9)
 *   CN_CLIP   : grad clip (L2)   (default 5.0)
 *   CN_BATCH  : batch size       (default 256)
 *   CN_CONVF  : conv LR factor  (default 1.0 -- conv3 trains fine at full LR)
 *   CN_AUG    : rotation aug deg (default 0 = off)
 *   CN_NORM   : standardize conv feat (default off)
 *   CN_EPOCHS: epochs           (default 20)
 *   CN_DIAG   : print conv-alive%% each epoch
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#include <pthread.h>
#include <unistd.h>
#include "convnet3.h"
#include "mlp.h"

#ifndef MAXFEAT
#define MAXFEAT (64*4 + 64)
#endif
/* thread-private deterministic RNG (xorshift) */
static uint32_t thr_rng(uint32_t *s){ *s^=*s<<13; *s^=*s>>17; *s^=*s<<5; return *s; }

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
    size_t g=fread(b,1,(size_t)n*pic,f); *c=(long)(g/pic); *d=b; fclose(f);
    return 0;
}

/* xorshift RNG (training shuffle + rotation aug) */
static uint32_t s_rng = 0x1234ABCDu;
static long rnd_long(long mod){ s_rng^=s_rng<<13; s_rng^=s_rng>>17; s_rng^=s_rng<<5; return (long)(s_rng%(unsigned long)mod); }

/* in-place rotate a 28x28 glyph by deg (cheap bilinear aug) */
static void rotate28(float *img, float *tmp, float deg){
    float a=deg*(float)M_PI/180.0f, ca=cosf(a), sa=sinf(a), c=13.5f;
    for(int y=0;y<28;y++) for(int x=0;x<28;x++){
        float dx=(float)x-c, dy=(float)y-c;
        float sx=dx*ca-dy*sa+c, sy=dx*sa+dy*ca+c;
        int x0=(int)floorf(sx), y0=(int)floorf(sy);
        float fx=sx-x0, fy=sy-y0; float v=0;
        for(int oy=0;oy<=1;oy++) for(int ox=0;ox<=1;ox++){
            int xx=x0+ox, yy=y0+oy; if(xx<0||xx>27||yy<0||yy>27) continue;
            float w=(ox?fx:1-fx)*(oy?fy:1-fy); v+=w*img[yy*28+xx];
        }
        tmp[y*28+x]=v;
    }
    memcpy(img,tmp,28*28*sizeof(float));
}

/* thread-private model replica + slice description for one worker */
typedef struct {
    int tid; ConvNet3 *cn; MLP *m;
    const unsigned char *img, *lab;
    int label_off, D, do_norm, nclass;
    const float *zmean, *zstd; float aug_deg;
    const long *idx; long start, count; long valid; uint32_t rng;
} TArg;

/* worker: forward+backward one slice into the thread-private replica.
 * Shared cn/m are READ-ONLY here; only this replica's grad buffers move. */
#define TW(i) ((float)(255-(i))/255.0f)
static void* worker(void* p){
    TArg *a=(TArg*)p; long valid=0;
    convnet3_zero_grad(a->cn); mlp_zero_grad(a->m);
    for(long j=0;j<a->count;j++){
        long n=a->idx[a->start+j]; int lab=a->lab[n]-a->label_off;
        if(lab<0||lab>=a->nclass) continue;
        const unsigned char *raw=a->img+(size_t)n*784;
        float im[784], imt[784];
        for(int q=0;q<784;q++) im[q]=TW(raw[q]);
        if(a->aug_deg>0){ float deg=((float)(thr_rng(&a->rng)%1000)/1000.0f*2.0f-1.0f)*a->aug_deg; rotate28(im,imt,deg); }
        float feat[1024]; convnet3_forward(a->cn,im,feat);
        if(a->do_norm) for(int d=0;d<a->D;d++) feat[d]=(feat[d]-a->zmean[d])/a->zstd[d];
        float sc[26]; mlp_forward(a->m,feat,sc); mlp_backward(a->m,feat,lab);
        float df[1024]; mlp_input_grad(a->m,feat,df);
        convnet3_backward(a->cn,im,feat,df);
        valid++;
    }
    a->valid=valid; return NULL;
}
#undef TW

int main(int argc, char **argv) {
    const char *dir = (argc>1)? argv[1] : "data";
    int h1 = getenv("CN_H1")? atoi(getenv("CN_H1")) : (argc>2? atoi(argv[2]):128);
    int h2 = getenv("CN_H2")? atoi(getenv("CN_H2")) : (argc>3? atoi(argv[3]):64);
    int epochs = getenv("CN_EPOCHS")? atoi(getenv("CN_EPOCHS")) : (argc>4? atoi(argv[4]):20);
    long traincap = (argc>5)? atol(argv[5]) : 0;
    (void)(argc>6 ? atof(argv[6]) : 0.0);
    (void)(argc>7 ? atof(argv[7]) : 0.0);

    float base_lr = getenv("CN_LR")? (float)atof(getenv("CN_LR")) : 0.05f;
    float mom     = getenv("CN_MOM")? (float)atof(getenv("CN_MOM")) : 0.9f;
    float clip_n  = getenv("CN_CLIP")? (float)atof(getenv("CN_CLIP")) : 1e9f;  /* default OFF (clipping at 5.0 silently kills the MLP) */
    long batch    = getenv("CN_BATCH")? atol(getenv("CN_BATCH")) : 256;
    float aug_deg  = getenv("CN_AUG") ? (float)atof(getenv("CN_AUG")) : 0.0f;
    int label_off = getenv("CN_LABOFF")? atoi(getenv("CN_LABOFF")) : 1;
    int do_norm   = getenv("CN_NORM")? 1 : 0;

    char ptr[512], pte[512], ptw[512], ptel[512];
    const char *trp = getenv("CN_TRAIN")? getenv("CN_TRAIN") : "emnist/emnist-letters-train";
    const char *tep = getenv("CN_TEST") ? getenv("CN_TEST")  : "emnist/emnist-letters-test";
    if (getenv("CN_PTR"))  snprintf(ptr, sizeof ptr, "%s", getenv("CN_PTR"));
    else snprintf(ptr, sizeof ptr, "%s/%s-images-idx3-ubyte", dir, trp);
    if (getenv("CN_PTE"))  snprintf(pte, sizeof pte, "%s", getenv("CN_PTE"));
    else snprintf(pte, sizeof pte, "%s/%s-images-idx3-ubyte", dir, tep);
    if (getenv("CN_PTW"))  snprintf(ptw, sizeof ptw, "%s", getenv("CN_PTW"));
    else snprintf(ptw, sizeof ptw, "%s/%s-labels-idx1-ubyte", dir, trp);
    if (getenv("CN_PTEL")) snprintf(ptel,sizeof ptel,"%s", getenv("CN_PTEL"));
    else snprintf(ptel,sizeof ptel,"%s/%s-labels-idx1-ubyte", dir, tep);

    unsigned char *tr_img=NULL,*te_img=NULL,*tr_lab=NULL,*te_lab=NULL;
    long ntr=0,nte=0,ntw=0,nte_l=0;
    if(load_idx(ptr,&tr_img,&ntr)) return 1;
    if(load_idx(pte,&te_img,&nte)) return 1;
    if(load_idx(ptw,&tr_lab,&ntw)) return 1;
    if(load_idx(ptel,&te_lab,&nte_l)) return 1;
    if(traincap && traincap<ntr) ntr=traincap;
    if(ntr>ntw) ntr=ntw;
    nte=nte_l;

    ConvNet3 *cn = convnet3_create(&CONV_MED);
    /* Optional: load a pretrained conv (e.g. to freeze good features and only
     * fit the MLP head, or to warm-start joint training). CN_LOAD_CONV=path */
    if(getenv("CN_LOAD_CONV")){
        ConvNet3 *loaded=NULL; ConvConfig3 cfg;
        if(convnet3_load(getenv("CN_LOAD_CONV"), &loaded, &cfg)==0 && loaded){
            convnet3_destroy(cn); cn=loaded;
            fprintf(stderr,"[init] loaded pretrained conv from %s\n", getenv("CN_LOAD_CONV"));
        } else {
            fprintf(stderr,"[init] WARN: CN_LOAD_CONV set but load failed (%s); using random init\n", getenv("CN_LOAD_CONV"));
        }
    }
    int D = convnet3_dim(cn);
    int nclass = getenv("CN_CLASS")? atoi(getenv("CN_CLASS")) : 26;
    MLP *m = mlp_create(D, h1, h2, nclass, 0x1234ABCDu);
    int use_adam = getenv("CN_OPT") ? strcmp(getenv("CN_OPT"), "sgd") != 0 : 0;   /* default SGD */
    int use_wubu = getenv("CN_OPT") ? strcmp(getenv("CN_OPT"), "wubu") == 0 : 0;
    float eff_lr = base_lr;
    if (use_adam && !getenv("CN_LR")) eff_lr = 0.002f;   /* Adam default */
    float conv_fac = getenv("CN_CONVF") ? (float)atof(getenv("CN_CONVF")) : 1.0f;
    int freeze_conv = getenv("CN_FREEZE_CONV") ? 1 : 0;   /* phase 1: conv only */
    int freeze_mlp  = getenv("CN_FREEZE_MLP")  ? 1 : 0;   /* phase 2: mlp only  */
    int phase2      = getenv("CN_PHASE2") ? atoi(getenv("CN_PHASE2")) : 0;  /* extra MLP-only epochs after main loop */
    long tstep=0;
    const float b1=0.9f,b2=0.999f,eps=1e-8f;
    printf("conv3+MLP: conv feats=%d -> MLP %d->%d->%d->%d; epochs=%d batch=%ld lr=%.4f %s norm=%d nclass=%d; train=%ld test=%ld\n",
           D,D,h1,h2,nclass,epochs,batch,eff_lr,use_adam?"adam":"sgd",do_norm,nclass,ntr,nte);

    float *zmean=malloc((size_t)D*sizeof(float));
    float *zstd =malloc((size_t)D*sizeof(float));
    if(do_norm){
        double *sum=calloc((size_t)D,sizeof(double)),*sum2=calloc((size_t)D,sizeof(double));
        for(long i=0;i<ntr;i++){ const unsigned char*raw=tr_img+i*784; float im[784]; for(int q=0;q<784;q++)im[q]=(float)(255-raw[q])/255.0f; float z[1024]; convnet3_forward(cn,im,z); for(int d=0;d<D;d++){sum[d]+=z[d];sum2[d]+=z[d]*z[d];} }
        for(int d=0;d<D;d++){ double mu=sum[d]/ntr, va=sum2[d]/ntr-mu*mu; float sd=sqrtf((float)va); if(sd<1e-2f)sd=1; zmean[d]=(float)mu; zstd[d]=sd; }
        free(sum); free(sum2);
    } else { for(int d=0;d<D;d++){zmean[d]=0;zstd[d]=1;} }

    /* optimizer buffers: momentum/Adam (mvel/mmsso) + WuBu
     * Riemannian diagonal natural-gradient (wvel/wcsq). */
    float *mvel[6]={0},*mmsso[6]={0};
    for(int g=0;g<6;g++){ mvel[g]=calloc((size_t)mlp_layer(m,g).n,sizeof(float)); mmsso[g]=calloc((size_t)mlp_layer(m,g).n,sizeof(float)); }
    float *cvel[6]={0},*cmsq[6]={0};
    for(int g=0;g<convnet3_layer_count(cn);g++){ cvel[g]=calloc((size_t)convnet3_layer(cn,g).n,sizeof(float)); cmsq[g]=calloc((size_t)convnet3_layer(cn,g).n,sizeof(float)); }
    float *wvel[6]={0},*wcsq[6]={0};
    for(int g=0;g<6;g++){ wvel[g]=calloc((size_t)mlp_layer(m,g).n,sizeof(float)); wcsq[g]=calloc((size_t)mlp_layer(m,g).n,sizeof(float)); }
    float *cwvel[6]={0},*cwcsq[6]={0};
    for(int g=0;g<convnet3_layer_count(cn);g++){ cwvel[g]=calloc((size_t)convnet3_layer(cn,g).n,sizeof(float)); cwcsq[g]=calloc((size_t)convnet3_layer(cn,g).n,sizeof(float)); }
    /* WuBu Riemannian diagonal preconditioner decay (inverse-curvature EMA). */
    float rho = getenv("CN_RHO") ? (float)atof(getenv("CN_RHO")) : 0.9f;

    /* ---- thread setup: detect cores, build per-thread model replicas ---- */
    int nprocs = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs < 1) nprocs = 1;
    int nth = getenv("CN_THREADS") ? atoi(getenv("CN_THREADS")) : nprocs;
    if (nth < 1) nth = 1;
    if (nth > 16) nth = 16;
    fprintf(stderr, "[threads] using %d worker thread(s) (detected %d cores)\n", nth, nprocs);

    /* thread-private grad replicas (alias shared weights; params read-only
     * during a batch, so no race). Allocated ONCE; grads reduced each batch. */
    ConvNet3 **cnT = calloc((size_t)nth, sizeof(ConvNet3*));
    MLP      **mT  = calloc((size_t)nth, sizeof(MLP*));
    for (int t=0; t<nth; t++){ cnT[t]=convnet3_gradbuf(cn); mT[t]=mlp_gradbuf(m); }

    long nbatch=(ntr+batch-1)/batch;
    int total_epochs = epochs + phase2;
    for(int ep=0;ep<total_epochs;ep++){
        if(ep==epochs && phase2>0){ freeze_conv=1; freeze_mlp=0;
            printf("  -- phase2: conv frozen, MLP-only for %d epochs --\n", phase2); }
        float lr = eff_lr; if(ep>(int)(epochs*0.8f)) lr*=0.5f;
        long *idx=malloc((size_t)ntr*sizeof(long));
        for(long i=0;i<ntr;i++) idx[i]=i;
        for(long i=ntr-1;i>0;i--){ long j=rnd_long(i+1); long t=idx[i]; idx[i]=idx[j]; idx[j]=t; }
        for(long b=0;b<nbatch;b++){
            convnet3_zero_grad(cn); mlp_zero_grad(m);
            long base=b*batch, remain=ntr-base; if(remain>batch)remain=batch;
            long per=remain/nth, extra=remain%nth, pos=base;
            pthread_t th[16]; TArg ta[16];
            for(int t=0;t<nth;t++){
                long c=per+(t<extra?1:0);
                ta[t]=(TArg){.tid=t,.cn=cnT[t],.m=mT[t],.img=tr_img,.lab=tr_lab,
                    .label_off=label_off,.D=D,.do_norm=do_norm,.nclass=nclass,
                    .zmean=zmean,.zstd=zstd,.aug_deg=aug_deg,.idx=idx,.start=pos,.count=c,
                    .rng=0x1234ABCDu+1u+(uint32_t)t*2654435761u};
                pos+=c; pthread_create(&th[t],NULL,worker,&ta[t]);
            }
            long cnt=0; for(int t=0;t<nth;t++){ pthread_join(th[t],NULL); cnt+=ta[t].valid; }
            if(cnt==0) continue;
            for(int t=0;t<nth;t++){ convnet3_add_grad(cn,cnT[t]); mlp_add_grad(m,mT[t]); }
            /* Standard minibatch SGD: MEAN gradient over the batch. Effective
             * step = lr*mean. NOTE the LR must be a MEAN-grad LR (~0.5-1.0 for
             * this MLP), NOT a per-sample LR (~0.005) — using 0.005 with the
             * mean made the step ~cnt too small = the 10% plateau all session.
             * The CONV needs a MUCH smaller LR than the MLP or it explodes:
             * set conv_fac ~0.01 (conv lr = lr*conv_fac). clip default OFF. */
            mlp_scale_grad(m,1.0f/(float)cnt);
            convnet3_scale_grad(cn,1.0f/(float)cnt);
            /* ---- optimizer update on SHARED model (single-threaded) ---- */
            if(!freeze_mlp){
            if(use_adam){
                tstep++; float corr=sqrtf(1.0f-powf(b2,(float)tstep))/(1.0f-powf(b1,(float)tstep));
                for(int g=0;g<6;g++){ MLPLayer L=mlp_layer(m,g); for(int i=0;i<L.n;i++){ float gv=L.grad[i]; mvel[g][i]=b1*mvel[g][i]+(1-b1)*gv; mmsso[g][i]=b2*mmsso[g][i]+(1-b2)*gv*gv; float uh=mvel[g][i]/(sqrtf(mmsso[g][i])+eps); L.param[i]-=lr*corr*uh; } }
            } else if(use_wubu){
                for(int g=0;g<6;g++){ MLPLayer L=mlp_layer(m,g); for(int i=0;i<L.n;i++){ float gv=L.grad[i]; wcsq[g][i]=rho*wcsq[g][i]+(1-rho)*gv*gv; float uh=gv/sqrtf(wcsq[g][i]+eps); L.param[i]-=lr*uh; } }
            } else {
                for(int g=0;g<6;g++){ MLPLayer L=mlp_layer(m,g); float gn=0; for(int i=0;i<L.n;i++)gn+=L.grad[i]*L.grad[i]; gn=sqrtf(gn); float sc=gn>clip_n?clip_n/gn:1.0f; for(int i=0;i<L.n;i++){ float gv=L.grad[i]*sc; mvel[g][i]=mom*mvel[g][i]+gv; L.param[i]-=lr*mvel[g][i]; } }
            }
            }
            if(!freeze_conv){
            if(use_adam){
                tstep++; float corr=sqrtf(1.0f-powf(b2,(float)tstep))/(1.0f-powf(b1,(float)tstep)); float clr=lr*conv_fac;
                for(int g=0;g<convnet3_layer_count(cn);g++){ ConvLayer3 L=convnet3_layer(cn,g); for(int i=0;i<L.n;i++){ float gv=L.grad[i]; cvel[g][i]=b1*cvel[g][i]+(1-b1)*gv; cmsq[g][i]=b2*cmsq[g][i]+(1-b2)*gv*gv; float uh=cvel[g][i]/(sqrtf(cmsq[g][i])+eps); L.param[i]-=clr*corr*uh; } }
            } else if(use_wubu){
                float clr=lr*conv_fac;
                for(int g=0;g<convnet3_layer_count(cn);g++){ ConvLayer3 L=convnet3_layer(cn,g); for(int i=0;i<L.n;i++){ float gv=L.grad[i]; cwcsq[g][i]=rho*cwcsq[g][i]+(1-rho)*gv*gv; float uh=gv/sqrtf(cwcsq[g][i]+eps); L.param[i]-=clr*uh; } }
            } else {
                float clr=lr*conv_fac;
                for(int g=0;g<convnet3_layer_count(cn);g++){ ConvLayer3 L=convnet3_layer(cn,g); float gn=0; for(int i=0;i<L.n;i++)gn+=L.grad[i]*L.grad[i]; gn=sqrtf(gn); float sc=gn>clip_n?clip_n/gn:1.0f; for(int i=0;i<L.n;i++){ float gv=L.grad[i]*sc; cvel[g][i]=mom*cvel[g][i]+gv; L.param[i]-=clr*cvel[g][i]; } }
            }
            }/*end freeze_conv*/
            if(getenv("CN_DBG") && ep==0){
                MLPLayer L0=mlp_layer(m,0); float g0=L0.grad[0], p0=L0.param[0], gn=0;
                for(int i=0;i<L0.n;i++) gn+=L0.grad[i]*L0.grad[i];
                fprintf(stderr,"[DBG] W1[0]=%.5f gW1[0]=%.5f gradL2=%.3f\n", p0,g0,sqrtf(gn));
            }
        }
        free(idx);

        if(getenv("CN_DIAG")){
            long dpv=0,tot=0; float zp[1024];
            for(long i=0;i<nte && i<500;i++){ const unsigned char*raw=te_img+i*784; float im[784]; for(int q=0;q<784;q++)im[q]=(float)(255-raw[q])/255.0f; convnet3_forward(cn,im,zp); for(int d=0;d<D;d++){tot++;if(zp[d]>0)dpv++;} }
            fprintf(stderr,"[CN_DIAG] ep%d conv alive %% = %.1f\n",ep+1,100.0f*(float)dpv/(float)tot);
        }

        long chk=ntr<4000?ntr:4000,cor=0; float sc[26];
        for(long i=0;i<chk;i++){ const unsigned char*raw=tr_img+i*784; float im[784]; for(int q=0;q<784;q++)im[q]=(float)(255-raw[q])/255.0f; float ft[1024]; convnet3_forward(cn,im,ft); if(do_norm)for(int d=0;d<D;d++)ft[d]=(ft[d]-zmean[d])/zstd[d]; mlp_forward(m,ft,sc); int best=0; for(int c=1;c<nclass;c++) if(sc[c]>sc[best])best=c; if(best==tr_lab[i]-label_off)cor++; }
        printf("  epoch %2d: train_acc~%.2f%%  lr=%.4f\n", ep+1, 100.0f*(float)cor/(float)chk, lr);
    }
    for(int t=0;t<nth;t++){ convnet3_destroy(cnT[t]); mlp_destroy(mT[t]); }
    free(cnT); free(mT);

    /* full test accuracy */
    long correct=0; float sc[26],z[1024];
    for(long i=0;i<nte;i++){ const unsigned char*raw=te_img+i*784; float im[784]; for(int q=0;q<784;q++)im[q]=(float)(255-raw[q])/255.0f; convnet3_forward(cn,im,z); if(do_norm)for(int d=0;d<D;d++)z[d]=(z[d]-zmean[d])/zstd[d]; mlp_forward(m,z,sc); int best=0; for(int c=1;c<nclass;c++) if(sc[c]>sc[best])best=c; if(best==te_lab[i]-label_off)correct++; }
    float acc=100.0f*(float)correct/(float)nte;
    printf("\n=== %s (ultra-light conv3+MLP, plain C11 %s) ===\n", getenv("CN_NAME")?getenv("CN_NAME"):"dataset",
           use_wubu?"wubu-natgrad":(use_adam?"adam":"sgd"));
    printf("OVERALL ACCURACY: %.2f%% (%ld/%ld)\n", acc, correct, nte);

    char cpath[1024]; snprintf(cpath,sizeof cpath,"%s/conv3.wts",dir);
    char mpath[1024]; snprintf(mpath,sizeof mpath,"%s/conv3_mlp.wts",dir);
    if(convnet3_save(cn,cpath)==0) printf("saved conv3 -> %s\n",cpath);
    if(mlp_save(m,zmean,zstd,D,mpath)==0) printf("saved mlp  -> %s\n",mpath);

    convnet3_destroy(cn); mlp_destroy(m);
    free(zmean); free(zstd); free(tr_img); free(te_img); free(tr_lab); free(te_lab);
    for(int g=0;g<6;g++){ free(mvel[g]); free(mmsso[g]); free(wvel[g]); free(wcsq[g]); }
    for(int g=0;g<convnet3_layer_count(cn);g++){ free(cvel[g]); free(cmsq[g]); free(cwvel[g]); free(cwcsq[g]); }
    return 0;
}
