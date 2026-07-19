/* convnet.h -- ultra-light feed-forward conv front-end (dependency-free C11).
 *
 * Purpose: turn a 28x28 glyph bitmap into a small, spatially-grounded
 * feature vector that a plain MLP can separate. A 2-stage conv + maxpool
 * stack (LeNet-light) is enough to push EMNIST Letters past 96% where a
 * flat MLP on raw/zoning pixels plateaus (~5%, random).
 *
 * Opaque struct. The caller trains it end-to-end together with an MLP:
 *   convnet_forward(cn, img)  -> features f (dim = convnet_dim)
 *   mlp_forward(m, f)           -> logits
 *   mlp_backward(m, f, tgt)      -> fills m grads; also produces dL/df
 *   convnet_input_grad(m, df)      -> df = dL/df (from the MLP)
 *   convnet_backward(cn, img, f, df) -> fills cn grads
 * Then apply an update to both m's and cn's parameter groups.
 *
 * Tuned for SINGLE-CORE scalar CPUs (Q6600-class, ~2011): plain nested
 * loops, float, no SIMD/SSE assumptions. ~200k MACs per 28x28 image.
 */
#ifndef WUBUOCR_CONVNET_H
#define WUBUOCR_CONVNET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lightweight, fixed 2-conv topology. Set K2 = 0 to disable stage 2
 * (then the feature dim is K1 * (outH1/P1) * (outW1/P1)). */
typedef struct {
    int inH, inW;          /* input bitmap size (28x28 for EMNIST) */
    int K1, S1, P1;        /* stage1: filters, kernel, maxpool */
    int K2, S2, P2;        /* stage2: 0 disables; else filters,kernel,pool */
} ConvConfig;

/* Default "ultra light" config: 28x28 -> 6@5x5 ->pool2 ->
 * 16@5x5 ->pool2 -> 256 features. ~200k MACs/image. */
static const ConvConfig CONV_LIGHT = { 28, 28, 6, 5, 2, 16, 5, 2 };

typedef struct ConvNet ConvNet;

ConvNet *convnet_create(const ConvConfig *cfg);
void      convnet_destroy(ConvNet *cn);

int  convnet_dim(const ConvNet *cn);   /* feature-vector length D */

/* Forward: img is inH*inW floats (row-major), ink = high (e.g. raw/255).
 * Writes D features into out_features[]. Caches activations for backward. */
void convnet_forward(const ConvNet *cn, const float *img, float *out_features);

/* Zero / scale the internal gradient buffers (call around a mini-batch). */
void convnet_zero_grad(ConvNet *cn);
void convnet_scale_grad(ConvNet *cn, float s);

/* Backward given df = dL/dfeatures (length D, from the downstream MLP).
 * img must be the SAME bitmap passed to the matching convnet_forward. */
void convnet_backward(ConvNet *cn, const float *img, const float *features,
                     const float *dfeatures);

/* Plain SGD update across all groups: param -= lr * grad. */
void convnet_apply_plain(ConvNet *cn, float lr);

/* Parameter groups for optimizer-driven updates (mirrors mlp_layer):
 * groups are [W1, b1, (W2, b2 if K2>0)]. Caller applies its
 * optimizer of choice (wubu Riemannian SGD, plain SGD, ...). */
typedef struct { float *param; float *grad; int n; } ConvLayer;
int       convnet_layer_count(const ConvNet *cn);
ConvLayer convnet_layer(ConvNet *cn, int idx);

/* Save / load conv weights. Returns 0 on success. */
int convnet_save(const ConvNet *cn, const char *path);
int convnet_load(const char *path, ConvNet **out, ConvConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_CONVNET_H */
