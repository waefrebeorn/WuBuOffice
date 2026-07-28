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
#include "conv_gemm.h"
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
    float leak;             /* ReLU leak, read ONCE at create (not per-call) */
    /* im2col column scratch (forward + backward GEMM targets) -- separate from
     * the dc gradient maps so col2im never aliases its own source. */
    float *col1,*col2,*col3;
    /* --- instance norm --- */
    int    use_in;                       /* instance norm enabled */
    float *ga1,*be1,*ga2,*be2,*ga3,*be3; /* per-channel gamma/beta */
    float *dga1,*dbe1,*dga2,*dbe2,*dga3,*dbe3;
    float *xh1,*xh2,*xh3;                /* cached normalized pre-activations */
    float *is1,*is2,*is3;                /* cached per-channel inv-std */
    /* --- CBAM attention (env CBAM=1): channel + spatial, applied on p2 --- */
    int    use_cbam;
    int    cbam_r;                       /* channel MLP bottleneck */
    float *ca_w1,*ca_b1,*ca_w2,*ca_b2;   /* channel att MLP (2*K2 -> r -> K2) */
    float *sa_w;                         /* spatial att: 2 weights (avg,max) */
    float *gca_w1,*gca_b1,*gca_w2,*gca_b2,*gsa_w;
    float *cbam_avg,*cbam_max;           /* cached channel avg/max (p2H*p2W x K2) */
    float *cbam_mc;                      /* channel att map (K2) */
    float *cbam_sa;                      /* spatial att map (p2H*p2W) */
};

ConvConfig3 CONV_MED = { 28,28, 16,5,2, 32,5,2, 64,3,1 };  /* 16→32→64 filters, 2×2×64=256 feats */
ConvConfig3 CONV_MED_PAD = { 32,32, 16,5,2, 32,5,2, 64,3,1 };  /* 32×32 padded input → 3×3×64=576 feats */
ConvConfig3 CONV_TINY = { 28,28, 32,5,2, 0,1,1, 0,1,1 };  /* single stage — NOT used (fdim=0 with K3=0) */
ConvConfig3 CONV_WIDE = { 28,28, 32,3,2, 64,3,2, 128,3,1 };  /* 3×3 kernels, 32→64→128, 3×3×128=1152 feats */
ConvConfig3 CONV_XL = { 28,28, 64,3,2, 128,3,2, 256,3,1 };  /* 3×3 kernels, 64→128→256, 3×3×256=2304 feats */
/* 2-STAGE (gradcheck-verified correct backward): no stage3. */
ConvConfig3 CONV_2STAGE = { 28,28, 32,3,2, 64,3,2, 0,1,1 };  /* 32→64, pool both, 5x5x64=1600 feats */
/* LARGE final map: stride-1 convs, ONE pool (after stage1) -> ~7x7xK3.
 * Avoids the 8x per-axis crush of MED_PAD; keeps spatial stroke info. */
ConvConfig3 CONV_BIGMAP = { 32,32, 32,3,1, 64,3,1, 128,3,1 };  /* stride-1, 1 pool -> 7x7x128=6272 feats; thicker filters */

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

    /* im2col column scratch, sized per stage (forward + backward GEMM). */
    cn->col1=malloc((size_t)cn->c1H*cn->c1W*cn->S1*cn->S1*sizeof(float));
    cn->col2=malloc((size_t)cn->c2H*cn->c2W*cn->S2*cn->S2*cn->K1*sizeof(float));
    cn->col3=malloc((size_t)cn->c3H*cn->c3W*cn->S3*cn->S3*cn->K2*sizeof(float));

    /* output feature dim = post-pool tensor of the LAST active stage
     * (stage3 has no pool -> c3; stage2 -> p2; stage1 -> p1) */
    if(cn->K3>0)      cn->fdim = cn->c3H*cn->c3W*cn->K3;
    else if(cn->K2>0) cn->fdim = cn->p2H*cn->p2W*cn->K2;
    else              cn->fdim = cn->p1H*cn->p1W*cn->K1;
    cn->shared_params=0;

    /* instance norm allocations (always allocated; used only if use_in) */
    cn->use_in = getenv("CN_INORM") ? atoi(getenv("CN_INORM")) : 0;
    cn->leak   = getenv("CN_LEAK") ? (float)atof(getenv("CN_LEAK")) : 0.1f;  /* leaky ReLU (0.1) prevents dead-conv collapse; MLP already leaky */
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

    /* CBAM attention init (only used if use_cbam) */
    cn->use_cbam = getenv("CBAM")? atoi(getenv("CBAM")):0;
    cn->cbam_r = 2;
    int K2=cn->K2, p2n=cn->p2H*cn->p2W;
    cn->ca_w1=malloc((size_t)cn->cbam_r*2*K2*sizeof(float)); cn->ca_b1=calloc((size_t)cn->cbam_r,sizeof(float));
    cn->ca_w2=malloc((size_t)K2*cn->cbam_r*sizeof(float)); cn->ca_b2=calloc((size_t)K2,sizeof(float));
    cn->sa_w=calloc(2,sizeof(float)); cn->sa_w[0]=1.0f; cn->sa_w[1]=1.0f;
    cn->gca_w1=calloc((size_t)cn->cbam_r*2*K2,sizeof(float)); cn->gca_b1=calloc((size_t)cn->cbam_r,sizeof(float));
    cn->gca_w2=calloc((size_t)K2*cn->cbam_r,sizeof(float)); cn->gca_b2=calloc((size_t)K2,sizeof(float));
    cn->gsa_w=calloc(2,sizeof(float));
    cn->cbam_avg=malloc((size_t)p2n*K2*sizeof(float)); cn->cbam_max=malloc((size_t)p2n*K2*sizeof(float));
    cn->cbam_mc=malloc((size_t)K2*sizeof(float)); cn->cbam_sa=malloc((size_t)p2n*sizeof(float));
    if(cn->use_cbam){ /* small He-ish init for channel MLP */
        uint32_t s=0xABCDEu; for(int i=0;i<cn->cbam_r*2*K2;i++) cn->ca_w1[i]=((float)(s=(s*1103515245u+12345u)>>16)/32768.0f-1.0f)*0.1f;
        for(int i=0;i<K2*cn->cbam_r;i++) cn->ca_w2[i]=((float)(s=(s*1103515245u+12345u)>>16)/32768.0f-1.0f)*0.1f;
    }

    /* He init + small POSITIVE bias so ReLU actually fires at init
     * (dead-conv fix). Zero bias + zero-mean weights => pre-activation ~0 =>
     * dead ReLU => no gradient => stuck-at-random. Bias 0.1 guarantees firing. */
    float s1=sqrtf(2.0f/(float)(cn->S1*cn->S1));
    for(int i=0;i<cn->K1*cn->S1*cn->S1;i++) cn->w1[i]=c3_uni(-1,1)*s1;
    for(int k=0;k<cn->K1;k++) cn->b1[k]=0.1f;
    float s2=sqrtf(2.0f/(float)(cn->K1*cn->S2*cn->S2));
    for(int i=0;i<cn->K2*cn->K1*cn->S2*cn->S2;i++) cn->w2[i]=c3_uni(-1,1)*s2;
    for(int k=0;k<cn->K2;k++) cn->b2[k]=0.1f;
    float s3=sqrtf(2.0f/(float)(cn->K2*cn->S3*cn->S3));
    for(int i=0;i<cn->K3*cn->K2*cn->S3*cn->S3;i++) cn->w3[i]=c3_uni(-1,1)*s3;
    for(int k=0;k<cn->K3;k++) cn->b3[k]=0.1f;
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
    cn->ca_w1=(float*)s->ca_w1; cn->ca_b1=(float*)s->ca_b1;
    cn->ca_w2=(float*)s->ca_w2; cn->ca_b2=(float*)s->ca_b2;
    cn->sa_w=(float*)s->sa_w;
    cn->shared_params=1;
    /* Keep use_in from source so INORM runs during forward/backward.
     * CBAM is disabled on gradbuf; NULL its scratch and buffer pointers
     * so destroy doesn't double-free the master model's allocations. */
    cn->use_cbam=0;
    cn->cbam_avg=cn->cbam_max=cn->cbam_mc=cn->cbam_sa=NULL;
    /* CBAM gradient buffers are not allocated — gradbuf has use_cbam=0 so
     * CBAM forward/backward code paths are never entered. */
    /* Shallow `*cn=*s` (line 177) aliased s->c1 (the activation cache). c1 is
     * a PER-REPLICA activation buffer (each worker writes its own forwards into
     * it), so it MUST be private -- leaving it aliased would make every
     * convnet3_destroy(cnT[t]) AND convnet3_destroy(cn) free the SAME s->c1
     * pointer -> double/triple free at cleanup. Allocate fresh (the other
     * caches p1/c2/p2/c3 are already private below). */
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
    cn->col1=malloc((size_t)s->c1H*s->c1W*s->S1*s->S1*sizeof(float));
    cn->col2=malloc((size_t)s->c2H*s->c2W*s->S2*s->S2*s->K1*sizeof(float));
    cn->col3=malloc((size_t)s->c3H*s->c3W*s->S3*s->S3*s->K2*sizeof(float));
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
        free(cn->ca_w1);free(cn->ca_b1);free(cn->ca_w2);free(cn->ca_b2);free(cn->sa_w);
        free(cn->gca_w1);free(cn->gca_b1);free(cn->gca_w2);free(cn->gca_b2);free(cn->gsa_w);
    }
    free(cn->c1);free(cn->p1);free(cn->c2);free(cn->p2);free(cn->c3);
    free(cn->am1);free(cn->am2);
    free(cn->gw1);free(cn->gb1);free(cn->gw2);free(cn->gb2);free(cn->gw3);free(cn->gb3);
    free(cn->dga1);free(cn->dbe1);free(cn->dga2);free(cn->dbe2);free(cn->dga3);free(cn->dbe3);
    free(cn->dc1);free(cn->dp1);free(cn->dc2);free(cn->dp2);free(cn->dc3);
    free(cn->col1);free(cn->col2);free(cn->col3);
    free(cn->xh1);free(cn->xh2);free(cn->xh3);free(cn->is1);free(cn->is2);free(cn->is3);
    free(cn->cbam_avg);free(cn->cbam_max);free(cn->cbam_mc);free(cn->cbam_sa);
    free(cn);
}

int convnet3_cbam_size(const ConvNet3 *cn){
    if(!cn || !cn->use_cbam) return 0;
    return cn->cbam_r*2*cn->K2 + cn->cbam_r + cn->K2*cn->cbam_r + cn->K2 + 2;
}
void convnet3_cbam_pack(const ConvNet3 *cn, float *o){
    if(!cn||!cn->use_cbam) return;
    int K=cn->K2, r=cn->cbam_r; size_t off=0;
    memcpy(o+off,cn->ca_w1,(size_t)r*2*K*sizeof(float)); off+=r*2*K;
    memcpy(o+off,cn->ca_b1,(size_t)r*sizeof(float)); off+=r;
    memcpy(o+off,cn->ca_w2,(size_t)K*r*sizeof(float)); off+=K*r;
    memcpy(o+off,cn->ca_b2,(size_t)K*sizeof(float)); off+=K;
    memcpy(o+off,cn->sa_w,2*sizeof(float)); off+=2;
}
void convnet3_cbam_unpack(ConvNet3 *cn, const float *i){
    if(!cn||!cn->use_cbam) return;
    int K=cn->K2, r=cn->cbam_r; size_t off=0;
    memcpy(cn->ca_w1,i+off,(size_t)r*2*K*sizeof(float)); off+=r*2*K;
    memcpy(cn->ca_b1,i+off,(size_t)r*sizeof(float)); off+=r;
    memcpy(cn->ca_w2,i+off,(size_t)K*r*sizeof(float)); off+=K*r;
    memcpy(cn->ca_b2,i+off,(size_t)K*sizeof(float)); off+=K;
    memcpy(cn->sa_w,i+off,2*sizeof(float)); off+=2;
}
void convnet3_enable_cbam(ConvNet3 *cn){ if(cn) cn->use_cbam=1; }
/* Plain-SGD step on CBAM attention weights (tiny module ~ a few hundred params;
 * Adam on the trunk is overkill here). grad buffers are zeroed by convnet3_zero_grad. */
void convnet3_sgd_cbam(ConvNet3 *cn, float lr){
    if(!cn||!cn->use_cbam) return;
    int K=cn->K2, r=cn->cbam_r; size_t off=0, n;
    n=(size_t)r*2*K; for(size_t i=0;i<n;i++) cn->ca_w1[i]-=lr*cn->gca_w1[i]; off+=n;
    n=(size_t)r;     for(size_t i=0;i<n;i++) cn->ca_b1[i]-=lr*cn->gca_b1[i]; off+=n;
    n=(size_t)K*r;   for(size_t i=0;i<n;i++) cn->ca_w2[i]-=lr*cn->gca_w2[i]; off+=n;
    n=(size_t)K;     for(size_t i=0;i<n;i++) cn->ca_b2[i]-=lr*cn->gca_b2[i]; off+=n;
    n=2;             for(size_t i=0;i<n;i++) cn->sa_w[i]-=lr*cn->gsa_w[i]; off+=n;
}
int convnet3_cbam_enabled(const ConvNet3 *cn){ return cn? cn->use_cbam : 0; }

/* Enable instance norm (buffers already allocated at create time) */
int convnet3_enable_inorm(ConvNet3 *cn){
    if(cn){ cn->use_in = 1; return 1; }
    return 0;
}

/* Set ReLU leak slope (read at create, but allow override for testing) */
void convnet3_set_leak(ConvNet3 *cn, float leak){
    if(cn) cn->leak = leak;
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
    if(cn->use_cbam){
        memset(cn->gca_w1,0,(size_t)cn->cbam_r*2*cn->K2*sizeof(float));
        memset(cn->gca_b1,0,(size_t)cn->cbam_r*sizeof(float));
        memset(cn->gca_w2,0,(size_t)cn->K2*cn->cbam_r*sizeof(float));
        memset(cn->gca_b2,0,(size_t)cn->K2*sizeof(float));
        memset(cn->gsa_w,0,2*sizeof(float));
    }
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
    /* consume: zero the source gradbuf so the next batch starts fresh (the
     * per-thread worker accumulates its samples into this buffer, and without
     * this reset gradients would leak across batches and explode). */
    convnet3_zero_grad((ConvNet3*)src);
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

/* conv GEMM + im2col/col2im compute core lives in conv_gemm.c (see conv_gemm.h). */

/* ---------------- CBAM attention (forward + backward) ----------------
 * Channel att: per-channel avg & max over spatial -> MLP(2*K2 -> r -> K2) ->
 * sigmoid -> Mc (K2). Spatial att: per-position avg & max over channels ->
 * sigmoid(w0*avg + w1*max) -> Sa (P2N). p2[p][k] *= Mc[k]*Sa[p]. */
static float cbam_sigf(float x){ return 1.0f/(1.0f+expf(-x)); }

static void cbam_fwd(ConvNet3 *cn){
    int K=cn->K2, P=cn->p2H*cn->p2W, r=cn->cbam_r;
    float *p2=cn->p2;
    for(int k=0;k<K;k++){
        float av=0,mx=-1e30f; for(int p=0;p<P;p++){ float v=p2[(size_t)p*K+k]; av+=v; if(v>mx)mx=v; } av/=P;
        cn->cbam_avg[k]=av; cn->cbam_max[k]=mx;
    }
    for(int k=0;k<K;k++){
        float h[r>0?r:1];
        for(int i=0;i<r;i++){
            float s=cn->ca_b1[i];
            s+=cn->ca_w1[(size_t)i*2*K+k]*cn->cbam_avg[k];
            s+=cn->ca_w1[(size_t)i*2*K+K+k]*cn->cbam_max[k];
            h[i]=(s>0?s:0.0f);
        }
        float s2=cn->ca_b2[k];
        for(int i=0;i<r;i++) s2+=cn->ca_w2[(size_t)k*r+i]*h[i];
        cn->cbam_mc[k]=cbam_sigf(s2);
    }
    for(int p=0;p<P;p++){
        float av=0,mx=-1e30f; for(int k=0;k<K;k++){ float v=p2[(size_t)p*K+k]; av+=v; if(v>mx)mx=v; } av/=K;
        cn->cbam_sa[p]=cbam_sigf(cn->sa_w[0]*av + cn->sa_w[1]*mx);
    }
    for(int p=0;p<P;p++) for(int k=0;k<K;k++) p2[(size_t)p*K+k]*=cn->cbam_mc[k]*cn->cbam_sa[p];
}

static void cbam_bwd(ConvNet3 *cn, float *dp2){
    int K=cn->K2, P=cn->p2H*cn->p2W, r=cn->cbam_r;
    float *p2=cn->p2;
    for(int p=0;p<P;p++) for(int k=0;k<K;k++) dp2[(size_t)p*K+k]*=cn->cbam_mc[k]*cn->cbam_sa[p];
    for(int k=0;k<K;k++){
        float dMc=0;
        for(int p=0;p<P;p++){ float p2orig=p2[(size_t)p*K+k]/(cn->cbam_mc[k]*cn->cbam_sa[p]); dMc+=dp2[(size_t)p*K+k]*p2orig*cn->cbam_sa[p]; }
        float sm=cn->cbam_mc[k]*(1.0f-cn->cbam_mc[k]);
        float dsig=dMc*sm;
        cn->gca_b2[k]+=dsig;
        float h[r>0?r:1];
        for(int i=0;i<r;i++){ float s=cn->ca_b1[i]; s+=cn->ca_w1[(size_t)i*2*K+k]*cn->cbam_avg[k]; s+=cn->ca_w1[(size_t)i*2*K+K+k]*cn->cbam_max[k]; h[i]=(s>0?s:0.0f); }
        for(int i=0;i<r;i++) cn->gca_w2[(size_t)k*r+i]+=dsig*h[i];
        for(int i=0;i<r;i++){ float dh=(h[i]>0?dsig:0.0f); cn->gca_b1[i]+=dh; cn->gca_w1[(size_t)i*2*K+k]+=dh*cn->cbam_avg[k]; cn->gca_w1[(size_t)i*2*K+K+k]+=dh*cn->cbam_max[k]; }
    }
    for(int p=0;p<P;p++){
        float av=0,mx=-1e30f; for(int k=0;k<K;k++){ float v=p2[(size_t)p*K+k]/(cn->cbam_mc[k]*cn->cbam_sa[p]); av+=v; if(v>mx)mx=v; } av/=K;
        float dSa=0; for(int k=0;k<K;k++){ float p2orig=p2[(size_t)p*K+k]/(cn->cbam_mc[k]*cn->cbam_sa[p]); dSa+=dp2[(size_t)p*K+k]*p2orig*cn->cbam_mc[k]; }
        float ss=cn->cbam_sa[p]*(1.0f-cn->cbam_sa[p]);
        float dsig=dSa*ss;
        cn->gsa_w[0]+=dsig*av; cn->gsa_w[1]+=dsig*mx;
    }
}


/* ---------------- forward ---------------- */
void convnet3_forward(const ConvNet3 *cn, const float *img, float *out){
    float leak = cn->leak;
    float *COL1 = cn->col1, *COL2 = cn->col2, *COL3 = cn->col3;

    /* stage1: img[H][W][1] -> c1   (P1 = S1*S1) */
    conv_im2col(img, cn->inH, cn->inW, 1, cn->S1, cn->c1H, cn->c1W, COL1);
    conv_gemm_fwd(cn->c1, COL1, cn->w1, cn->c1H*cn->c1W, cn->K1, cn->S1*cn->S1, 0.0f);
    for(int p=0;p<cn->c1H*cn->c1W;p++) for(int k=0;k<cn->K1;k++) cn->c1[(size_t)p*cn->K1+k]+=cn->b1[k];
    if(cn->use_in) in_apply(cn->c1,cn->xh1,cn->is1,cn->ga1,cn->be1,cn->K1,cn->c1H,cn->c1W,leak);
    else for(int i=0;i<cn->c1H*cn->c1W*cn->K1;i++){ float a=cn->c1[i]; cn->c1[i]=a>0?a:leak*a; }
    /* stage1 pool (max) */
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
    /* stage2: p1[H][W][K1] -> c2   (P2 = S2*S2*K1) */
    conv_im2col(cn->p1, cn->p1H, cn->p1W, cn->K1, cn->S2, cn->c2H, cn->c2W, COL2);
    conv_gemm_fwd(cn->c2, COL2, cn->w2, cn->c2H*cn->c2W, cn->K2, cn->S2*cn->S2*cn->K1, 0.0f);
    for(int p=0;p<cn->c2H*cn->c2W;p++) for(int k=0;k<cn->K2;k++) cn->c2[(size_t)p*cn->K2+k]+=cn->b2[k];
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
    /* stage3: p2[H][W][K2] -> c3 (no pool)   (P3 = S3*S3*K2).
     * When K3==0 (2-stage net) stage3 is absent; the final features are the
     * pooled stage2 output p2.  Output whichever exists. */
    if(cn->K3>0){
        if(cn->use_cbam) cbam_fwd((ConvNet3*)cn);
        conv_im2col(cn->p2, cn->p2H, cn->p2W, cn->K2, cn->S3, cn->c3H, cn->c3W, COL3);
        conv_gemm_fwd(cn->c3, COL3, cn->w3, cn->c3H*cn->c3W, cn->K3, cn->S3*cn->S3*cn->K2, 0.0f);
        for(int p=0;p<cn->c3H*cn->c3W;p++) for(int k=0;k<cn->K3;k++) cn->c3[(size_t)p*cn->K3+k]+=cn->b3[k];
        if(cn->use_in) in_apply(cn->c3,cn->xh3,cn->is3,cn->ga3,cn->be3,cn->K3,cn->c3H,cn->c3W,leak);
        else for(int i=0;i<cn->c3H*cn->c3W*cn->K3;i++){ float a=cn->c3[i]; cn->c3[i]=a>0?a:leak*a; }
        for(int i=0;i<cn->fdim;i++) out[i]=cn->c3[i];
    } else if(cn->K2>0){
        /* 2-stage: features = pooled stage2 output p2 (post-INORM c2) */
        for(int i=0;i<cn->fdim;i++) out[i]=cn->p2[i];
    } else {
        /* 1-stage: features = pooled stage1 output p1 */
        for(int i=0;i<cn->fdim;i++) out[i]=cn->p1[i];
    }
}
/* Global average pool: ft = [H][W][C] row-major (C inner), out = [C] (1 avg per channel). */
void convnet3_gap(const ConvNet3 *cn, const float *features, float *gap_out, int *H, int *W, int *C){
    int h=cn->c3H, w=cn->c3W, c=cn->K3;
    if(H)*H=h; if(W)*W=w; if(C)*C=c;
    int n=h*w;
    for(int k=0;k<c;k++){ float s=0; for(int p=0;p<n;p++) s+=features[(size_t)p*c+k]; gap_out[k]=s/(float)n; }
}

/* Instance-norm backward for one stage. On entry `dact` holds dL/d(activation)
 * (post-ReLU) for the whole map; `cbuf` holds the post-ReLU activation (for the
 * ReLU mask), `xh` the cached normalized pre-activations, `is` the cached
 * per-channel 1/std. Produces dL/d(pre-activation) IN PLACE into `dact`,
 * accumulates gamma/beta grads. This is the standard instance-norm gradient:
 *   dxhat = d * gamma
 *   dy    = (dxhat - mean(dxhat) - xhat*(mean(dxhat*xhat) - mean(dxhat)*mean(xhat)))
 *           / std
 * (with mean(xhat)=0 so the last cross-term collapses; we keep the general form.
 *  NB: the 1/N lives INSIDE the means -- no extra /N outside, or conv grads
 *  get scaled by 1/area and the conv freezes during INORM training.)
 * Gradient clipping on istd prevents blow-up on dead channels.
 */
static void in_backward(float *dact, const float *cbuf, const float *xh,
                        const float *is, const float *ga, float *dga, float *dbe,
                        int K, int H, int W, float leak){
    int N=H*W; int NK=N*K;
    /* Heap buffers sized for N*K (per position+channel), not just N.
     * The leaky-mask / dxhat must be indexed [p*K+k] consistently. */
    float *d = malloc(sizeof(float)*(size_t)NK);
    float *dxhat = malloc(sizeof(float)*(size_t)NK);
    if(!d || !dxhat){ if(d)free(d); if(dxhat)free(dxhat); for(int i=0;i<NK;i++) dact[i]=0; return; }
    for(int k=0;k<K;k++){
        /* 1) ReLU-mask the incoming gradient -> d */
        for(int p=0;p<N;p++){ int idx=p*K+k; float dv=dact[idx]; if(cbuf[idx]<=0.0f) dv*=leak; d[idx]=dv; }
        /* 2) dxhat = d * gamma ; accumulate gamma/beta grads */
        float sdx=0, sdxh=0, sxh=0;
        for(int p=0;p<N;p++){ int idx=p*K+k; float xv=xh[idx]; float dh=d[idx]*ga[k];
            dxhat[idx]=dh; dga[k]+=d[idx]*xv; dbe[k]+=d[idx]; sdx+=dh; sdxh+=dh*xv; sxh+=xv; }
        float mean_dx=sdx/N, mean_dxh=sdxh/N, mean_xh=sxh/N;
        /* 3) pre-activation gradient (clip istd to avoid blow-up on dead chans) */
        float istd=is[k]; if(istd>1e3f) istd=1e3f;
        for(int p=0;p<N;p++){ int idx=p*K+k; float xv=xh[idx];
            float dh=dxhat[idx];
            float dy=( (dh - mean_dx) - xv*(mean_dxh - mean_dx*mean_xh) ) * istd;
            dact[idx]=dy;
        }
    }
    free(d); free(dxhat);
}

/* ---------------- backward ---------------- */
void convnet3_backward(ConvNet3 *cn, const float *img, const float *feat, const float *dfeat){
    (void)feat;
    float leak = cn->leak;
    if(getenv("DUMP_DC")){ fprintf(stderr,"[BWD-ENTRY c3] %.4f %.4f %.4f %.4f %.4f %.4f\n", cn->c3[0],cn->c3[1],cn->c3[2],cn->c3[3],cn->c3[4],cn->c3[5]); }
    /* zero the per-call scratch accumulators (they are += below) */
    for(int i=0;i<cn->c1H*cn->c1W*cn->K1;i++) cn->dp1[i]=0.0f;
    for(int i=0;i<cn->c2H*cn->c2W*cn->K2;i++) cn->dp2[i]=0.0f;

    /* ---- last-stage gradient routing ----
     * The loss gradient arrives as dfeat (w.r.t. the network output = the
     * post-pool tensor of the LAST active stage). Route it to the right
     * buffer: stage3 (no pool) takes it directly as dc3; stage2/stage1
     * take it as their post-pool dp, which is then scattered back through
     * the maxpool into the pre-pool dc. */
    if(cn->K3>0){
        for(int i=0;i<cn->fdim;i++) cn->dc3[i]=dfeat[i];
        if(cn->use_in) in_backward(cn->dc3,cn->c3,cn->xh3,cn->is3,cn->ga3,cn->dga3,cn->dbe3,cn->K3,cn->c3H,cn->c3W,leak);
        else for(int i=0;i<cn->fdim;i++){ if(cn->c3[i]<=0.0f) cn->dc3[i]*=leak; }
    } else if(cn->K2>0){
        for(int i=0;i<cn->fdim;i++) cn->dp2[i]=dfeat[i];   /* last stage = stage2 (pooled) */
    } else {
        for(int i=0;i<cn->fdim;i++) cn->dp1[i]=dfeat[i];   /* last stage = stage1 (pooled) */
    }

    int c3H=cn->c3H, c3W=cn->c3W, K3=cn->K3, S3=cn->S3, K2=cn->K2;
    int p2H=cn->p2H, p2W=cn->p2W;
    /* stage3 conv backward -> dp2 (only when stage3 exists) */
    if(cn->K3>0){
        for(int k=0;k<K3;k++) for(int y=0;y<c3H;y++) for(int x=0;x<c3W;x++){
            float dc = cn->dc3[((size_t)y*c3W+x)*K3+k];
            cn->gb3[k]+=dc;
            const float *w = cn->w3 + (size_t)k*S3*S3*K2;
            for(int c=0;c<K2;c++) for(int dy=0;dy<S3;dy++) for(int dx=0;dx<S3;dx++){
                int iy=y+dy, ix=x+dx;
                cn->gw3[(size_t)k*S3*S3*K2 + (c*S3+dy)*S3+dx] += dc * cn->p2[((size_t)iy*p2W+ix)*K2+c];
                cn->dp2[((size_t)iy*p2W+ix)*K2+c] += dc * w[(c*S3+dy)*S3+dx];
            }
        }
        if(getenv("DUMP_DC")){
            FILE*f=fopen("/tmp/gw3_after3.txt","w");
            fprintf(f,"gw3_after_stage3");
            for(int i=0;i<cn->K3*cn->K2*cn->S3*cn->S3;i++) fprintf(f," %.6f",cn->gw3[i]);
            fprintf(f,"\n"); fclose(f);
        }
    }

    /* ---- stage2: pool backward (dp2 -> dc2) + INORM/leaky ---- */
    {
        int c2H=cn->c2H, c2W=cn->c2W, K2b=cn->K2;
        if(cn->use_cbam) cbam_bwd(cn, cn->dp2);
        if(cn->P2>1){
            for(int i=0;i<c2H*c2W*K2b;i++) cn->dc2[i]=0;
            for(int k=0;k<K2b;k++) for(int py=0;py<p2H;py++) for(int px=0;px<p2W;px++){
                int a=cn->am2[((size_t)py*p2W+px)*K2b+k];
                cn->dc2[(size_t)a*K2b+k]+=cn->dp2[((size_t)py*p2W+px)*K2b+k];
            }
        } else {
            for(int i=0;i<c2H*c2W*K2b;i++) cn->dc2[i]=cn->dp2[i];
        }
        if(cn->use_in) in_backward(cn->dc2,cn->c2,cn->xh2,cn->is2,cn->ga2,cn->dga2,cn->dbe2,cn->K2,cn->c2H,cn->c2W,leak);
        else for(int i=0;i<c2H*c2W*K2b;i++){ if(cn->c2[i]<=0.0f) cn->dc2[i]*=leak; }
    }

    int p1H=cn->p1H, p1W=cn->p1W, K1=cn->K1, S2=cn->S2;
    /* stage2 conv backward -> dp1 (only when stage2 exists) */
    if(cn->K2>0){
        int c2H=cn->c2H, c2W=cn->c2W, K2b=cn->K2;
        for(int k=0;k<K2b;k++) for(int y=0;y<c2H;y++) for(int x=0;x<c2W;x++){
            float dc = cn->dc2[((size_t)y*c2W+x)*K2b+k];
            cn->gb2[k]+=dc;
            const float *w = cn->w2 + (size_t)k*S2*S2*K1;
            for(int c=0;c<K1;c++) for(int dy=0;dy<S2;dy++) for(int dx=0;dx<S2;dx++){
                int iy=y+dy, ix=x+dx;
                cn->gw2[(size_t)k*S2*S2*K1 + (c*S2+dy)*S2+dx] += dc * cn->p1[((size_t)iy*p1W+ix)*K1+c];
                cn->dp1[((size_t)iy*p1W+ix)*K1+c] += dc * w[(c*S2+dy)*S2+dx];
            }
        }
    }

    /* ---- stage1: pool backward (dp1 -> dc1) + INORM/leaky ---- */
    {
        int c1H=cn->c1H, c1W=cn->c1W;
        if(cn->P1>1){
            for(int i=0;i<c1H*c1W*K1;i++) cn->dc1[i]=0;
            for(int k=0;k<K1;k++) for(int py=0;py<p1H;py++) for(int px=0;px<p1W;px++){
                int a=cn->am1[((size_t)py*p1W+px)*K1+k];
                cn->dc1[(size_t)a*K1+k]+=cn->dp1[((size_t)py*p1W+px)*K1+k];
            }
        } else {
            for(int i=0;i<c1H*c1W*K1;i++) cn->dc1[i]=cn->dp1[i];
        }
        if(cn->use_in) in_backward(cn->dc1,cn->c1,cn->xh1,cn->is1,cn->ga1,cn->dga1,cn->dbe1,cn->K1,cn->c1H,cn->c1W,leak);
        else for(int i=0;i<c1H*c1W*K1;i++){ if(cn->c1[i]<=0.0f) cn->dc1[i]*=leak; }
    }

    /* ---- stage1 weight/bias grad: bias by hand (sum), weight via gemm_dW
     *      (reuse the shared GEMM; C=gw1[N=K1][K=S1*S1] += dc1[M][K1] * col1[M][K]).
     *      col1 holds the forward's unfolded patches, so this is exactly the
     *      hand loop but vectorizable and index-safe. ---- */
    int S1=cn->S1;
    int M1 = cn->c1H*cn->c1W;
    for(int k=0;k<K1;k++) for(int y=0;y<cn->c1H;y++) for(int x=0;x<cn->c1W;x++)
        cn->gb1[k]+=cn->dc1[((size_t)y*cn->c1W+x)*K1+k];
    /* accumulate weight grad into gw1 (beta=1) so a per-thread gradbuf sums
     * the samples it processes within a batch; the trunk add_grad then
     * reduces the thread buffers and consumes (zeros) them. */
    conv_gemm_dW(cn->gw1, cn->dc1, cn->col1, M1, K1, S1*S1, 1.0f);
    /* TEMP DEBUG: dump intermediate gradient buffers for oracle diff */
    if(getenv("DUMP_DC")){
        FILE*f=fopen("/tmp/dc_c.txt","w");
        int n;
        #define WR(name,buf,n) do{ fprintf(f,"%s",name); for(int _i=0;_i<(n);_i++) fprintf(f," %.6f",(buf)[_i]); fprintf(f,"\n"); }while(0)
        WR("dc3",cn->dc3,cn->c3H*cn->c3W*cn->K3);
        WR("dp2",cn->dp2,cn->c2H*cn->c2W*cn->K2);
        WR("dc2",cn->dc2,cn->c2H*cn->c2W*cn->K2);
        WR("dp1",cn->dp1,cn->c1H*cn->c1W*cn->K1);
        WR("dc1",cn->dc1,cn->c1H*cn->c1W*cn->K1);
        WR("c3",cn->c3,cn->c3H*cn->c3W*cn->K3);
        WR("xh3",cn->xh3,cn->c3H*cn->c3W*cn->K3);
        WR("is3",cn->is3,cn->K3);
        WR("ga3",cn->ga3,cn->K3);
        WR("df",dfeat,cn->fdim);
        WR("col3",cn->col3,cn->c3H*cn->c3W*cn->S3*cn->S3*cn->K2);
        WR("col2",cn->col2,cn->c2H*cn->c2W*cn->S2*cn->S2*cn->K1);
        WR("col1",cn->col1,cn->c1H*cn->c1W*cn->S1*cn->S1);
        #undef WR
        fclose(f);
        fprintf(stderr,"[BWD-EXIT c3] %.4f %.4f %.4f %.4f %.4f %.4f\n", cn->c3[0],cn->c3[1],cn->c3[2],cn->c3[3],cn->c3[4],cn->c3[5]);
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
/* TEMP debug accessor: copy post-inorm c of a stage into out (stage 1/2/3). */
void convnet3_dbg_c(ConvNet3*cn,int stage,float*out){
    if(stage==1){ for(int i=0;i<cn->c1H*cn->c1W*cn->K1;i++) out[i]=cn->c1[i]; }
    else if(stage==2){ for(int i=0;i<cn->c2H*cn->c2W*cn->K2;i++) out[i]=cn->c2[i]; }
    else { for(int i=0;i<cn->c3H*cn->c3W*cn->K3;i++) out[i]=cn->c3[i]; }
}
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
