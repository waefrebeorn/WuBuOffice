/* conv_gemm.c -- im2col/col2im + GEMM kernels for the conv engine.
 * See conv_gemm.h. C11, no deps. OpenMP-parallel + SIMD inner loops.
 */
#include "conv_gemm.h"
#include <math.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/* build im2col: src is [H][W][K_in] (row-major H*W*K_in), writes col[Oh*Ow][S*S*K_in].
 * Pads with 0 outside the valid region (matches the original valid-conv, P=0). */
void conv_im2col(const float *src, int H, int W, int K_in, int S, int Oh, int Ow, float *col){
    int pp = S*S*K_in;
    for(int oh=0; oh<Oh; oh++){
        for(int ow=0; ow<Ow; ow++){
            float *row = col + ((size_t)oh*Ow+ow)*pp;
            int sp=0;
            for(int c=0; c<K_in; c++){
                for(int dy=0; dy<S; dy++){
                    int iy = oh+dy;
                    for(int dx=0; dx<S; dx++){
                        int ix = ow+dx;
                        if(iy>=0 && iy<H && ix>=0 && ix<W)
                            row[sp] = src[((size_t)iy*W+ix)*K_in + c];
                        else
                            row[sp] = 0.0f;
                        sp++;
                    }
                }
            }
        }
    }
}

/* col2im: scatter-add dCOL[Oh*Ow][S*S*K_in] gradients back into dsrc[H][W][K_in]. */
void conv_col2im(const float *dcol, int H, int W, int K_in, int S, int Oh, int Ow, float *dsrc){
    int pp = S*S*K_in;
    for(int oh=0; oh<Oh; oh++){
        for(int ow=0; ow<Ow; ow++){
            const float *row = dcol + ((size_t)oh*Ow+ow)*pp;
            int sp=0;
            for(int c=0; c<K_in; c++){
                for(int dy=0; dy<S; dy++){
                    int iy = oh+dy;
                    float *dsrcc = dsrc + (size_t)c;
                    for(int dx=0; dx<S; dx++){
                        int ix = ow+dx;
                        if(iy>=0 && iy<H && ix>=0 && ix<W)
                            dsrcc[((size_t)iy*W+ix)*K_in] += row[sp];
                        sp++;
                    }
                }
            }
        }
    }
}

/* Three GEMM contracts, all row-major, contiguous inner loop (auto-vectorized). */

void conv_gemm_fwd(float *C, const float *A, const float *B, int M, int N, int K, float beta){
    #pragma omp parallel for schedule(static)
    for(int i=0;i<M;i++){
        float *Cr = C + (size_t)i*N;
        const float *Ar = A + (size_t)i*K;
        #pragma omp simd
        for(int j=0;j<N;j++){
            const float *Br = B + (size_t)j*K;   /* B[N][K]: row j, stride K */
            float s = (beta==0.0f) ? 0.0f : Cr[j]*beta;
            for(int kk=0;kk<K;kk++) s += Ar[kk]*Br[kk];
            Cr[j] = s;
        }
    }
}

void conv_gemm_dW(float *C, const float *A, const float *B, int M, int N, int K, float beta){
    #pragma omp parallel for schedule(static)
    for(int k=0;k<N;k++){
        float *Cr = C + (size_t)k*K;
        if(beta==0.0f) for(int p=0;p<K;p++) Cr[p]=0.0f;
        else            for(int p=0;p<K;p++) Cr[p]*=beta;
        for(int m=0;m<M;m++){
            float a = A[(size_t)m*N + k];
            if(a==0.0f) continue;
            const float *Br = B + (size_t)m*K;
            #pragma omp simd
            for(int p=0;p<K;p++) Cr[p] += a*Br[p];
        }
    }
}

void conv_gemm_dX(float *C, const float *A, const float *B, int M, int N, int K, float beta){
    #pragma omp parallel for schedule(static)
    for(int i=0;i<M;i++){
        float *Cr = C + (size_t)i*K;
        if(beta==0.0f) for(int p=0;p<K;p++) Cr[p]=0.0f;
        else            for(int p=0;p<K;p++) Cr[p]*=beta;
        const float *Ar = A + (size_t)i*N;
        for(int kk=0;kk<N;kk++){
            float a = Ar[kk];
            if(a==0.0f) continue;
            const float *Br = B + (size_t)kk*K;   /* B[N][K]: stride K */
            #pragma omp simd
            for(int p=0;p<K;p++) Cr[p] += a*Br[p];
        }
    }
}
