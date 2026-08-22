/* convnet3_internal.h -- layout of the opaque ConvNet3 struct, shared by
 * the convnet core (convnet3.c) and the CBAM attention module
 * (convnet3_cbam.c). Internal to src/wubuocr. */
#ifndef CONVNET3_INTERNAL_H
#define CONVNET3_INTERNAL_H

#include "convnet3.h"
#include <stdint.h>

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

/* cbam module */
void cbam_fwd(ConvNet3 *cn);
void cbam_bwd(ConvNet3 *cn, float *dp2);

#endif /* CONVNET3_INTERNAL_H */
