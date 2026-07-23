/* mlp.h -- lightweight feed-forward classifier (dependency-free C11).
 *
 * Opaque MLP: z -> h1(leaky relu) -> h2(leaky relu) -> scores -> softmax.
 * Leaky ReLU (slope MLP_LEAK) is used so hidden units can never fully die; a
 * dead hidden layer was the root cause of a prior collapse-to-one-class bug.
 *
 * The module is deliberately FREE of any optimizer dependency: training drives
 * it through mlp_train_step() (which fills internal gradient buffers) and then
 * queries parameter groups via mlp_layer() to apply an update with whatever
 * optimizer the caller chooses (wubu Riemannian SGD, plain SGD, ...).
 */
#ifndef WUBUOCR_MLP_H
#define WUBUOCR_MLP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MLP_LEAK 0.1f
#define MLP_NCLASS_MAX 26

typedef struct MLP MLP;

/* Create an MLP with the given topology. Seed drives He-style init. */
MLP *mlp_create(int din, int h1, int h2, int K, uint32_t seed);
void mlp_destroy(MLP *m);

int  mlp_din(const MLP *m);
int  mlp_h1(const MLP *m);
int  mlp_h2(const MLP *m);
int  mlp_K(const MLP *m);

/* Forward pass; writes K scores (pre-softmax logits) into out_scores[].
 * Also caches the activations needed for the next mlp_backward. */
void mlp_forward(const MLP *m, const float *z, float *out_scores);

/* Zero the internal gradient buffers (call once before a batch). */
void mlp_zero_grad(MLP *m);

/* Accumulate cross-entropy gradients for ONE sample (target in [0,K)).
 * REQUIRES a mlp_forward(m, z, ...) to have been called immediately before
 * (for this same z) so the hidden activations h1act/h2act are cached.
 * Calling backward without a preceding forward yields ALL-ZERO gradients
 * (stale cached activations), which silently poisons training -- always
 * forward-then-backward, exactly as emnist_train_conv3.c does per sample. */
void mlp_backward(MLP *m, const float *z, int target);

/* Label-smoothed variant (smooth in [0,1)): the one-hot target is blended
 * with uniform, dscore[c] = softmax[c] - (c==target ? 1-smooth : smooth/(K-1)).
 * 2026-standard regularization: smooth=0.1 typically. Reduces overconfidence
 * and improves generalization at ~no cost. smooth<=0 falls back to hard target. */
void mlp_backward_smooth(MLP *m, const float *z, int target, float smooth);

/* Convenience: zero gradients then accumulate ONE sample's gradient. */
void mlp_train_step(MLP *m, const float *z, int target);

/* Convenience: zero gradients then accumulate ONE sample's gradient (smoothed). */
void mlp_train_step_smooth(MLP *m, const float *z, int target, float smooth);

/* Gradient of the loss w.r.t. the input z (length din), given that
 * mlp_backward(m, z, target) has just been called for the last sample.
 * Writes dL/dz into out_dz[]. This is the bridge that lets an upstream
 * module (e.g. ConvNet) backprop through the MLP: chain dz into the
 * conv net's convnet_backward(img, features, dz). */
void mlp_input_grad(MLP *m, const float *z, float *out_dz);

/* Scale all gradient buffers by s (used to average a batch: call with
 * 1/batch_size after accumulating batch_size samples). */
void mlp_scale_grad(MLP *m, float s);

/* Accumulate src gradient buffers into dst (thread-private -> shared reduce). */
void mlp_add_grad(MLP *dst, const MLP *src);

/* Grad-only replica: aliases m's weights, allocates fresh caches+grads. */
MLP *mlp_gradbuf(const MLP *m);

/* Parameter groups, for optimizer-driven updates. Group i exposes a live
 * pointer to its weights (param) and to the gradient buffer (grad) of length
 * n. The caller applies its update in place: param[k] -= lr * grad[k] (or via
 * a Riemannian optimizer). 6 groups: W1,b1,W2,b2,W3,b3. */
typedef struct { float *param; float *grad; int n; } MLPLayer;
int    mlp_layer_count(const MLP *m);
MLPLayer mlp_layer(MLP *m, int idx);

/* Plain SGD update across all groups: param -= lr * grad. */
void mlp_apply_plain(MLP *m, float lr);

/* Save / load weights + standardization stats. The stats (zmean,zstd) let the
 * inference side standardize identically. Returns 0 on success. */
int mlp_save(const MLP *m, const float *zmean, const float *zstd, int dim,
             const char *path);
/* On load, *dim is set to the stored feature dim; zmean/zstd must point to
 * arrays of at least that size (caller allocates). Returns 0 on success. */
int mlp_load(const char *path, MLP **out,
             float *zmean, float *zstd, int *dim);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_MLP_H */
