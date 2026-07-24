/* test_gpu_blas.c -- verify the CUDA GEMM/conv match the CPU reference.
 * Uses a deterministic LCG so it's reproducible. Run with no args; prints
 * PASS/FAIL. When built CPU-only (no CUDA), gpu_available()==0 and the
 * functions are pure-CPU, so it still validates the stub path. */
#include "gpu_blas.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

static uint32_t rng = 0x12345678u;
static float frnd(void){ rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5;
    return ((float)(rng & 0xFFFFFF)/(float)0xFFFFFF) * 2.0f - 1.0f; }

static int max_diff(const float *a, const float *b, int n, float *md){
    float m = 0.0f;
    for(int i=0;i<n;i++){ float d = fabsf(a[i]-b[i]); if(d>m)m=d; }
    *md = m; return m < 1e-3f;
}

int main(void){
    int fails = 0;
    printf("gpu_available = %d\n", gpu_available());

    /* ---- GEMM ---- */
    {
        int M=64,K=96,N=48;
        float *A=malloc((size_t)M*K*sizeof(float));
        float *B=malloc((size_t)K*N*sizeof(float));
        float *Cg=malloc((size_t)M*N*sizeof(float));
        float *Cc=malloc((size_t)M*N*sizeof(float));
        for(int i=0;i<M*K;i++)A[i]=frnd();
        for(int i=0;i<K*N;i++)B[i]=frnd();
        gpu_gemm(A,B,Cg,M,K,N);
        /* CPU ref inline */
        for(int i=0;i<M;i++)for(int j=0;j<N;j++){float s=0;for(int k=0;k<K;k++)s+=A[(size_t)i*K+k]*B[(size_t)k*N+j];Cc[(size_t)i*N+j]=s;}
        float md; int ok=max_diff(Cg,Cc,M*N,&md);
        printf("GEMM   %s (maxdiff=%.2e)\n", ok?"PASS":"FAIL", md);
        if(!ok)fails++;
        free(A);free(B);free(Cg);free(Cc);
    }

    /* ---- CONV forward ---- */
    {
        int Hin=20,Win=20,Cin=3,Cout=8,Kh=3,Kw=3,S=1,P=1,act=1;
        int Hout,Wout;
        int nin=Hin*Win*Cin, nw=Cout*Cin*Kh*Kw, nout;
        float *in=malloc(nin*sizeof(float));
        float *w=malloc(nw*sizeof(float));
        float *b=malloc(Cout*sizeof(float));
        for(int i=0;i<nin;i++)in[i]=frnd();
        for(int i=0;i<nw;i++)w[i]=frnd();
        for(int i=0;i<Cout;i++)b[i]=frnd();
        float *og=malloc((size_t)Hin*Win*Cout*sizeof(float));
        float *oc=malloc((size_t)Hin*Win*Cout*sizeof(float));
        gpu_conv2d_fwd(in,Hin,Win,Cin,w,b,Cout,Kh,Kw,S,P,act,og,&Hout,&Wout);
        /* CPU ref */
        int hout=(Hin+2*P-Kh)/S+1, wout=(Win+2*P-Kw)/S+1;
        for(int oc_=0;oc_<Cout;oc_++)for(int h=0;h<hout;h++)for(int ww=0;ww<wout;ww++){
            float acc=b[oc_]; int sh=h*S-P,sw=ww*S-P;
            for(int ic=0;ic<Cin;ic++)for(int kh=0;kh<Kh;kh++){int y=sh+kh;if(y<0||y>=Hin)continue;
                for(int kw=0;kw<Kw;kw++){int x=sw+kw;if(x<0||x>=Win)continue;
                    float v=in[(size_t)y*Win*Cin+(size_t)x*Cin+ic];
                    float wt=w[(((size_t)oc_*Cin+ic)*Kh+kh)*Kw+kw]; acc+=v*wt;}}
            if(act)acc=acc>0?acc:0;
            oc[((size_t)h*wout+ww)*Cout+oc_]=acc;
        }
        nout=hout*wout*Cout;
        float md; int ok=max_diff(og,oc,nout,&md);
        printf("CONV   %s (Hout=%d Wout=%d maxdiff=%.2e)\n", ok?"PASS":"FAIL", Hout,Wout, md);
        if(!ok)fails++;
        free(in);free(w);free(b);free(og);free(oc);
    }

    printf(fails? "OVERALL: FAIL\n" : "OVERALL: PASS\n");
    return fails?1:0;
}
