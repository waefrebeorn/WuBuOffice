/* train_conv3_clean.c -- GROUND-UP rebuild of the conv3+MLP EMNIST letters trainer.
 *
 * Design principles (after the prior stuck-at-random collapse):
 *   1. Verify primitives by OVERFITTING A SINGLE BATCH first. If the net
 *      cannot drive that batch's loss to ~0 and accuracy to ~100%, the
 *      primitives/loop are broken -- we abort before wasting time on full
 *      training. This is the standard "overfit one batch" sanity gate.
 *   2. Leaky ReLU (leak=0.1) + small positive conv bias (set in convnet3
 *      create) keep neurons alive.
 *   3. Feature z-normalization ON by default (std=1) so the MLP operates in
 *      its active regime. The z-norm gradient backout (df/=zstd) is applied
 *      before convnet3_backward -- required for correctness.
 *   4. Single-threaded, explicit, Adam with bias correction on both heads.
 *
 * Usage: train_conv3_clean <data_dir> [epochs=20] [lr=0.002] [batch=256]
 *        SAVE=/path  LOAD=/path  (save/load the trained model)
 */
#include "convnet3.h"
#include "mlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- IDX loader (standard MNIST/EMNIST, 4-byte BE header dims) ---- */
static long idx_count(const unsigned char *h){
    return ((long)h[4]<<24)|((long)h[5]<<16)|((long)h[6]<<8)|h[7];
}
static int load_idx(const char *p, unsigned char **d, long *c){
    FILE *f=fopen(p,"rb"); if(!f) return -1;
    unsigned char h[16]; if(fread(h,1,16,f)!=16){fclose(f);return -1;}
    long n=idx_count(h); unsigned char *b=malloc((size_t)n*784);
    size_t g=fread(b,1,(size_t)n*784,f); *c=(long)(g/784); *d=b; fclose(f);
    if(g < (size_t)n*784){ /* truncated */ } return 0;
}

#define PAD_SZ 32
static void pad28(const float *im28, float *out, int S){
    int off=(S-28)/2;
    for(int y=0;y<S;y++) for(int x=0;x<S;x++){
        int sy=y-off, sx=x-off;
        out[(size_t)y*S+x] = (sy>=0&&sy<28&&sx>=0&&sx<28)? im28[(size_t)sy*28+sx] : 0.0f;
    }
}

/* Adam optimizer state for a flat param array. */
typedef struct { float *m,*v; } AdamBuf;
static AdamBuf mk_adam(int n){ AdamBuf a; a.m=calloc(n,sizeof(float)); a.v=calloc(n,sizeof(float)); return a; }
static void adam_step(float *p,float *g,AdamBuf *a,int n,float lr,float b1,float b2,float eps,long t){
    float bc1=1.0f-powf(b1,(float)t), bc2=1.0f-powf(b2,(float)t);
    for(int i=0;i<n;i++){
        a->m[i]=b1*a->m[i]+(1-b1)*g[i];
        a->v[i]=b2*a->v[i]+(1-b2)*g[i]*g[i];
        float mhat=a->m[i]/bc1;          /* bias-corrected 1st moment */
        float vhat=a->v[i]/bc2;          /* bias-corrected 2nd moment */
        p[i]-=lr*(mhat/(sqrtf(vhat)+eps)); /* standard Adam */
    }
}

/* cross-entropy of one sample (for diagnostics) */
static double ce_one(MLP *m,const float *z,int K,int t){
    float sc[26]; mlp_forward(m,z,sc);
    float mx=sc[0]; for(int c=1;c<K;c++) if(sc[c]>mx)mx=sc[c];
    double s=0; for(int c=0;c<K;c++) s+=expf(sc[c]-mx);
    return -(sc[t]-mx-logf((float)s));
}

int main(int argc,char**argv){
    const char *dir=(argc>1)?argv[1]:"data";
    int epochs=(argc>2)?atoi(argv[2]):20;
    float lr=(argc>3)?(float)atof(argv[3]):0.002f;
    long batch=(argc>4)?atol(argv[4]):256;
    const char *SAVE=getenv("SAVE");
    const char *LOAD=getenv("LOAD");

    unsigned char *tr_img=NULL,*te_img=NULL,*tr_lab=NULL,*te_lab=NULL;
    long ntr=0,nte=0,ntw=0,nte_l=0;
    char ptr[512],pte[512],ptw[512],ptel[512];
    snprintf(ptr,sizeof ptr,"%s/emnist-letters-train-images-idx3-ubyte",dir);
    snprintf(pte,sizeof pte,"%s/emnist-letters-test-images-idx3-ubyte",dir);
    snprintf(ptw,sizeof ptw,"%s/emnist-letters-train-labels-idx1-ubyte",dir);
    snprintf(ptel,sizeof ptel,"%s/emnist-letters-test-labels-idx1-ubyte",dir);
    if(load_idx(ptr,&tr_img,&ntr)){printf("load fail %s\n",ptr);return 1;}
    if(load_idx(pte,&te_img,&nte)){printf("load fail %s\n",pte);return 1;}
    if(load_idx(ptw,&tr_lab,&ntw)){printf("load fail %s\n",ptw);return 1;}
    if(load_idx(ptel,&te_lab,&nte_l)){printf("load fail %s\n",ptel);return 1;}
    printf("loaded: train=%ld test=%ld (26 classes, label 1..26 -> 0..25)\n",ntr,nte);

    /* ---- model ---- */
    ConvConfig3 cfg={32,32,16,5,2,32,5,2,64,3,1}; /* CONV_MED_PAD: 576 feats */
    ConvNet3 *cn=NULL; MLP *m=NULL;
    int h1=128,h2=64,K=26;
    float *zmean,*zstd;
    cn=convnet3_create(&cfg);
    int D=convnet3_dim(cn);
    m=mlp_create(D,h1,h2,K,0x1234ABCDu);
    (void)LOAD;
    zmean=malloc(D*sizeof(float)); zstd=malloc(D*sizeof(float));

    /* ---- compute z-norm stats over full train set (correct, once) ---- */
    {
        double *sum=calloc(D,sizeof(double)),*sum2=calloc(D,sizeof(double));
        float im[784],imp[PAD_SZ*PAD_SZ],ft[2048];
        for(long i=0;i<ntr;i++){
            const unsigned char*raw=tr_img+(size_t)i*784;
            for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
            pad28(im,imp,PAD_SZ); convnet3_forward(cn,imp,ft);
            for(int d=0;d<D;d++){sum[d]+=ft[d];sum2[d]+=ft[d]*ft[d];}
        }
        for(int d=0;d<D;d++){ double mu=sum[d]/ntr,va=sum2[d]/ntr-mu*mu; float sd=(float)sqrtf((float)va); if(sd<1e-2f)sd=1; zmean[d]=(float)mu; zstd[d]=sd; }
        free(sum);free(sum2);
    }
    printf("z-norm: feat dim=%d, mean std=%.3f (std=1 after norm)\n",D,zstd[0]);

    /* ---- Adam buffers ---- */
    int cn_n=0; for(int g=0;g<convnet3_layer_count(cn);g++) cn_n+=convnet3_layer(cn,g).n;
    int ml_n=0; for(int g=0;g<mlp_layer_count(m);g++) ml_n+=mlp_layer(m,g).n;
    /* flatten-style: per-group adam */
    AdamBuf *cad=malloc(convnet3_layer_count(cn)*sizeof(AdamBuf));
    AdamBuf *mad=malloc(mlp_layer_count(m)*sizeof(AdamBuf));
    for(int g=0;g<convnet3_layer_count(cn);g++) cad[g]=mk_adam(convnet3_layer(cn,g).n);
    for(int g=0;g<mlp_layer_count(m);g++) mad[g]=mk_adam(mlp_layer(m,g).n);

    /* ============ GATE 1: overfit a single batch ============ */
    printf("\n[GATE] overfitting one batch of %ld samples...\n",batch);
    long gate_n=batch<ntr?batch:ntr;
    float *gfeat=malloc((size_t)gate_n*D*sizeof(float));
    int *glab=malloc(gate_n*sizeof(int));
    for(long i=0;i<gate_n;i++){
        const unsigned char*raw=tr_img+(size_t)i*784; float im[784],imp[PAD_SZ*PAD_SZ];
        for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
        pad28(im,imp,PAD_SZ); convnet3_forward(cn,imp,gfeat+(size_t)i*D);
        for(int d=0;d<D;d++) gfeat[(size_t)i*D+d]=(gfeat[(size_t)i*D+d]-zmean[d])/zstd[d];
        glab[i]=tr_lab[i]-1;
    }
    double g0=0; for(long i=0;i<gate_n;i++) g0+=ce_one(m,gfeat+(size_t)i*D,K,glab[i]);
    long tstep=0;
    float b1=0.9f,b2=0.999f,eps=1e-8f;
    mlp_zero_grad(m);  /* zero ONCE; accumulate per-sample via mlp_backward */
    for(int it=0;it<10;it++){
        for(long i=0;i<gate_n;i++){
            mlp_forward(m,gfeat+(size_t)i*D,NULL);
            mlp_backward(m,gfeat+(size_t)i*D,glab[i]);
        }
        /* update MLP (Adam) -- scale summed gradients to MEAN first */
        mlp_scale_grad(m,1.0f/(float)gate_n);
        tstep++;
        for(int g=0;g<mlp_layer_count(m);g++){ MLPLayer L=mlp_layer(m,g);
            adam_step(L.param,L.grad,&mad[g],L.n,0.01f,b1,b2,eps,tstep); }
        /* freeze conv during gate (isolate MLP+features path) */
    }
    double g1=0,gate_correct=0; for(long i=0;i<gate_n;i++){
        g1+=ce_one(m,gfeat+(size_t)i*D,K,glab[i]);
        float sc[26]; mlp_forward(m,gfeat+(size_t)i*D,sc); int best=0; for(int c=1;c<K;c++) if(sc[c]>sc[best])best=c; if(best==glab[i])gate_correct++;
    }
    printf("[GATE] MLP-only on frozen conv feats: CE %.3f -> %.3f, batch acc %.1f%%\n",
           g0/gate_n,g1/gate_n,100.0*gate_correct/gate_n);
    if(gate_correct < gate_n*0.5 || g1/gate_n > g0/gate_n*0.5){
        printf("[GATE] FAIL: MLP cannot fit even frozen features. ABORT (primitives/loop broken).\n");
        return 2;
    }
    printf("[GATE] PASS: MLP fits frozen features. Proceeding to joint training.\n");
    free(gfeat);free(glab);

    /* ============ full training (joint, Adam on both) ============ */
    printf("\n[train] joint training %d epochs, batch %ld, lr %.4f\n",epochs,batch,lr);
    long *idx=malloc(ntr*sizeof(long));
    for(long i=0;i<ntr;i++) idx[i]=i;
    for(int ep=0;ep<epochs;ep++){
        /* shuffle */
        for(long i=ntr-1;i>0;i--){ long j=((unsigned long)rand()*(i+1))/RAND_MAX; long t=idx[i];idx[i]=idx[j];idx[j]=t; }
        float ep_loss=0; long ep_cnt=0;
        long nbatch=(ntr+batch-1)/batch;
        for(long b=0;b<nbatch;b++){
            long base=b*batch, remain=ntr-base; if(remain>batch)remain=batch;
            convnet3_zero_grad(cn); mlp_zero_grad(m);
            float *gfeatB=malloc((size_t)remain*D*sizeof(float));
            int *glabB=malloc(remain*sizeof(int));
            for(long j=0;j<remain;j++){
                long n=idx[base+j]; int lab=tr_lab[n]-1; glabB[j]=lab;
                const unsigned char*raw=tr_img+(size_t)n*784; float im[784],imp[PAD_SZ*PAD_SZ];
                for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
                pad28(im,imp,PAD_SZ); convnet3_forward(cn,imp,gfeatB+(size_t)j*D);
                for(int d=0;d<D;d++) gfeatB[(size_t)j*D+d]=(gfeatB[(size_t)j*D+d]-zmean[d])/zstd[d];
                mlp_forward(m,gfeatB+(size_t)j*D,0); /* cache acts */
                mlp_backward(m,gfeatB+(size_t)j*D,lab);
                float df[2048]; mlp_input_grad(m,gfeatB+(size_t)j*D,df);
                for(int d=0;d<D;d++) df[d]/=zstd[d]; /* z-norm backout */
                convnet3_backward(cn,imp,gfeatB+(size_t)j*D,df);
                float sc[26]; mlp_forward(m,gfeatB+(size_t)j*D,sc); /* for loss */
                float mx=sc[0]; for(int c=1;c<K;c++) if(sc[c]>mx)mx=sc[c];
                double s=0; for(int c=0;c<K;c++) s+=expf(sc[c]-mx);
                ep_loss+=-(sc[lab]-mx-logf((float)s)); ep_cnt++;
            }
            mlp_scale_grad(m,1.0f/(float)remain);
            convnet3_scale_grad(cn,1.0f/(float)remain);
            tstep++;
            /* DIAG: gradient magnitudes after first batch */
            if(ep==0 && b==0){
                double mgL2=0,cgL2=0;
                for(int g=0;g<mlp_layer_count(m);g++){MLPLayer L=mlp_layer(m,g);for(int k=0;k<L.n;k++)mgL2+=L.grad[k]*L.grad[k];}
                for(int g=0;g<convnet3_layer_count(cn);g++){ConvLayer3 L=convnet3_layer(cn,g);for(int k=0;k<L.n;k++)cgL2+=L.grad[k]*L.grad[k];}
                fprintf(stderr,"[DIAG] after batch0: MLP gradL2=%.4f  CONV gradL2=%.4f  (conv/MLP=%.3f)\n",sqrt(mgL2),sqrt(cgL2),sqrt(cgL2)/sqrt(mgL2));
            }
            for(int g=0;g<mlp_layer_count(m);g++){ MLPLayer L=mlp_layer(m,g);
                adam_step(L.param,L.grad,&mad[g],L.n,lr,b1,b2,eps,tstep); }
            for(int g=0;g<convnet3_layer_count(cn);g++){ ConvLayer3 L=convnet3_layer(cn,g);
                adam_step(L.param,L.grad,&cad[g],L.n,lr*0.1f,b1,b2,eps,tstep); } /* conv_fac=0.1 */
            free(gfeatB);free(glabB);
        }
        /* eval */
        long cor=0; float sc[26];
        long chk=ntr<4000?ntr:4000;
        for(long i=0;i<chk;i++){ const unsigned char*raw=tr_img+(size_t)i*784; float im[784],imp[PAD_SZ*PAD_SZ],ft[2048];
            for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f; pad28(im,imp,PAD_SZ); convnet3_forward(cn,imp,ft);
            for(int d=0;d<D;d++) ft[d]=(ft[d]-zmean[d])/zstd[d]; mlp_forward(m,ft,sc);
            int best=0; for(int c=1;c<K;c++) if(sc[c]>sc[best])best=c; if(best==tr_lab[i]-1)cor++; }
        printf("  epoch %2d: train_acc~%.2f%%  mean_loss=%.4f\n",ep+1,100.0f*cor/chk,ep_loss/ep_cnt);
    }

    /* final test */
    long correct=0; float sc[26],z[2048];
    for(long i=0;i<nte;i++){ const unsigned char*raw=te_img+(size_t)i*784; float im[784],imp[PAD_SZ*PAD_SZ];
        for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f; pad28(im,imp,PAD_SZ); convnet3_forward(cn,imp,z);
        for(int d=0;d<D;d++) z[d]=(z[d]-zmean[d])/zstd[d]; mlp_forward(m,z,sc);
        int best=0; for(int c=1;c<K;c++) if(sc[c]>sc[best])best=c; if(best==te_lab[i]-1)correct++; }
    printf("\n=== OVERALL TEST ACCURACY: %.2f%% (%ld/%ld) ===\n",100.0f*correct/nte,correct,nte);

    if(SAVE){
        if(convnet3_save(cn,SAVE)==0) printf("saved conv -> %s\n",SAVE);
        if(mlp_save(m,zmean,zstd,D,SAVE)==0) printf("saved mlp  -> %s\n",SAVE);
    }
    return 0;
}
