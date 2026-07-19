/* train_mt.c -- MULTITHREADED (4-core) mini-batch data-parallel trainer for the
 * conv3+MLP stack. Same proven math as train_persample.c (per-sample forward/
 * backward, instance norm, conv grad clip, z-norm chain rule, weight decay) but
 * parallelized across CN_THREADS worker threads via the gradbuf/add_grad infra.
 *
 * Parallelization: each mini-batch of B samples is split across T threads. Each
 * thread owns a grad-buffer REPLICA (convnet3_gradbuf/mlp_gradbuf) that ALIASES
 * the shared read-only weights but has its OWN caches + grad buffers -- so the
 * per-sample instance-norm caches (xh/is/c*) never race. Threads accumulate
 * grads over their shard; the main thread reduces (add_grad), scales by 1/B
 * (mean grad), and applies ONE SGD update. Weights are read-only during a batch.
 *
 * LR note: mean-grad mini-batch needs a LARGER LR than per-sample SGD. Rough
 * linear scaling: CN_LR_batch ~= B * CN_LR_persample. For B=32 and the proven
 * per-sample lr 0.02, use CN_LR ~ 0.3-0.6.
 *
 * Env (superset of train_persample.c):
 *   CN_TRAIN,CN_TEST,CN_CLASS,CN_LABOFF,CN_LR,CN_EPOCHS,CN_AUG,CN_NORM,CN_SUBSET
 *   CN_INORM,CN_WIDE,CN_K1/K2/K3,CN_H1/H2,CN_CONVF,CN_CCLIP,CN_WD
 *   CN_THREADS (default 4), CN_BATCH (default 32)
 * Usage: train_mt <datadir>
 */
#define _POSIX_C_SOURCE 199309L
#include "convnet3.h"
#include "mlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

static uint32_t rng=0x2468ACEu;
static uint32_t xr(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static float fr(void){ return (float)xr()/(float)0xFFFFFFFFu; }

static unsigned char* load_idx_img(const char*path,long*n,int*rows,int*cols){
    FILE*f=fopen(path,"rb"); if(!f){fprintf(stderr,"FATAL open %s\n",path);exit(1);}
    unsigned char h[16]; if(fread(h,1,16,f)!=16){exit(1);}
    long cnt=((long)h[4]<<24)|((long)h[5]<<16)|((long)h[6]<<8)|h[7];
    int r=((int)h[8]<<24)|((int)h[9]<<16)|((int)h[10]<<8)|h[11];
    int c=((int)h[12]<<24)|((int)h[13]<<16)|((int)h[14]<<8)|h[15];
    *n=cnt;*rows=r;*cols=c;
    unsigned char*d=malloc((size_t)cnt*r*c); if(fread(d,1,(size_t)cnt*r*c,f)!=(size_t)cnt*r*c){exit(1);}
    fclose(f); return d;
}
static unsigned char* load_idx_lab(const char*path,long*n){
    FILE*f=fopen(path,"rb"); if(!f){fprintf(stderr,"FATAL open %s\n",path);exit(1);}
    unsigned char h[8]; if(fread(h,1,8,f)!=8){exit(1);}
    long cnt=((long)h[4]<<24)|((long)h[5]<<16)|((long)h[6]<<8)|h[7]; *n=cnt;
    unsigned char*d=malloc(cnt); if(fread(d,1,cnt,f)!=(size_t)cnt){exit(1);}
    fclose(f); return d;
}
static void rot(const float*in,float*out,float deg){
    float rad=deg*3.14159265f/180.0f, cs=cosf(rad), sn=sinf(rad); float cx=13.5f,cy=13.5f;
    for(int y=0;y<28;y++)for(int x=0;x<28;x++){
        float dx=x-cx,dy=y-cy; float sx=cs*dx+sn*dy+cx, sy=-sn*dx+cs*dy+cy;
        int x0=(int)floorf(sx),y0=(int)floorf(sy); float fx=sx-x0,fy=sy-y0; float v=0;
        for(int j=0;j<2;j++)for(int i=0;i<2;i++){int xx=x0+i,yy=y0+j; if(xx>=0&&xx<28&&yy>=0&&yy<28){float w=(i?fx:1-fx)*(j?fy:1-fy); v+=w*in[yy*28+xx];}}
        out[y*28+x]=v;
    }
}

/* ---- shared globals for workers (read-only during a batch) ---- */
static unsigned char *g_trI, *g_trL;
static int   g_D, g_nclass, g_laboff;
static float g_aug;
static const float *g_zmean, *g_zstd;
static const long  *g_batch;      /* sample indices for current batch */

/* per-thread worker state */
typedef struct {
    ConvNet3 *cn;   /* gradbuf replica (aliases weights) */
    MLP      *mlp;  /* gradbuf replica */
    int start, end; /* [start,end) into g_batch */
    long correct;   /* train hits in this shard */
    uint32_t seed;  /* thread-local RNG for aug */
} Worker;

static uint32_t txr(uint32_t*s){ uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }
static float    tfr(uint32_t*s){ return (float)txr(s)/(float)0xFFFFFFFFu; }

static void* worker_fn(void*arg){
    Worker*w=(Worker*)arg;
    int D=g_D;
    float im[784],rim[784],z[1024],feat[1024],df[1024],sc[64];
    convnet3_zero_grad(w->cn); mlp_zero_grad(w->mlp);
    w->correct=0;
    for(int bi=w->start; bi<w->end; bi++){
        long n=g_batch[bi]; const unsigned char*raw=g_trI+n*784;
        for(int q=0;q<784;q++) im[q]=(float)(255-raw[q])/255.0f;
        const float*src=im;
        if(g_aug>0){ float deg=(tfr(&w->seed)*2-1)*g_aug; rot(im,rim,deg); src=rim; }
        convnet3_forward(w->cn,src,feat);
        for(int d=0;d<D;d++) z[d]=(feat[d]-g_zmean[d])/g_zstd[d];
        mlp_forward(w->mlp,z,sc);
        int best=0; for(int k=1;k<g_nclass;k++) if(sc[k]>sc[best])best=k;
        int lab=g_trL[n]-g_laboff; if(best==lab) w->correct++;
        mlp_backward(w->mlp,z,lab);           /* accumulates into replica grads */
        mlp_input_grad(w->mlp,z,df);
        for(int d=0;d<D;d++) df[d]/=g_zstd[d];/* chain rule through z-norm */
        convnet3_backward(w->cn,src,feat,df); /* accumulates into replica grads */
    }
    return NULL;
}

int main(int argc,char**argv){
    const char*dir=argc>1?argv[1]:"data";
    const char*trs=getenv("CN_TRAIN")?getenv("CN_TRAIN"):"fashion/train";
    const char*tes=getenv("CN_TEST")?getenv("CN_TEST"):"fashion/t10k";
    int nclass=getenv("CN_CLASS")?atoi(getenv("CN_CLASS")):10;
    int laboff=getenv("CN_LABOFF")?atoi(getenv("CN_LABOFF")):0;
    float lr=getenv("CN_LR")?(float)atof(getenv("CN_LR")):0.4f;
    int epochs=getenv("CN_EPOCHS")?atoi(getenv("CN_EPOCHS")):35;
    float aug=getenv("CN_AUG")?(float)atof(getenv("CN_AUG")):0.0f;
    int donorm=getenv("CN_NORM")?atoi(getenv("CN_NORM")):1;
    long cap=getenv("CN_SUBSET")?atol(getenv("CN_SUBSET")):0;
    int nthreads=getenv("CN_THREADS")?atoi(getenv("CN_THREADS")):4;
    int batch=getenv("CN_BATCH")?atoi(getenv("CN_BATCH")):32;
    if(nthreads<1)nthreads=1; if(batch<nthreads)batch=nthreads;

    char p[512]; long ntr,nte; int r,c,rl,cl; long nl;
    snprintf(p,sizeof p,"%s/%s-images-idx3-ubyte",dir,trs); g_trI=load_idx_img(p,&ntr,&r,&c);
    snprintf(p,sizeof p,"%s/%s-labels-idx1-ubyte",dir,trs); g_trL=load_idx_lab(p,&nl);
    snprintf(p,sizeof p,"%s/%s-images-idx3-ubyte",dir,tes); unsigned char*teI=load_idx_img(p,&nte,&rl,&cl);
    snprintf(p,sizeof p,"%s/%s-labels-idx1-ubyte",dir,tes); unsigned char*teL=load_idx_lab(p,&nl);
    if(cap>0&&cap<ntr)ntr=cap;

    ConvConfig3 ccfg = CONV_MED;
    if(getenv("CN_WIDE") && atoi(getenv("CN_WIDE"))){ ccfg.K1=32; ccfg.K2=64; ccfg.K3=128; }
    if(getenv("CN_K1")) ccfg.K1=atoi(getenv("CN_K1"));
    if(getenv("CN_K2")) ccfg.K2=atoi(getenv("CN_K2"));
    if(getenv("CN_K3")) ccfg.K3=atoi(getenv("CN_K3"));
    ConvNet3*cn=convnet3_create(&ccfg); int D=convnet3_dim(cn);
    int h1=getenv("CN_H1")?atoi(getenv("CN_H1")):256;
    int h2=getenv("CN_H2")?atoi(getenv("CN_H2")):128;
    MLP*m=mlp_create(D,h1,h2,nclass,0x1234ABCDu);
    g_D=D; g_nclass=nclass; g_laboff=laboff; g_aug=aug;

    printf("MT conv3+MLP: train=%ld test=%ld lr=%.4f epochs=%d aug=%.0f norm=%d nclass=%d threads=%d batch=%d\n",
           ntr,nte,lr,epochs,aug,donorm,nclass,nthreads,batch);
    printf("conv cfg: K1=%d K2=%d K3=%d fdim=%d  (H1=%d H2=%d)\n",ccfg.K1,ccfg.K2,ccfg.K3,D,h1,h2);

    float *feat=malloc(D*sizeof(float));
    float *zmean=calloc(D,sizeof(float)),*zstd=calloc(D,sizeof(float));
    if(donorm){
        long ns=ntr<4000?ntr:4000; float*acc=calloc(D,sizeof(float)),*ac2=calloc(D,sizeof(float)); float im[784];
        for(long i=0;i<ns;i++){const unsigned char*raw=g_trI+i*784; for(int q=0;q<784;q++)im[q]=(float)(255-raw[q])/255.0f; convnet3_forward(cn,im,feat); for(int d=0;d<D;d++){acc[d]+=feat[d];ac2[d]+=feat[d]*feat[d];}}
        for(int d=0;d<D;d++){zmean[d]=acc[d]/ns; float v=ac2[d]/ns-zmean[d]*zmean[d]; zstd[d]=v>1e-4f?sqrtf(v):1.0f;}
        free(acc);free(ac2);
    } else { for(int d=0;d<D;d++){zmean[d]=0;zstd[d]=1;} }
    g_zmean=zmean; g_zstd=zstd;

    /* per-thread replicas (reused across all batches) */
    Worker*W=calloc(nthreads,sizeof(Worker));
    pthread_t*th=calloc(nthreads,sizeof(pthread_t));
    for(int t=0;t<nthreads;t++){
        W[t].cn=convnet3_gradbuf(cn);
        W[t].mlp=mlp_gradbuf(m);
        W[t].seed=0x9E3779B9u ^ (uint32_t)(t*2654435761u) ^ 1u;
    }

    long*idx=malloc(ntr*sizeof(long)); for(long i=0;i<ntr;i++)idx[i]=i;
    float cclip=getenv("CN_CCLIP")?(float)atof(getenv("CN_CCLIP")):1.0f;
    float cvf=getenv("CN_CONVF")?(float)atof(getenv("CN_CONVF")):1.0f;
    float wd=getenv("CN_WD")?(float)atof(getenv("CN_WD")):0.0f;
    float im[784],z[1024]; (void)im; (void)z;
    int convlc=convnet3_layer_count(cn);

    struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
    for(int ep=0;ep<epochs;ep++){
        for(long i=ntr-1;i>0;i--){long j=xr()%(i+1);long t=idx[i];idx[i]=idx[j];idx[j]=t;}
        float clr=lr; if(ep>=(int)(epochs*0.4f))clr=lr*0.3f; if(ep>=(int)(epochs*0.7f))clr=lr*0.1f; if(ep>=(int)(epochs*0.9f))clr=lr*0.03f;
        float wdf=1.0f-clr*wd;
        long cor=0;
        for(long b0=0;b0<ntr;b0+=batch){
            long b1=b0+batch; if(b1>ntr)b1=ntr; int bn=(int)(b1-b0);
            g_batch=idx+b0;
            /* partition bn samples across threads */
            int per=(bn+nthreads-1)/nthreads;
            int nt=0;
            for(int t=0;t<nthreads;t++){
                int s=t*per, e=s+per; if(s>bn)s=bn; if(e>bn)e=bn;
                W[t].start=s; W[t].end=e;
                if(s<e){ pthread_create(&th[t],NULL,worker_fn,&W[t]); nt=t+1; }
                else W[t].correct=0;
            }
            for(int t=0;t<nt;t++) if(W[t].start<W[t].end) pthread_join(th[t],NULL);
            /* reduce thread grads into main net + count hits */
            convnet3_zero_grad(cn); mlp_zero_grad(m);
            for(int t=0;t<nthreads;t++){
                if(W[t].start<W[t].end){ convnet3_add_grad(cn,W[t].cn); mlp_add_grad(m,W[t].mlp); }
                cor+=W[t].correct;
            }
            /* mean grad over batch, then SGD update (mlp: +weight decay; conv: +clip) */
            float inv=1.0f/(float)bn;
            for(int g=0;g<6;g++){MLPLayer L=mlp_layer(m,g);
                if(wd>0){ for(int k=0;k<L.n;k++) L.param[k]=wdf*L.param[k]-clr*inv*L.grad[k]; }
                else    { for(int k=0;k<L.n;k++) L.param[k]-=clr*inv*L.grad[k]; } }
            for(int g=0;g<convlc;g++){ConvLayer3 L=convnet3_layer(cn,g);
                float gn=0; for(int k=0;k<L.n;k++){float gg=L.grad[k]*inv; gn+=gg*gg;} gn=sqrtf(gn);
                float scl=(gn>cclip)?cclip/gn:1.0f;
                for(int k=0;k<L.n;k++) L.param[k]-=clr*cvf*scl*inv*L.grad[k]; }
        }
        /* test each epoch (subset 2000 for speed except last) -- parallel-friendly but kept simple */
        long tec=(ep==epochs-1)?nte:2000; long tc=0;
        float tim[784],tz[1024],tsc[64];
        for(long i=0;i<tec;i++){const unsigned char*raw=teI+i*784; for(int q=0;q<784;q++)tim[q]=(float)(255-raw[q])/255.0f; convnet3_forward(cn,tim,feat); for(int d=0;d<D;d++)tz[d]=(feat[d]-zmean[d])/zstd[d]; mlp_forward(m,tz,tsc); int best=0; for(int k=1;k<nclass;k++)if(tsc[k]>tsc[best])best=k; if(best==(teL[i]-laboff))tc++;}
        clock_gettime(CLOCK_MONOTONIC,&t1);
        double el=(t1.tv_sec-t0.tv_sec)+(t1.tv_nsec-t0.tv_nsec)/1e9;
        printf("ep%2d lr=%.4f train_acc=%.2f%% test_acc=%.2f%%  [%.1fs elapsed]\n",
               ep+1,clr,100.0*cor/ntr,100.0*tc/tec,el);
        fflush(stdout);
    }
    convnet3_save(cn,"data/conv3.wts");
    mlp_save(m,zmean,zstd,D,"data/conv3_mlp.wts");
    printf("saved conv3 -> data/conv3.wts, mlp -> data/conv3_mlp.wts\n");
    for(int t=0;t<nthreads;t++){ convnet3_destroy(W[t].cn); mlp_destroy(W[t].mlp); }
    return 0;
}
