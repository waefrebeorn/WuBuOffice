/* convnet3.h -- 3-stage ultra-light conv front-end (medium capacity).
 * Same opaque-struct contract as convnet.h but supports three conv
 * stages. C11, no deps, single-core scalar. Designed for Q6600-class.
 *
 * Geometry (28x28 input, MED arch):
 *   s1: K=16 S=5 P=2 -> c1=24^2x16, p1=12^2x16
 *   s2: K=32 S=5 P=2 -> c2= 8^2x32, p2= 4^2x32
 *   s3: K=64 S=3 P=1 (no final pool) -> c3=2^2x64, features=256
 */
#ifndef WUBUOCR_CONVNET3_H
#define WUBUOCR_CONVNET3_H

#include <stdint.h>

typedef struct ConvNet3 ConvNet3;   /* opaque */

/* arch: {inH,inW, K1,S1,P1, K2,S2,P2, K3,S3,P3} */
typedef struct { int inH,inW, K1,S1,P1, K2,S2,P2, K3,S3,P3; } ConvConfig3;

extern ConvConfig3 CONV_MED;
extern ConvConfig3 CONV_MED_PAD;  /* 32×32 input-padded version of MED: 16→32→64, 3×3×64=576 feats */
extern ConvConfig3 CONV_TINY;
extern ConvConfig3 CONV_WIDE;  /* 3×3 kernels, 2-stage, 32→64→128, keeps spatial dims */
extern ConvConfig3 CONV_XL;    /* 28×28, 3×3 kernels, 64→128→256→2304 feats */
extern ConvConfig3 CONV_2STAGE; /* 28×28, 2-stage 32→64, gradcheck-verified */
extern ConvConfig3 CONV_BIGMAP; /* 32×32, stride-1 convs, single pool -> 7×7×128 big map */

ConvNet3 *convnet3_create(const ConvConfig3 *cfg);
void       convnet3_destroy(ConvNet3 *cn);
ConvNet3 *convnet3_gradbuf(const ConvNet3 *cn);  /* grad-only replica, aliases cn's weights */
int        convnet3_dim(const ConvNet3 *cn);          /* feature length */
void       convnet3_zero_grad(ConvNet3 *cn);
void       convnet3_scale_grad(ConvNet3 *cn, float s);
void       convnet3_add_grad(ConvNet3 *dst, const ConvNet3 *src);  /* reduce grad buffers */
void       convnet3_forward(const ConvNet3 *cn, const float *img, float *out_features);
void       convnet3_backward(ConvNet3 *cn, const float *img, const float *feat, const float *dfeat);
void       convnet3_gap(const ConvNet3 *cn, const float *features, float *gap_out, int *H, int *W, int *C);  /* global avg pool, fills H,W,C */

/* per-layer view for the trainer's optimizer (weights then biases, s1..s3) */
typedef struct { float *param; float *grad; int n; } ConvLayer3;
int        convnet3_layer_count(const ConvNet3 *cn);   /* 6 */
ConvLayer3 convnet3_layer(ConvNet3 *cn, int idx);
void       convnet3_dbg_c(ConvNet3 *cn, int stage, float *out); /* TEMP debug */

int        convnet3_save(const ConvNet3 *cn, const char *path);
int        convnet3_load(const char *path, ConvNet3 **out, ConvConfig3 *cfg);

/* CBAM attention weights (only present if CBAM was enabled at create).
 * Total params = r*2*K + r + K*r + K + 2, packed/unpacked contiguously:
 *   [ca_w1(r*2*K), ca_b1(r), ca_w2(K*r), ca_b2(K), sa_w(2)] */
int   convnet3_cbam_size(const ConvNet3 *cn);
void  convnet3_cbam_pack(const ConvNet3 *cn, float *out);
void  convnet3_cbam_unpack(ConvNet3 *cn, const float *in);
void  convnet3_enable_cbam(ConvNet3 *cn);   /* turn CBAM on (buffers already allocated) */
void  convnet3_sgd_cbam(ConvNet3 *cn, float lr);  /* plain-SGD step on CBAM weights */
int   convnet3_cbam_enabled(const ConvNet3 *cn);   /* 1 if CBAM active */
/* Instance norm and leaky ReLU control (set before first forward) */
int   convnet3_enable_inorm(ConvNet3 *cn);   /* turn instance norm on (buffers already allocated) */
void  convnet3_set_leak(ConvNet3 *cn, float leak); /* set ReLU leak slope */
int   convnet3_inorm_enabled(const ConvNet3 *cn);  /* check if instance norm is enabled */
#endif
