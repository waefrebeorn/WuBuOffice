/* test_gru_cmp.c -- compares the GPU-matmul GRU (src/gpu/gru_gpu.c, linked as
 * the real gru_* symbols in this binary) against the scalar CPU reference
 * (src/wubuocr/gru.c, included below with all public symbols renamed gruC_*).
 * Same seed => identical init => forward/grad/dx must match to float noise. */
#include "gru.h"
#include "gpu_blas.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

/* ---- CPU reference: include the REAL gru.c with renamed symbols ---- */
typedef struct GRUC GRUC;   /* replaces the typedef gru.h would provide (guard skips it) */
#define GRU GRUC
#define gru_create      gruC_create
#define gru_free        gruC_free
#define gru_outdim      gruC_outdim
#define gru_num_params  gruC_num_params
#define gru_param       gruC_param
#define gru_grad        gruC_grad
#define gru_zero_grad   gruC_zero_grad
#define gru_forward     gruC_forward
#define gru_get_output  gruC_get_output
#define gru_backward    gruC_backward
#include "../src/wubuocr/gru.c"
#undef GRU
#undef gru_create
#undef gru_free
#undef gru_outdim
#undef gru_num_params
#undef gru_param
#undef gru_grad
#undef gru_zero_grad
#undef gru_forward
#undef gru_get_output
#undef gru_backward

static uint32_t rng=0x1234u;
static float frnd(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return ((float)(rng&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f; }

int main(void){
    int D=5,H=6,T=9;
    GRU  *rg=gru_create(D,H,0,0xABCDu);   /* GPU-path implementation */
    GRUC *rc=gruC_create(D,H,0,0xABCDu);  /* scalar CPU reference    */
    printf("gpu=%d\n", gpu_available());
    int od=gru_outdim(rg);

    /* same seed must give identical params; assert it */
    float *pg=gru_param(rg), *pc=gruC_param(rc);
    int npcmp=gru_num_params(rg);
    int wdiff=0; for(int i=0;i<npcmp;i++) if(fabsf(pg[i]-pc[i])>1e-6f) wdiff++;
    printf("WEIGHT diffs=%d/%d\n", wdiff, npcmp);

    float *x=malloc((size_t)T*D*sizeof(float)); for(int i=0;i<T*D;i++)x[i]=frnd();
    float *yg=malloc((size_t)T*od*sizeof(float)), *yc=malloc((size_t)T*od*sizeof(float));
    gru_forward(rg,T,x);  gru_get_output(rg,yg);
    gruC_forward(rc,T,x); gruC_get_output(rc,yc);
    float mf=0; for(int i=0;i<T*od;i++){float d=fabsf(yg[i]-yc[i]); if(d>mf)mf=d;}
    printf("FORWARD maxdiff=%.3e %s\n", mf, mf<1e-5f?"PASS":"FAIL");

    /* analytic grad compare */
    float *dy=calloc((size_t)T*od,sizeof(float)); for(int i=0;i<T*od;i++)dy[i]=1.0f;
    float *dxg=malloc((size_t)T*D*sizeof(float)), *dxc=malloc((size_t)T*D*sizeof(float));
    gru_zero_grad(rg);  gru_backward(rg,T,dy,dxg);
    gruC_zero_grad(rc); gruC_backward(rc,T,dy,dxc);
    float *gg=gru_grad(rg), *gc=gruC_grad(rc);
    float mg=0; for(int i=0;i<npcmp;i++){float d=fabsf(gg[i]-gc[i]); if(d>mg)mg=d;}
    printf("GRAD maxdiff=%.3e (%d params) %s\n", mg, npcmp, mg<1e-4f?"PASS":"FAIL");
    float md=0; for(int i=0;i<T*D;i++){float d=fabsf(dxg[i]-dxc[i]); if(d>md)md=d;}
    printf("DX maxdiff=%.3e %s\n", md, md<1e-4f?"PASS":"FAIL");

    int ok = (wdiff==0) && mf<1e-5f && mg<1e-4f && md<1e-4f;
    printf(ok?"OVERALL: PASS\n":"OVERALL: FAIL\n");
    free(x); free(yg); free(yc); free(dy); free(dxg); free(dxc);
    gru_free(rg); gruC_free(rc);
    return ok?0:1;
}
