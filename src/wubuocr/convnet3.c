/* convnet3.c -- 3-stage ultra-light conv front-end (medium capacity).
 * See convnet3.h. C11, no deps, single-core scalar.
 * ~670k MACs per 28x28 image at CONV_MED. Q6600-class.
 *
 * INSTANCE NORM (gated by env CN_INORM=1, read once at create):
 *   Per-channel, per-sample normalization of conv pre-activations over the
 *   spatial map, with learnable gamma/beta, applied BEFORE ReLU. This is a
 *   batch-free form of BatchNorm (identical at train and test — no running
 *   stats needed) that stabilizes conv gradients so the conv can train at a
 *   higher LR. When CN_INORM is unset, behavior is byte-identical to the
 *   proven ReLU-only path.
 */
#include "convnet3.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define IN_EPS 1e-5f

struct ConvNet3 {
    int inH, inW;
    int K1,S1,P1, K2,S2,P2, K3,S3,P3;
    float *w1,*b1, *w2,*b2, *w3,*b3;
    int c1H,c1W,p1H,p1W;
    int c2H,c2W,p2H,p2W;
    int c3H,c3W;            /* stage3 conv out (no final pool) */
    int fdim;              /* feature length */
    /* caches */
    float *c1,*p1, *c2,*p2, *c3;
    int   *am1,*am2;      /* maxpool argmax (stage3 has no pool) */
    /* grads */
    float *gw1,*gb1,*gw2,*gb2,*gw3,*gb3;
    /* scratch */
    float *dc1,*dp1, *dc2,*dp2, *dc3;
    int   shared_params;   /* 1 if weight pointers alias another net (grad-only replica) */
    /* --- instance norm --- */
    int    use_in;                       /* instance norm enabled */
    float *ga1,*be1,*ga2,*be2,*ga3,*be3; /* per-channel gamma/beta */
    float *dga1,*dbe1,*dga2,*dbe2,*dga3,*dbe3;
    float *xh1,*xh2,*xh3;                /* cached normalized pre-activations */
    float *is1,*is2,*is3;                /* cached per-channel inv-std */
};

ConvConfig3 CONV_MED = { 28,28, 16,5,2, 32,5,2, 64,3,1 };

/* deterministic xorshift RNG for He-init */
static uint32_t s_rng = 0x1234ABCDu;
static void   c3_seed(uint32_t s){ s_rng = s ? s : 0x9E3779B9u; }
static float  c3_uni(float lo,float hi){
    s_rng ^= s_rng<<13; s_rng ^= s_rng>>17; s_rng ^= s_rng<<5;
    float u=(float)s_rng/(float)0xFFFFFFFFu; return lo+u*(hi-lo);
}

ConvNet3 *convnet3_create(const ConvConfig3 *cfg){
    if (!cfg) cfg = &CONV_MED;
    ConvNet3 *cn=(ConvNet3*)calloc(1,sizeof(*cn));
    if(!cn) return NULL;
    cn->inH=cfg->inH; cn->inW=cfg->inW;
    cn->K1=cfg->K1; cn->S1=cfg->S1; cn->P1=cfg->P1;
    cn->K2=cfg->K2; cn->S2=cfg->S2; cn->P2=cfg->P2;
    cn->K3=cfg->K3; cn->S3=cfg->S3; cn->P3=cfg->P3;

    cn->c1H=cn->inH-cn->S1+1; cn->c1W=cn->inW-cn->S1+1;
    cn->p1H=cn->c1H/cn->P1;   cn->p1W=cn->c1W/cn->P1;
    cn->c2H=cn->p1H-cn->S2+1; cn->c2W=cn->p1W-cn->S2+1;
    cn->p2H=cn->c2H/cn->P2;   cn->p2W=cn->c2W/cn->P2;
    cn->c3H=cn->p2H-cn->S3+1; cn->c3W=cn->p2W-cn->S3+1;

    cn->w1=malloc((size_t)cn->K1*cn->S1*cn->S1*sizeof(float));
    cn->b1=malloc((size_t)cn->K1*sizeof(float));
    cn->w2=malloc((size_t)cn->K2*cn->K1*cn->S2*cn->S2*sizeof(float));
    cn->b2=malloc((size_t)cn->K2*sizeof(float));
    cn->w3=malloc((size_t)cn->K3*cn->K2*cn->S3*cn->S3*sizeof(float));
    cn->b3=malloc((size_t)cn->K3*sizeof(float));

    cn->c1=malloc((size_t)cn->c1H*cn->c1W*cn->K1*sizeof(float));
    cn->p1=malloc((size_t)cn->p1H*cn->p1W*cn->K1*sizeof(float));
    cn->am1=malloc((size_t)cn->p1H*cn->p1W*cn->K1*sizeof(int));
    cn->c2=malloc((size_t)cn->c2H*cn->c2W*cn->K2*sizeof(float));
    cn->p2=malloc((size_t)cn->p2H*cn->p2W*cn->K2*sizeof(float));
    cn->am2=malloc((size_t)cn->p2H*cn->p2W*cn->K2*sizeof(int));
    cn->c3=malloc((size_t)cn->c3H*cn->c3W*cn->K3*sizeof(float));

    cn->gw1=calloc((size_t)cn->K1*cn->S1*cn->S1,sizeof(float));
    cn->gb1=calloc((size_t)cn->K1,sizeof(float));
    cn->gw2=calloc((size_t)cn->K2*cn->K1*cn->S2*cn->S2,sizeof(float));
    cn->gb2=calloc((size_t)cn->K2,sizeof(float));
    cn->gw3=calloc((size_t)cn->K3*cn->K2*cn->S3*cn->S3,sizeof(float));
    cn->gb3=calloc((size_t)cn->K3,sizeof(float));

    cn->dc1=malloc((size_t)cn->c1H*cn->c1W*cn->K1*sizeof(float));
    cn->dp1=malloc((size_t)cn->c1H*cn->c1W*cn->K1*sizeof(float));
    cn->dc2=malloc((size_t)cn->c2H*cn->c2W*cn->K2*sizeof(float));
    cn->dp2=malloc((size_t)cn->c2H*cn->c2W*cn->K2*sizeof(float));
    cn->dc3=malloc((size_t)cn->c3H*cn->c3W*cn->K3*sizeof(float));

    cn->fdim=cn->K3*cn->c3H*cn->c3W;
    cn->shared_params=0;

    /* instance norm allocations (always allocated; used only if use_in) */
    cn->use_in = getenv("CN_INORM") ? atoi(getenv("CN_INORM")) : 0;
    cn->ga1=malloc((size_t)cn->K1*sizeof(float)); cn->be1=calloc((size_t)cn->K1,sizeof(float));
    cn->ga2=malloc((size_t)cn->K2*sizeof(float)); cn->be2=calloc((size_t)cn->K2,sizeof(float));
    cn->ga3=malloc((size_t)cn->K3*sizeof(float)); cn->be3=calloc((size_t)cn->K3,sizeof(float));
    cn->dga1=calloc((size_t)cn->K1,sizeof(float)); cn->dbe1=calloc((size_t)cn->K1,sizeof(float));
    cn->dga2=calloc((size_t)cn->K2,sizeof(float)); cn->dbe2=calloc((size_t)cn->K2,sizeof(float));
    cn->dga3=calloc((size_t)cn->K3,sizeof(float)); cn->dbe3=calloc((size_t)cn->K3,sizeof(float));
    for(int k=0;k<cn->K1;k++) cn->ga1[k]=1.0f;
    for(int k=0;k<cn->K2;k++) cn->ga2[k]=1.0f;
    for(int k=0;k<cn->K3;k++) cn->ga3[k]=1.0f;
    cn->xh1=malloc((size_t)cn->c1H*cn->c1W*cn->K1*sizeof(float));
    cn->xh2=malloc((size_t)cn->c2H*cn->c2W*cn->K2*sizeof(float));
    cn->xh3=malloc((size_t)cn->c3H*cn->c3W*cn->K3*sizeof(float));
    cn->is1=malloc((size_t)cn->K1*sizeof(float));
    cn->is2=malloc((size_t)cn->K2*sizeof(float));
    cn->is3=malloc((size_t)cn->K3*sizeof(float));

    /* He init + small positive bias so ReLU fires at init (dead-conv fix). */
    c3_seed(0x1234ABCDu);
    float s1=sqrtf(2.0f/(float)(cn->S1*cn->S1));
    for(int i=0;i<cn->K1*cn->S1*cn->S1;i++) cn->w1[i]=c3_uni(-1,1)*s1;
    for(int k=0;k<cn->K1;k++) cn->b1[k]=0.5f;
    float s2=sqrtf(2.0f/(float)(cn->K1*cn->S2*cn->S2));
    for(int i=0;i<cn->K2*cn->K1*cn->S2*cn->S2;i++) cn->w2[i]=c3_uni(-1,1)*s2;
    for(int k=0;k<cn->K2;k++) cn->b2[k]=0.5f;
    float s3=sqrtf(2.0f/(float)(cn->K2*cn->S3*cn->S3));
    for(int i=0;i<cn->K3*cn->K2*cn->S3*cn->S3;i++) cn->w3[i]=c3_uni(-1,1)*s3;
    for(int k=0;k<cn->K3;k++) cn->b3[k]=0.5f;
    return cn;
}

/* Grad-buffer-only replica: allocates caches + grad buffers but ALIASES
 * src's weight pointers (params are read-only during a batch, so no race).
 * Used as a thread-private accumulator in the multithreaded trainer, so we
 * avoid copying weights every batch. Destroy frees weights only if !shared. */
ConvNet3 *convnet3_gradbuf(const ConvNet3 *s){
    if(!s) return NULL;
    ConvNet3 *cn=(ConvNet3*)calloc(1,sizeof(*cn));
    *cn=*s;
    cn->w1=(float*)s->w1; cn->b1=(float*)s->b1;
    cn->w2=(float*)s->w2; cn->b2=(float*)s->b2;
    cn->w3=(float*)s->w3; cn->b3=(float*)s->b3;
    cn->ga1=(float*)s->ga1; cn->be1=(float*)s->be1;
    cn->ga2=(float*)s->ga2; cn->be2=(float*)s->be2;
    cn->ga3=(float*)s->ga3; cn->be3=(float*)s->be3;
    cn->shared_params=1;
    cn->c1=malloc((size_t)s->c1H*s->c1W*s->K1*sizeof(float));
    cn->p1=malloc((size_t)s->p1H*s->p1W*s->K1*sizeof(float));
    cn->am1=malloc((size_t)s->p1H*s->p1W*s->K1*sizeof(int));
    cn->c2=malloc((size_t)s->c2H*s->c2W*s->K2*sizeof(float));
    cn->p2=malloc((size_t)s->p2H*s->p2W*s->K2*sizeof(float));
    cn->am2=malloc((size_t)s->p2H*s->p2W*s->K2*sizeof(int));
    cn->c3=malloc((size_t)s->c3H*s->c3W*s->K3*sizeof(float));
    cn->gw1=calloc((size_t)s->K1*s->S1*s->S1,sizeof(float));
    cn->gb1=calloc((size_t)s->K1,sizeof(float));
    cn->gw2=calloc((size_t)s->K2*s->K1*s->S2*s->S2,sizeof(float));
    cn->gb2=calloc((size_t)s->K2,sizeof(float));
    cn->gw3=calloc((size_t)s->K3*s->K2*s->S3*s->S3,sizeof(float));
    cn->gb3=calloc((size_t)s->K3,sizeof(float));
    cn->dga1=calloc((size_t)s->K1,sizeof(float)); cn->dbe1=calloc((size_t)s->K1,sizeof(float));
    cn->dga2=calloc((size_t)s->K2,sizeof(float)); cn->dbe2=calloc((size_t)s->K2,sizeof(float));
    cn->dga3=calloc((size_t)s->K3,sizeof(float)); cn->dbe3=calloc((size_t)s->K3,sizeof(float));
    cn->dc1=malloc((size_t)s->c1H*s->c1W*s->K1*sizeof(float));
    cn->dp1=malloc((size_t)s->c1H*s->c1W*s->K1*sizeof(float));
    cn->dc2=malloc((size_t)s->c2H*s->c2W*s->K2*sizeof(float));
    cn->dp2=malloc((size_t)s->c2H*s->c2W*s->K2*sizeof(float));
    cn->dc3=malloc((size_t)s->fdim*sizeof(float));
    cn->xh1=malloc((size_t)s->c1H*s->c1W*s->K1*sizeof(float));
    cn->xh2=malloc((size_t)s->c2H*s->c2W*s->K2*sizeof(float));
    cn->xh3=malloc((size_t)s->c3H*s->c3W*s->K3*sizeof(float));
    cn->is1=malloc((size_t)s->K1*sizeof(float));
    cn->is2=malloc((size_t)s->K2*sizeof(float));
    cn->is3=malloc((size_t)s->K3*sizeof(float));
    return cn;
}

void convnet3_destroy(ConvNet3 *cn){
    if(!cn) return;
    if(!cn->shared_params){
        free(cn->w1);free(cn->b1);free(cn->w2);free(cn->b2);free(cn->w3);free(cn->b3);
        free(cn->ga1);free(cn->be1);free(cn->ga2);free(cn->be2);free(cn->ga3);free(cn->be3);
    }
    free(cn->c1);free(cn->p1);free(cn->c2);free(cn->p2);free(cn->c3);
    free(cn->am1);free(cn->am2);
    free(cn->gw1);free(cn->gb1);free(cn->gw2);free(cn->gb2);free(cn->gw3);free(cn->gb3);
    free(cn->dga1);free(cn->dbe1);free(cn->dga2);free(cn->dbe2);free(cn->dga3);free(cn->dbe3);
    free(cn->dc1);free(cn->dp1);free(cn->dc2);free(cn->dp2);free(cn->dc3);
    free(cn->xh1);free(cn->xh2);free(cn->xh3);free(cn->is1);free(cn->is2);free(cn->is3);
    free(cn);
}

int convnet3_dim(const ConvNet3 *cn){ return cn->fdim; }
void convnet3_zero_grad(ConvNet3 *cn){
    memset(cn->gw1,0,(size_t)cn->K1*cn->S1*cn->S1*sizeof(float));
    memset(cn->gb1,0,(size_t)cn->K1*sizeof(float));
    memset(cn->gw2,0,(size_t)cn->K2*cn->K1*cn->S2*cn->S2*sizeof(float));
    memset(cn->gb2,0,(size_t)cn->K2*sizeof(float));
    memset(cn->gw3,0,(size_t)cn->K3*cn->K2*cn->S3*cn->S3*sizeof(float));
    memset(cn->gb3,0,(size_t)cn->K3*sizeof(float));
    memset(cn->dga1,0,(size_t)cn->K1*sizeof(float)); memset(cn->dbe1,0,(size_t)cn->K1*sizeof(float));
    memset(cn->dga2,0,(size_t)cn->K2*sizeof(float)); memset(cn->dbe2,0,(size_t)cn->K2*sizeof(float));
    memset(cn->dga3,0,(size_t)cn->K3*sizeof(float)); memset(cn->dbe3,0,(size_t)cn->K3*sizeof(float));
}
void convnet3_scale_grad(ConvNet3 *cn,float s){
    for(int i=0;i<cn->K1*cn->S1*cn->S1;i++) cn->gw1[i]*=s;
    for(int i=0;i<cn->K1;i++) cn->gb1[i]*=s;
    for(int i=0;i<cn->K2*cn->K1*cn->S2*cn->S2;i++) cn->gw2[i]*=s;
    for(int i=0;i<cn->K2;i++) cn->gb2[i]*=s;
    for(int i=0;i<cn->K3*cn->K2*cn->S3*cn->S3;i++) cn->gw3[i]*=s;
    for(int i=0;i<cn->K3;i++) cn->gb3[i]*=s;
    for(int i=0;i<cn->K1;i++){ cn->dga1[i]*=s; cn->dbe1[i]*=s; }
    for(int i=0;i<cn->K2;i++){ cn->dga2[i]*=s; cn->dbe2[i]*=s; }
    for(int i=0;i<cn->K3;i++){ cn->dga3[i]*=s; cn->dbe3[i]*=s; }
}
/* Accumulate src gradient buffers into dst (for thread-private -> shared
 * reduction). Shared model params are NOT touched; only *grad buffers. */
void convnet3_add_grad(ConvNet3 *dst, const ConvNet3 *src){
    long n;
    n=(long)dst->K1*dst->S1*dst->S1; for(long i=0;i<n;i++) dst->gw1[i]+=src->gw1[i];
    n=(long)dst->K1;                  for(long i=0;i<n;i++) dst->gb1[i]+=src->gb1[i];
    n=(long)dst->K2*dst->K1*dst->S2*dst->S2; for(long i=0;i<n;i++) dst->gw2[i]+=src->gw2[i];
    n=(long)dst->K2;                  for(long i=0;i<n;i++) dst->gb2[i]+=src->gb2[i];
    n=(long)dst->K3*dst->K2*dst->S3*dst->S3; for(long i=0;i<n;i++) dst->gw3[i]+=src->gw3[i];
    n=(long)dst->K3;                  for(long i=0;i<n;i++) dst->gb3[i]+=src->gb3[i];
    for(int i=0;i<dst->K1;i++){ dst->dga1[i]+=src->dga1[i]; dst->dbe1[i]+=src->dbe1[i]; }
    for(int i=0;i<dst->K2;i++){ dst->dga2[i]+=src->dga2[i]; dst->dbe2[i]+=src->dbe2[i]; }
    for(int i=0;i<dst->K3;i++){ dst->dga3[i]+=src->dga3[i]; dst->dbe3[i]+=src->dbe3[i]; }
}

/* Instance-norm a channel's spatial map in-place over `cn->c*` (holding
 * pre-activations), caching xhat and inv-std, then apply gamma/beta and ReLU.
 * act[pos] = relu(gamma*xhat + beta). */
static void in_apply(float *cbuf, float *xh, float *is, const float *ga,
                     const float *be, int K, int H, int W, float leak){
    int N = H*W;
    for(int k=0;k<K;k++){
        float mu=0; for(int p=0;p<N;p++) mu += cbuf[(size_t)p*K+k];
        mu/=N;
        float var=0; for(int p=0;p<N;p++){ float d=cbuf[(size_t)p*K+k]-mu; var+=d*d; }
        var/=N;
        float istd=1.0f/sqrtf(var+IN_EPS); is[k]=istd;
        for(int p=0;p<N;p++){
            float xhat=(cbuf[(size_t)p*K+k]-mu)*istd;
            xh[(size_t)p*K+k]=xhat;
            float yv=ga[k]*xhat+be[k];
            cbuf[(size_t)p*K+k]= yv>0? yv : leak*yv;
        }
    }
}

/* ---------------- forward ---------------- */
void convnet3_forward(const ConvNet3 *cn, const float *img, float *out){
    float leak = getenv("CN_LEAK") ? (float)atof(getenv("CN_LEAK")) : 0.0f;
    /* stage1 conv (store pre-activation into c1) */
    for(int k=0;k<cn->K1;k++) for(int y=0;y<cn->c1H;y++) for(int x=0;x<cn->c1W;x++){
        float s=cn->b1[k]; const float *wk=cn->w1+(size_t)k*cn->S1*cn->S1;
        for(int dy=0;dy<cn->S1;dy++) for(int dx=0;dx<cn->S1;dx++){
            s+=wk[dy*cn->S1+dx]*img[((size_t)(y+dy)*cn->inW+(x+dx))];
        }
        cn->c1[((size_t)y*cn->c1W+x)*cn->K1+k]=s;
    }
    if(cn->use_in) in_apply(cn->c1,cn->xh1,cn->is1,cn->ga1,cn->be1,cn->K1,cn->c1H,cn->c1W,leak);
    else for(int i=0;i<cn->c1H*cn->c1W*cn->K1;i++){ float a=cn->c1[i]; cn->c1[i]=a>0?a:leak*a; }
    /* stage1 pool */
    for(int k=0;k<cn->K1;k++) for(int py=0;py<cn->p1H;py++) for(int px=0;px<cn->p1W;px++){
        int best=0; float bv=-1e30f;
        for(int dy=0;dy<cn->P1;dy++) for(int dx=0;dx<cn->P1;dx++){
            int iy=py*cn->P1+dy, ix=px*cn->P1+dx;
            float v=cn->c1[((size_t)iy*cn->c1W+ix)*cn->K1+k];
            if(v>bv){bv=v;best=iy*cn->c1W+ix;}
        }
        cn->p1[((size_t)py*cn->p1W+px)*cn->K1+k]=bv;
        cn->am1[((size_t)py*cn->p1W+px)*cn->K1+k]=best;
    }
    /* stage2 over p1 (store pre-activation into c2) */
    for(int k=0;k<cn->K2;k++) for(int y=0;y<cn->c2H;y++) for(int x=0;x<cn->c2W;x++){
        float s=cn->b2[k]; const float *wk=cn->w2+(size_t)k*cn->K1*cn->S2*cn->S2;
        for(int c=0;c<cn->K1;c++) for(int dy=0;dy<cn->S2;dy++) for(int dx=0;dx<cn->S2;dx++){
            s+=wk[((c*cn->S2+dy)*cn->S2+dx)]*cn->p1[((size_t)(y+dy)*cn->p1W+(x+dx))*cn->K1+c];
        }
        cn->c2[((size_t)y*cn->c2W+x)*cn->K2+k]=s;
    }
    if(cn->use_in) in_apply(cn->c2,cn->xh2,cn->is2,cn->ga2,cn->be2,cn->K2,cn->c2H,cn->c2W,leak);
    else for(int i=0;i<cn->c2H*cn->c2W*cn->K2;i++){ float a=cn->c2[i]; cn->c2[i]=a>0?a:leak*a; }
    /* stage2 pool */
    for(int k=0;k<cn->K2;k++) for(int py=0;py<cn->p2H;py++) for(int px=0;px<cn->p2W;px++){
        int best=0; float bv=-1e30f;
        for(int dy=0;dy<cn->P2;dy++) for(int dx=0;dx<cn->P2;dx++){
            int iy=py*cn->P2+dy, ix=px*cn->P2+dx;
            float v=cn->c2[((size_t)iy*cn->c2W+ix)*cn->K2+k];
            if(v>bv){bv=v;best=iy*cn->c2W+ix;}
        }
        cn->p2[((size_t)py*cn->p2W+px)*cn->K2+k]=bv;
        cn->am2[((size_t)py*cn->p2W+px)*cn->K2+k]=best;
    }
    /* stage3 over p2 (no pool; store pre-activation into c3) */
    for(int k=0;k<cn->K3;k++) for(int y=0;y<cn->c3H;y++) for(int x=0;x<cn->c3W;x++){
        float s=cn->b3[k]; const float *wk=cn->w3+(size_t)k*cn->K2*cn->S3*cn->S3;
        for(int c=0;c<cn->K2;c++) for(int dy=0;dy<cn->S3;dy++) for(int dx=0;dx<cn->S3;dx++){
            s+=wk[((c*cn->S3+dy)*cn->S3+dx)]*cn->p2[((size_t)(y+dy)*cn->p2W+(x+dx))*cn->K2+c];
        }
        cn->c3[((size_t)y*cn->c3W+x)*cn->K3+k]=s;
    }
    if(cn->use_in) in_apply(cn->c3,cn->xh3,cn->is3,cn->ga3,cn->be3,cn->K3,cn->c3H,cn->c3W,leak);
    else for(int i=0;i<cn->c3H*cn->c3W*cn->K3;i++){ float a=cn->c3[i]; cn->c3[i]=a>0?a:leak*a; }
    /* features = c3 flattened */
    for(int i=0;i<cn->fdim;i++) out[i]=cn->c3[i];
}

/* Instance-norm backward for one stage. On entry `dact` holds dL/d(activation)
 * (post-ReLU) for the whole map; `cbuf` holds the post-ReLU activation (for the
 * ReLU mask). Produces dL/d(pre-activation) IN PLACE into `dact`, accumulates
 * gamma/beta grads. Formula: dpre = istd*(dxhat - mean(dxhat) - xhat*mean(dxhat*xhat)). */
static void in_backward(float *dact, const float *cbuf, const float *xh,
                        const float *is, const float *ga, float *dga, float *dbe,
                        int K, int H, int W, float leak){
    int N=H*W;
    for(int k=0;k<K;k++){
        /* pass A: relu mask -> dy (in place in dact), accumulate dga/dbe + sums */
        float sdx=0, sdxx=0;
        for(int p=0;p<N;p++){
            size_t idx=(size_t)p*K+k;
            float dy=dact[idx]; if(cbuf[idx]<=0.0f) dy*=leak; /* relu' */
            dact[idx]=dy;
            dga[k]+=dy*xh[idx]; dbe[k]+=dy;
            float dxhat=dy*ga[k];
            sdx+=dxhat; sdxx+=dxhat*xh[idx];
        }
        float m1=sdx/N, m2=sdxx/N, istd=is[k];
        /* pass B: dpre in place */
        for(int p=0;p<N;p++){
            size_t idx=(size_t)p*K+k;
            float dxhat=dact[idx]*ga[k];
            dact[idx]=istd*(dxhat - m1 - xh[idx]*m2);
        }
    }
}

/* ---------------- backward ---------------- */
void convnet3_backward(ConvNet3 *cn, const float *img, const float *feat, const float *dfeat){
    (void)feat;
    float leak = getenv("CN_LEAK") ? (float)atof(getenv("CN_LEAK")) : 0.0f;
    /* df -> dL/d(act3) (=dc3) */
    for(int i=0;i<cn->fdim;i++) cn->dc3[i]=dfeat[i];
    /* stage3: convert dc3 (post-relu grad) -> dpre3 */
    if(cn->use_in) in_backward(cn->dc3,cn->c3,cn->xh3,cn->is3,cn->ga3,cn->dga3,cn->dbe3,cn->K3,cn->c3H,cn->c3W,leak);
    /* stage3 backward: dpre3 -> gw3,gb3, dp2 */
    for(int i=0;i<cn->c2H*cn->c2W*cn->K2;i++) cn->dp2[i]=0;
    for(int k=0;k<cn->K3;k++) for(int y=0;y<cn->c3H;y++) for(int x=0;x<cn->c3W;x++){
        size_t oidx=((size_t)y*cn->c3W+x)*cn->K3+k;
        float g=cn->dc3[oidx];
        if(!cn->use_in && cn->c3[oidx]<=0.0f) g*=leak; /* relu mask (in_backward already applied it) */
        cn->gb3[k]+=g;
        const float *wk=cn->w3+(size_t)k*cn->K2*cn->S3*cn->S3;
        for(int c=0;c<cn->K2;c++) for(int dy=0;dy<cn->S3;dy++) for(int dx=0;dx<cn->S3;dx++){
            int iy=y+dy, ix=x+dx; int pidx=((size_t)iy*cn->p2W+ix)*cn->K2+c;
            cn->gw3[((size_t)k*cn->K2*cn->S3*cn->S3)+((c*cn->S3+dy)*cn->S3+dx)] += g*cn->p2[pidx];
            cn->dp2[pidx]+=g*wk[((c*cn->S3+dy)*cn->S3+dx)];
        }
    }
    /* stage2 pool backward: dp2 -> dc2 */
    for(int i=0;i<cn->c2H*cn->c2W*cn->K2;i++) cn->dc2[i]=0;
    for(int k=0;k<cn->K2;k++) for(int py=0;py<cn->p2H;py++) for(int px=0;px<cn->p2W;px++){
        int a=cn->am2[((size_t)py*cn->p2W+px)*cn->K2+k];
        cn->dc2[(size_t)a*cn->K2+k]+=cn->dp2[((size_t)py*cn->p2W+px)*cn->K2+k];
    }
    /* stage2: convert dc2 (post-relu grad) -> dpre2 */
    if(cn->use_in) in_backward(cn->dc2,cn->c2,cn->xh2,cn->is2,cn->ga2,cn->dga2,cn->dbe2,cn->K2,cn->c2H,cn->c2W,leak);
    /* stage2 conv backward: dpre2 -> gw2,gb2, dp1 */
    for(int i=0;i<cn->c1H*cn->c1W*cn->K1;i++) cn->dp1[i]=0;
    for(int k=0;k<cn->K2;k++) for(int y=0;y<cn->c2H;y++) for(int x=0;x<cn->c2W;x++){
        size_t oidx=((size_t)y*cn->c2W+x)*cn->K2+k;
        float g=cn->dc2[oidx];
        if(!cn->use_in && cn->c2[oidx]<=0.0f) g*=leak;
        cn->gb2[k]+=g;
        const float *wk=cn->w2+(size_t)k*cn->K1*cn->S2*cn->S2;
        for(int c=0;c<cn->K1;c++) for(int dy=0;dy<cn->S2;dy++) for(int dx=0;dx<cn->S2;dx++){
            int iy=y+dy, ix=x+dx; int pidx=((size_t)iy*cn->p1W+ix)*cn->K1+c;
            cn->gw2[((size_t)k*cn->K1*cn->S2*cn->S2)+((c*cn->S2+dy)*cn->S2+dx)] += g*cn->p1[pidx];
            cn->dp1[pidx]+=g*wk[((c*cn->S2+dy)*cn->S2+dx)];
        }
    }
    /* stage1 pool backward: dp1 -> dc1 */
    for(int i=0;i<cn->c1H*cn->c1W*cn->K1;i++) cn->dc1[i]=0;
    for(int k=0;k<cn->K1;k++) for(int py=0;py<cn->p1H;py++) for(int px=0;px<cn->p1W;px++){
        int a=cn->am1[((size_t)py*cn->p1W+px)*cn->K1+k];
        cn->dc1[(size_t)a*cn->K1+k]+=cn->dp1[((size_t)py*cn->p1W+px)*cn->K1+k];
    }
    /* stage1: convert dc1 (post-relu grad) -> dpre1 */
    if(cn->use_in) in_backward(cn->dc1,cn->c1,cn->xh1,cn->is1,cn->ga1,cn->dga1,cn->dbe1,cn->K1,cn->c1H,cn->c1W,leak);
    /* stage1 conv backward: dpre1 -> gw1,gb1 */
    for(int k=0;k<cn->K1;k++) for(int y=0;y<cn->c1H;y++) for(int x=0;x<cn->c1W;x++){
        size_t cidx=((size_t)y*cn->c1W+x)*cn->K1+k;
        float dpre=cn->dc1[cidx];
        if(!cn->use_in && cn->c1[cidx]<=0) dpre *= leak;
        cn->gb1[k]+=dpre;
        float *wk=cn->gw1+(size_t)k*cn->S1*cn->S1;
        for(int dy=0;dy<cn->S1;dy++) for(int dx=0;dx<cn->S1;dx++){
            wk[dy*cn->S1+dx]+=dpre*img[((size_t)(y+dy)*cn->inW+(x+dx))];
        }
    }
}

int convnet3_layer_count(const ConvNet3 *cn){ return cn->use_in ? 12 : 6; }
ConvLayer3 convnet3_layer(ConvNet3 *cn,int idx){
    ConvLayer3 L={0};
    switch(idx){
        case 0: L.param=cn->w1; L.grad=cn->gw1; L.n=cn->K1*cn->S1*cn->S1; break;
        case 1: L.param=cn->b1; L.grad=cn->gb1; L.n=cn->K1; break;
        case 2: L.param=cn->w2; L.grad=cn->gw2; L.n=cn->K2*cn->K1*cn->S2*cn->S2; break;
        case 3: L.param=cn->b2; L.grad=cn->gb2; L.n=cn->K2; break;
        case 4: L.param=cn->w3; L.grad=cn->gw3; L.n=cn->K3*cn->K2*cn->S3*cn->S3; break;
        case 5: L.param=cn->b3; L.grad=cn->gb3; L.n=cn->K3; break;
        /* instance-norm gamma/beta (only reached when use_in -> count==12) */
        case 6: L.param=cn->ga1; L.grad=cn->dga1; L.n=cn->K1; break;
        case 7: L.param=cn->be1; L.grad=cn->dbe1; L.n=cn->K1; break;
        case 8: L.param=cn->ga2; L.grad=cn->dga2; L.n=cn->K2; break;
        case 9: L.param=cn->be2; L.grad=cn->dbe2; L.n=cn->K2; break;
        case 10:L.param=cn->ga3; L.grad=cn->dga3; L.n=cn->K3; break;
        default:L.param=cn->be3; L.grad=cn->dbe3; L.n=cn->K3; break;
    }
    return L;
}

/* ---------------- save/load ---------------- */
static void dmp(FILE*f,const float*a,int n){ for(int i=0;i<n;i++) fprintf(f,"%a\n",a[i]); }
static int rdf(FILE*f,float*a,int n){ for(int i=0;i<n;i++) if(fscanf(f,"%a\n",&a[i])!=1) return -1; return 0; }
int convnet3_save(const ConvNet3 *cn,const char*path){
    FILE*f=fopen(path,"w"); if(!f) return -1;
    fprintf(f,"wubu_ocr_conv3_v1 %d %d %d %d %d %d %d %d %d %d %d\n",
        cn->inH,cn->inW,cn->K1,cn->S1,cn->P1,cn->K2,cn->S2,cn->P2,cn->K3,cn->S3,cn->P3);
    dmp(f,cn->w1,cn->K1*cn->S1*cn->S1); dmp(f,cn->b1,cn->K1);
    dmp(f,cn->w2,cn->K2*cn->K1*cn->S2*cn->S2); dmp(f,cn->b2,cn->K2);
    dmp(f,cn->w3,cn->K3*cn->K2*cn->S3*cn->S3); dmp(f,cn->b3,cn->K3);
    if(cn->use_in){
        fprintf(f,"inorm 1\n");
        dmp(f,cn->ga1,cn->K1); dmp(f,cn->be1,cn->K1);
        dmp(f,cn->ga2,cn->K2); dmp(f,cn->be2,cn->K2);
        dmp(f,cn->ga3,cn->K3); dmp(f,cn->be3,cn->K3);
    }
    fclose(f); return 0;
}
int convnet3_load(const char*path,ConvNet3**out,ConvConfig3*cfg){
    FILE*f=fopen(path,"r"); if(!f) return -1;
    ConvConfig3 c; int a,b,K1,S1,P1,K2,S2,P2,K3,S3,P3;
    if(fscanf(f,"wubu_ocr_conv3_v1 %d %d %d %d %d %d %d %d %d %d %d\n",
        &a,&b,&K1,&S1,&P1,&K2,&S2,&P2,&K3,&S3,&P3)!=11){fclose(f);return -1;}
    c=(ConvConfig3){a,b,K1,S1,P1,K2,S2,P2,K3,S3,P3};
    ConvNet3*cn=convnet3_create(&c); if(!cn){fclose(f);return -1;}
    if(rdf(f,cn->w1,cn->K1*cn->S1*cn->S1)){convnet3_destroy(cn);fclose(f);return -1;}
    if(rdf(f,cn->b1,cn->K1)){convnet3_destroy(cn);fclose(f);return -1;}
    if(rdf(f,cn->w2,cn->K2*cn->K1*cn->S2*cn->S2)){convnet3_destroy(cn);fclose(f);return -1;}
    if(rdf(f,cn->b2,cn->K2)){convnet3_destroy(cn);fclose(f);return -1;}
    if(rdf(f,cn->w3,cn->K3*cn->K2*cn->S3*cn->S3)){convnet3_destroy(cn);fclose(f);return -1;}
    if(rdf(f,cn->b3,cn->K3)){convnet3_destroy(cn);fclose(f);return -1;}
    /* optional instance-norm block */
    int inflag=0;
    if(fscanf(f,"inorm %d\n",&inflag)==1 && inflag){
        cn->use_in=1;
        if(rdf(f,cn->ga1,cn->K1)||rdf(f,cn->be1,cn->K1)||
           rdf(f,cn->ga2,cn->K2)||rdf(f,cn->be2,cn->K2)||
           rdf(f,cn->ga3,cn->K3)||rdf(f,cn->be3,cn->K3)){convnet3_destroy(cn);fclose(f);return -1;}
    }
    fclose(f); *out=cn; if(cfg)*cfg=c; return 0;
}
