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

ConvNet3 *convnet3_create(const ConvConfig3 *cfg);
void       convnet3_destroy(ConvNet3 *cn);
ConvNet3 *convnet3_gradbuf(const ConvNet3 *cn);  /* grad-only replica, aliases cn's weights */
int        convnet3_dim(const ConvNet3 *cn);          /* feature length */
void       convnet3_zero_grad(ConvNet3 *cn);
void       convnet3_scale_grad(ConvNet3 *cn, float s);
void       convnet3_add_grad(ConvNet3 *dst, const ConvNet3 *src);  /* reduce grad buffers */
void       convnet3_forward(const ConvNet3 *cn, const float *img, float *out_features);
void       convnet3_backward(ConvNet3 *cn, const float *img, const float *feat, const float *dfeat);

/* per-layer view for the trainer's optimizer (weights then biases, s1..s3) */
typedef struct { float *param; float *grad; int n; } ConvLayer3;
int        convnet3_layer_count(const ConvNet3 *cn);   /* 6 */
ConvLayer3 convnet3_layer(ConvNet3 *cn, int idx);

int        convnet3_save(const ConvNet3 *cn, const char *path);
int        convnet3_load(const char *path, ConvNet3 **out, ConvConfig3 *cfg);

#endif
