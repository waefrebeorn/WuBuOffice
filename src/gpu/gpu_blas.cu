/* gpu_blas.cu -- CUDA implementations of the CRNN trainer's heavy ops.
 *
 * Compiled with nvcc only when WITH_CUDA=ON. Provides C-callable entry points
 * (gpu_gemm, gpu_conv2d_fwd, gpu_available) that mirror host float buffers
 * into device memory, compute, and copy back. The host-side fallback (used
 * when this TU is not compiled) lives in gpu_blas_stub.c so the rest of the
 * project builds CPU-only without CUDA at all.
 *
 * Numerics: results match the CPU reference to within float rounding (validated
 * by tests/test_gpu_blas.c). We keep the kernels simple and correct first;
 * performance tuning (shared-memory tiling, cuBLAS) comes after correctness.
 */
#include "gpu_blas.h"
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* device: GEMM  C[MxN] = A[MxK] * B[KxN]  (row-major, naive tiled)    */
/* ------------------------------------------------------------------ */
__global__ void gemm_kernel(const float *A, const float *B, float *C,
                            int M, int K, int N) {
    const int T = 16;
    __shared__ float As[T][T], Bs[T][T];
    int row = blockIdx.y * T + threadIdx.y;
    int col = blockIdx.x * T + threadIdx.x;
    float acc = 0.0f;
    for (int t = 0; t < (K + T - 1) / T; t++) {
        if (row < M && t * T + threadIdx.x < K)
            As[threadIdx.y][threadIdx.x] = A[row * K + t * T + threadIdx.x];
        else As[threadIdx.y][threadIdx.x] = 0.0f;
        if (col < N && t * T + threadIdx.y < K)
            Bs[threadIdx.y][threadIdx.x] = B[(t * T + threadIdx.y) * N + col];
        else Bs[threadIdx.y][threadIdx.x] = 0.0f;
        __syncthreads();
        for (int k = 0; k < T; k++) acc += As[threadIdx.y][k] * Bs[k][threadIdx.x];
        __syncthreads();
    }
    if (row < M && col < N) C[row * N + col] = acc;
}

/* ------------------------------------------------------------------ */
/* device: conv forward (NHWC), out[h][w][oc] = act( bias[oc] +                      */
/*           sum_{ic,kh,kw} in[sh+h*S][sw+w*S][ic] * w[oc][ic][kh][kw] )  */
/* ------------------------------------------------------------------ */
__global__ void conv_kernel(const float *in, int Hin, int Win, int Cin,
                            const float *w, const float *bias,
                            int Cout, int Kh, int Kw, int S, int P, int act,
                            float *out, int Hout, int Wout) {
    int oc = blockIdx.z;
    int w2 = blockIdx.x;
    int h2 = blockIdx.y;
    if (oc >= Cout || w2 >= Wout || h2 >= Hout) return;
    float acc = bias[oc];
    int sh = h2 * S - P, sw = w2 * S - P;
    for (int ic = 0; ic < Cin; ic++) {
        for (int kh = 0; kh < Kh; kh++) {
            int y = sh + kh;
            if (y < 0 || y >= Hin) continue;
            for (int kw = 0; kw < Kw; kw++) {
                int x = sw + kw;
                if (x < 0 || x >= Win) continue;
                float v = in[(size_t)y * Win * Cin + (size_t)x * Cin + ic];
                float wt = w[(((size_t)oc * Cin + ic) * Kh + kh) * Kw + kw];
                acc += v * wt;
            }
        }
    }
    if (act == 1) acc = acc > 0.0f ? acc : 0.0f;  /* ReLU */
    out[((size_t)h2 * Wout + w2) * Cout + oc] = acc;
}

static int g_dev_ok = -1;  /* -1 = unknown */

int gpu_available(void) {
    if (g_dev_ok >= 0) return g_dev_ok;
    int n = 0;
    cudaError_t e = cudaGetDeviceCount(&n);
    g_dev_ok = (e == cudaSuccess && n > 0) ? 1 : 0;
    return g_dev_ok;
}

void gpu_gemm(const float *A, const float *B, float *C, int M, int K, int N) {
    if (!gpu_available()) {  /* CPU fallback */
        for (int i = 0; i < M; i++)
            for (int j = 0; j < N; j++) {
                float s = 0.0f;
                for (int k = 0; k < K; k++) s += A[(size_t)i * K + k] * B[(size_t)k * N + j];
                C[(size_t)i * N + j] = s;
            }
        return;
    }
    float *dA, *dB, *dC;
    size_t sa = (size_t)M * K * sizeof(float);
    size_t sb = (size_t)K * N * sizeof(float);
    size_t sc = (size_t)M * N * sizeof(float);
    cudaMalloc(&dA, sa); cudaMalloc(&dB, sb); cudaMalloc(&dC, sc);
    cudaMemcpy(dA, A, sa, cudaMemcpyHostToDevice);
    cudaMemcpy(dB, B, sb, cudaMemcpyHostToDevice);
    const int T = 16;
    dim3 block(T, T);
    dim3 grid((N + T - 1) / T, (M + T - 1) / T);
    gemm_kernel<<<grid, block>>>(dA, dB, dC, M, K, N);
    cudaMemcpy(C, dC, sc, cudaMemcpyDeviceToHost);
    cudaFree(dA); cudaFree(dB); cudaFree(dC);
}

/* Transposed-A GEMM: C[MxN] = A^T[KxM] * B[KxN], where A is row-major [MxK].
 * Equivalent to: for i,j: C[i*N+j] = sum_k A[k*M+i] * B[k*N+j]. */
__global__ void gemmT_kernel(const float *A, const float *B, float *C,
                             int M, int K, int N) {
    const int T = 16;
    __shared__ float As[T][T], Bs[T][T];
    int row = blockIdx.y * T + threadIdx.y;   /* i in [0,M) */
    int col = blockIdx.x * T + threadIdx.x;   /* j in [0,N) */
    float acc = 0.0f;
    for (int t = 0; t < (K + T - 1) / T; t++) {
        int ka = t * T + threadIdx.x;  /* k index for A col */
        int kb = t * T + threadIdx.y;  /* k index for B row */
        if (row < M && ka < K) As[threadIdx.y][threadIdx.x] = A[(size_t)ka * M + row];
        else As[threadIdx.y][threadIdx.x] = 0.0f;
        if (col < N && kb < K) Bs[threadIdx.y][threadIdx.x] = B[(size_t)kb * N + col];
        else Bs[threadIdx.y][threadIdx.x] = 0.0f;
        __syncthreads();
        for (int k = 0; k < T; k++) acc += As[threadIdx.y][k] * Bs[k][threadIdx.x];
        __syncthreads();
    }
    if (row < M && col < N) C[(size_t)row * N + col] = acc;
}

void gpu_gemmT(const float *A, const float *B, float *C, int M, int K, int N) {
    if (!gpu_available()) {  /* CPU fallback */
        for (int i = 0; i < M; i++)
            for (int j = 0; j < N; j++) {
                float s = 0.0f;
                for (int k = 0; k < K; k++) s += A[(size_t)k * M + i] * B[(size_t)k * N + j];
                C[(size_t)i * N + j] = s;
            }
        return;
    }
    float *dA, *dB, *dC;
    size_t sa = (size_t)K * M * sizeof(float);
    size_t sb = (size_t)K * N * sizeof(float);
    size_t sc = (size_t)M * N * sizeof(float);
    cudaMalloc(&dA, sa); cudaMalloc(&dB, sb); cudaMalloc(&dC, sc);
    cudaMemcpy(dA, A, sa, cudaMemcpyHostToDevice);
    cudaMemcpy(dB, B, sb, cudaMemcpyHostToDevice);
    const int T = 16;
    dim3 block(T, T);
    dim3 grid((N + T - 1) / T, (M + T - 1) / T);
    gemmT_kernel<<<grid, block>>>(dA, dB, dC, M, K, N);
    cudaMemcpy(C, dC, sc, cudaMemcpyDeviceToHost);
    cudaFree(dA); cudaFree(dB); cudaFree(dC);
}

/* B-transposed GEMM: C[MxN] = A[MxK] * B^T[NxK]. Kernel indexes B as B[n*K+k]. */
__global__ void gemmNT_kernel(const float *A, const float *B, float *C,
                              int M, int K, int N) {
    const int T = 16;
    __shared__ float As[T][T], Bs[T][T];
    int row = blockIdx.y * T + threadIdx.y;   /* i in [0,M) */
    int col = blockIdx.x * T + threadIdx.x;   /* j in [0,N) */
    float acc = 0.0f;
    for (int t = 0; t < (K + T - 1) / T; t++) {
        int ka = t * T + threadIdx.x;  /* k for A */
        int kb = t * T + threadIdx.y;  /* k for B row */
        if (row < M && ka < K) As[threadIdx.y][threadIdx.x] = A[(size_t)row * K + ka];
        else As[threadIdx.y][threadIdx.x] = 0.0f;
        if (col < N && kb < K) Bs[threadIdx.y][threadIdx.x] = B[(size_t)col * K + kb];
        else Bs[threadIdx.y][threadIdx.x] = 0.0f;
        __syncthreads();
        for (int k = 0; k < T; k++) acc += As[threadIdx.y][k] * Bs[k][threadIdx.x];
        __syncthreads();
    }
    if (row < M && col < N) C[(size_t)row * N + col] = acc;
}

void gpu_gemmNT(const float *A, const float *B, float *C, int M, int K, int N) {
    if (!gpu_available()) {  /* CPU fallback */
        for (int i = 0; i < M; i++)
            for (int j = 0; j < N; j++) {
                float s = 0.0f;
                for (int k = 0; k < K; k++) s += A[(size_t)i * K + k] * B[(size_t)j * K + k];
                C[(size_t)i * N + j] = s;
            }
        return;
    }
    float *dA, *dB, *dC;
    size_t sa = (size_t)M * K * sizeof(float);
    size_t sb = (size_t)N * K * sizeof(float);
    size_t sc = (size_t)M * N * sizeof(float);
    cudaMalloc(&dA, sa); cudaMalloc(&dB, sb); cudaMalloc(&dC, sc);
    cudaMemcpy(dA, A, sa, cudaMemcpyHostToDevice);
    cudaMemcpy(dB, B, sb, cudaMemcpyHostToDevice);
    const int T = 16;
    dim3 block(T, T);
    dim3 grid((N + T - 1) / T, (M + T - 1) / T);
    gemmNT_kernel<<<grid, block>>>(dA, dB, dC, M, K, N);
    cudaMemcpy(C, dC, sc, cudaMemcpyDeviceToHost);
    cudaFree(dA); cudaFree(dB); cudaFree(dC);
}

void gpu_conv2d_fwd(const float *in, int Hin, int Win, int Cin,
                    const float *w, const float *bias,
                    int Cout, int Kh, int Kw, int S, int P, int act,
                    float *out, int *Hout, int *Wout) {
    int hout = (Hin + 2 * P - Kh) / S + 1;
    int wout = (Win + 2 * P - Kw) / S + 1;
    *Hout = hout; *Wout = wout;
    if (!gpu_available()) {  /* CPU fallback */
        for (int oc = 0; oc < Cout; oc++)
            for (int h = 0; h < hout; h++)
                for (int ww = 0; ww < wout; ww++) {
                    float acc = bias[oc];
                    int sh = h * S - P, sw = ww * S - P;
                    for (int ic = 0; ic < Cin; ic++)
                        for (int kh = 0; kh < Kh; kh++) {
                            int y = sh + kh; if (y < 0 || y >= Hin) continue;
                            for (int kw = 0; kw < Kw; kw++) {
                                int x = sw + kw; if (x < 0 || x >= Win) continue;
                                float v = in[(size_t)y * Win * Cin + (size_t)x * Cin + ic];
                                float wt = w[(((size_t)oc * Cin + ic) * Kh + kh) * Kw + kw];
                                acc += v * wt;
                            }
                        }
                    if (act == 1) acc = acc > 0.0f ? acc : 0.0f;
                    out[((size_t)h * wout + ww) * Cout + oc] = acc;
                }
        return;
    }
    float *din, *dw, *db, *dout;
    size_t sin = (size_t)Hin * Win * Cin * sizeof(float);
    size_t sw = (size_t)Cout * Cin * Kh * Kw * sizeof(float);
    size_t sb = (size_t)Cout * sizeof(float);
    size_t sout = (size_t)hout * wout * Cout * sizeof(float);
    cudaMalloc(&din, sin); cudaMalloc(&dw, sw); cudaMalloc(&db, sb); cudaMalloc(&dout, sout);
    cudaMemcpy(din, in, sin, cudaMemcpyHostToDevice);
    cudaMemcpy(dw, w, sw, cudaMemcpyHostToDevice);
    cudaMemcpy(db, bias, sb, cudaMemcpyHostToDevice);
    dim3 grid(wout, hout, Cout);
    conv_kernel<<<grid, 1>>>(din, Hin, Win, Cin, dw, db, Cout, Kh, Kw, S, P, act, dout, hout, wout);
    cudaMemcpy(out, dout, sout, cudaMemcpyDeviceToHost);
    cudaFree(din); cudaFree(dw); cudaFree(db); cudaFree(dout);
}
