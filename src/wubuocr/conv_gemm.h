/* conv_gemm.h -- im2col/col2im + the three GEMM kernels backing the conv engine.
 *
 * Pure, struct-free, dependency-free C11. Extracted from convnet3.c so the
 * compute core compiles and unit-tests independently of the ConvNet3 struct.
 *
 * The conv "out[h,w,k] = b[k] + sum_{c,dy,dx} w[k,c,dy,dx]*in[h+dy,w+dx,c]"
 * becomes, with the spatial patches unfolded into rows of a column matrix,
 *   OUT = COL @ W^T      (one cache-friendly GEMM the compiler auto-vectorizes)
 * which removes the gather-memory access pattern that dominated wall-time.
 *
 * The GEMM kernels are parallelized with OpenMP (one team over the output
 * rows / accumulation groups) and inner-loop SIMD, so they use all cores and
 * the vector unit on the fast (WITH_PERF) build.
 */
#ifndef WUBUOCR_CONV_GEMM_H
#define WUBUOCR_CONV_GEMM_H

#include <stddef.h>

/* build im2col: src is [H][W][K_in] (row-major H*W*K_in), writes
 * col[Oh*Ow][S*S*K_in]. Pads with 0 outside the valid region (valid-conv, P=0). */
void conv_im2col(const float *src, int H, int W, int K_in, int S, int Oh, int Ow, float *col);

/* col2im: scatter-add dCOL[Oh*Ow][S*S*K_in] gradients back into dsrc[H][W][K_in]. */
void conv_col2im(const float *dcol, int H, int W, int K_in, int S, int Oh, int Ow, float *dsrc);

/* Three GEMM contracts, all row-major, contiguous inner loop (auto-vectorized):
 *   gemm_fwd  C[M][N] += A[M][K] * B[N][K]   (B is [N][K], stride K)
 *             -> forward conv:  C[out][k_out] = patch[m] * W[k_out][patch]
 *   gemm_dW   C[N][K] += A[M][N] * B[M][K]   (both M-leading)
 *             -> weight grad:  dW[k_out][p] = sum_m dC[m][k_out] * patch[m][p]
 *   gemm_dX   C[M][K] += A[M][N] * B[N][K]   (standard C=A*B)
 *             -> input grad:  dX[m][p] = sum_k dC[m][k] * W[k][p]            */
void conv_gemm_fwd(float *C, const float *A, const float *B, int M, int N, int K, float beta);
void conv_gemm_dW(float *C, const float *A, const float *B, int M, int N, int K, float beta);
void conv_gemm_dX(float *C, const float *A, const float *B, int M, int N, int K, float beta);

#endif /* WUBUOCR_CONV_GEMM_H */
