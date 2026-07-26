/* gpu_blas_stub.c -- CPU-only fallback for the GPU BLAS API.
 * Compiled instead of gpu_blas.cu when WITH_CUDA=OFF (or no CUDA toolkit).
 * Keeps the same C API so callers don't branch on build config. */
#include "gpu_blas.h"

int gpu_available(void) { return 0; }

void gpu_gemm(const float *A, const float *B, float *C, int M, int K, int N) {
    for (int i = 0; i < M; i++)
        for (int j = 0; j < N; j++) {
            float s = 0.0f;
            for (int k = 0; k < K; k++) s += A[(size_t)i * K + k] * B[(size_t)k * N + j];
            C[(size_t)i * N + j] = s;
        }
}

void gpu_conv2d_fwd(const float *in, int Hin, int Win, int Cin,
                    const float *w, const float *bias,
                    int Cout, int Kh, int Kw, int S, int P, int act,
                    float *out, int *Hout, int *Wout) {
    int hout = (Hin + 2 * P - Kh) / S + 1;
    int wout = (Win + 2 * P - Kw) / S + 1;
    *Hout = hout; *Wout = wout;
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
}

void gpu_gemmT(const float *A, const float *B, float *C, int M, int K, int N) {
    for (int i = 0; i < M; i++)
        for (int j = 0; j < N; j++) {
            float s = 0.0f;
            for (int k = 0; k < K; k++) s += A[(size_t)k * M + i] * B[(size_t)k * N + j];
            C[(size_t)i * N + j] = s;
        }
}

void gpu_gemmNT(const float *A, const float *B, float *C, int M, int K, int N) {
    for (int i = 0; i < M; i++)
        for (int j = 0; j < N; j++) {
            float s = 0.0f;
            for (int k = 0; k < K; k++) s += A[(size_t)i * K + k] * B[(size_t)j * K + k];
            C[(size_t)i * N + j] = s;
        }
}
