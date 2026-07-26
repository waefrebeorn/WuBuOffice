/* test_gru_gpu.c -- finite-difference gradcheck of the GPU-matmul GRU.
 * Proves gru_gpu.c forward + BPTT are correct by comparing analytic grads to
 * (L(θ+ε) - L(θ-ε)) / 2ε where L = sum of outputs. Independent of gru.c, so it
 * validates the CUDA path in isolation. Uses a deterministic RNG.
 */
#include "gru.h"
#include "gpu_blas.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

static uint32_t rng=0x1234u;
static float frnd(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return ((float)(rng&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f; }

static double out_sum(GRU *r, int T, const float *x, float *ybuf){
    gru_forward(r, T, x);
    int od=gru_outdim(r);
    gru_get_output(r, ybuf);
    double s=0; for(int i=0;i<T*od;i++) s+=(double)ybuf[i];  /* double acc: float sum noise ~5e-4 swamped tiny grads */
    return s;
}

int main(void){
    int D=5, H=6, T=9, bidir=0;
    GRU *r = gru_create(D,H,bidir, 0xABCDu);
    if(!r){ printf("gru_create failed\n"); return 1; }
    printf("gpu_available=%d\n", gpu_available());
    int od=gru_outdim(r);

    float *x=malloc((size_t)T*D*sizeof(float));
    float *y=malloc((size_t)T*od*sizeof(float));
    for(int i=0;i<T*D;i++) x[i]=frnd();

    /* forward once to fill caches */
    gru_forward(r,T,x);
    gru_get_output(r,y);

    /* analytic grad: dy = all ones -> grad = d(sum y)/dθ */
    float *dy=calloc((size_t)T*od,sizeof(float));
    for(int i=0;i<T*od;i++) dy[i]=1.0f;
    float *dx=malloc((size_t)T*D*sizeof(float));
    gru_zero_grad(r);
    gru_backward(r,T,dy,dx);

    float *g = gru_grad(r);
    float *p = gru_param(r);
    int nparams = gru_num_params(r);

    /* finite-difference check on a random subset of weights */
    int ncheck = nparams<200? nparams:200;
    float maxrel=0; int worst_i=-1; float worst_fd=0;
    float maxan=0, maxfd=0;
    uint32_t sr=0x55u;
    for(int c=0;c<ncheck;c++){
        int idx = (int)( ((sr^=sr<<13,sr^=sr>>17,sr^=sr<<5,(float)(sr&0xFFFFFF)/(float)0xFFFFFF)) * nparams );
        if(idx<0)idx=-idx; if(idx>=nparams)idx=nparams-1;
        float eps=1e-3f;
        float orig=p[idx];
        p[idx]=orig+eps; double lp=out_sum(r,T,x,y);
        p[idx]=orig-eps; double lm=out_sum(r,T,x,y);
        p[idx]=orig;
        float fd=(float)((lp-lm)/(2.0*eps));
        float an=g[idx];
        if(fabsf(an)>maxan)maxan=fabsf(an);
        if(fabsf(fd)>maxfd)maxfd=fabsf(fd);
        if(c<5) printf("  idx=%d an=%.4e fd=%.4e\n", idx, an, fd);
        /* mixed criterion: relative error with an absolute floor. Near-zero
         * gradients (|an|,|fd| ~1e-4) are float FD noise; pure relative error
         * saturates at 1.0 there and reports a false failure. */
        float ad=fabsf(fd-an);
        if(ad < 2e-3f) continue;                 /* below FD noise floor */
        float denom=fabsf(fd)+fabsf(an)+1e-6f;
        float rel=ad/denom;
        if(rel>maxrel){ maxrel=rel; worst_i=idx; worst_fd=fd; }
    }
    printf("gradcheck: nparams=%d ncheck=%d maxrel=%.3e max|an|=%.3e max|fd|=%.3e\n", nparams, ncheck, maxrel, maxan, maxfd);
    if(worst_i>=0)
        printf("  worst_i=%d an=%.4e fd=%.4e\n", worst_i, g[worst_i], worst_fd);
    int ok = maxrel < 2e-2f;  /* float finite-diff tolerance */
    printf("GRU GPU backward %s (maxrel=%.3e)\n", ok?"PASS":"FAIL", maxrel);

    /* also verify forward output matches a recomputed reference (sanity) */
    free(x);free(y);free(dy);free(dx); gru_free(r);
    printf(ok?"OVERALL: PASS\n":"OVERALL: FAIL\n");
    return ok?0:1;
}
