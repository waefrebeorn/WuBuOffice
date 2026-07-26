#include "gpu_blas.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static int fail=0;
static void chk(const char*name, const float*a,const float*b,int n,float tol){
    float m=0; for(int i=0;i<n;i++){float d=fabsf(a[i]-b[i]); if(d>m)m=d;}
    printf("%-10s maxdiff=%8.2e %s\n", name, m, m<tol?"PASS":"FAIL"); if(m>=tol)fail++;
}
int main(void){
    /* NT check: C[MxN]=A[MxK]*B[NxK]^T */
    {int M=4,K=5,N=3;
     float *A=malloc(M*K*4),*B=malloc(N*K*4),*C=malloc(M*N*4),*R=malloc(M*N*4);
     for(int i=0;i<M*K;i++)A[i]=((i*7)%11)/11.0f-0.5f;
     for(int i=0;i<N*K;i++)B[i]=((i*3)%13)/13.0f-0.5f;
     gpu_gemmNT(A,B,C,M,K,N);
     for(int i=0;i<M;i++)for(int j=0;j<N;j++){float s=0;for(int k=0;k<K;k++)s+=A[i*K+k]*B[j*K+k];R[i*N+j]=s;}
     chk("gemmNT",C,R,M*N,1e-4f);
     free(A);free(B);free(C);free(R);}
    /* T check: C[MxN]=A[KxM]^T*B[KxN] */
    {int M=4,K=5,N=3;
     float *A=malloc(K*M*4),*B=malloc(K*N*4),*C=malloc(M*N*4),*R=malloc(M*N*4);
     for(int i=0;i<K*M;i++)A[i]=((i*7)%11)/11.0f-0.5f;
     for(int i=0;i<K*N;i++)B[i]=((i*3)%13)/13.0f-0.5f;
     gpu_gemmT(A,B,C,M,K,N);
     for(int i=0;i<M;i++)for(int j=0;j<N;j++){float s=0;for(int k=0;k<K;k++)s+=A[k*M+i]*B[k*N+j];R[i*N+j]=s;}
     chk("gemmT",C,R,M*N,1e-4f);
     free(A);free(B);free(C);free(R);}
    printf(fail?"FAIL\n":"PASS\n"); return fail?1:0;
}
