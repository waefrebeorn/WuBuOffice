/* gpu_blas.h -- C11 API for the CUDA BLAS/conv primitives used by the CRNN
 * trainer. When built WITHOUT CUDA (WITH_CUDA=OFF) these are no-ops/stubs so
 * the rest of the codebase compiles unchanged; the real implementations live
 * in gpu_blas.cu.
 *
 * Design contract: weights and activations stay in the trainer's existing
 * float buffers on the host. The GPU kernels mirror those buffers into device
 * memory, compute, and copy results back. The on-disk .crnn format is
 * untouched, so the C inference path is unchanged.
 */
#ifndef WUBU_GPU_BLAS_H
#define WUBU_GPU_BLAS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 if a CUDA device is available, 0 otherwise. */
int gpu_available(void);

/* Matrix multiply C[MxN] = A[MxK] * B[KxN] (row-major). All float.
 * Falls back to a CPU implementation when CUDA is unavailable. */
void gpu_gemm(const float *A, const float *B, float *C,
              int M, int K, int N);

/* 2D convolution forward, NHWC-ish layout used by convnet3 stage math:
 *   in:  [Hin*Win*Cin], out: [Hout*Wout*Cout]
 *   weights: [Cout*Cin*Kh*Kw] (Cout outer), bias: [Cout]
 *   stride S, pad P, activation: 0=none, 1=ReLU.
 * Mirrors convnet3's per-stage conv (no pooling here; caller pools). */
void gpu_conv2d_fwd(const float *in, int Hin, int Win, int Cin,
                    const float *w, const float *bias,
                    int Cout, int Kh, int Kw, int S, int P, int act,
                    float *out, int *Hout, int *Wout);

/* Transposed-A GEMM: C[MxN] = A^T[KxM] * B[KxN] (A stored row-major [MxK]).
 * Used for gradient accumulation (dW = dY^T * X). Falls back to CPU. */
void gpu_gemmT(const float *A, const float *B, float *C, int M, int K, int N);

/* B-transposed GEMM: C[MxN] = A[MxK] * B^T[NxK] (B stored row-major [NxK]).
 * Used for gate preactivations (az = X * Wz^T) and their backward. Falls back to CPU. */
void gpu_gemmNT(const float *A, const float *B, float *C, int M, int K, int N);

#ifdef __cplusplus
}
#endif
#endif
