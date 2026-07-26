/* compare gru_gpu (CUDA-matmul) vs gru.c (CPU reference) directly. */
#include "gru.h"
#include "gpu_blas.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
/* CPU reference (renamed from gru.c) */
#include "/tmp/gruC.c"

static uint32_t rng=0x1234u;
static float frnd(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return ((float)(rng&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f; }

int main(void){
    int D=5,H=6,T=9;
    GRU *rg=gru_create(D,H,0,0xABCDu);
    GRUC *rc=gruC_create(D,H,0,0xABCDu);  /* same seed -> same init? gru.c uses its own rng */
    printf("gpu=%d\n", gpu_available());
    int od=gru_outdim(rg);
    float *x=malloc(T*D*4); for(int i=0;i<T*D;i++)x[i]=frnd();
    float *yg=malloc(T*od*4), *yc=malloc(T*od*4);
    gru_forward(rg,T,x); gru_get_output(rg,yg);
    gruC_forward(rc,T,x); gruC_get_output(rc,yc);
    float mf=0; for(int i=0;i<T*od;i++){float d=fabsf(yg[i]-yc[i]); if(d>mf)mf=d;}
    printf("FORWARD maxdiff=%.3e %s\n", mf, mf<1e-3?"PASS":"FAIL");
    /* weight dump head */
    float *pg=gru_param(rg), *pc=gruC_param(rc); int npcmp=gru_num_params(rg);
    int wdiff=0; for(int i=0;i<npcmp;i++) if(fabsf(pg[i]-pc[i])>1e-5f) wdiff++;
    printf("WEIGHT diffs=%d/%d (first pg,pc = %.4f %.4f)\n", wdiff, npcmp, pg[0], pc[0]);
    printf("h0: gpu=%.4f cpu=%.4f  hlast: gpu=%.4f cpu=%.4f\n",
           yg[0], yc[0], yg[(T-1)*od], yc[(T-1)*od]);
    for(int t=1;t<4;t++) printf("  h%d: gpu=%.6f cpu=%.6f\n", t, yg[t*od], yc[t*od]);
    /* manual forward with hardcoded GRUOffs (H=6,D=5): Wz0 Wr30 Wh60 Uz90 Ur126 Uh162 Bz198 Br204 Bh210 */
    {
        int H=6,D=5; const float *P=gru_param(rg);
        float *hh=calloc(T*H,sizeof(float)), *zz=calloc(T*H,sizeof(float));
        for(int t=0;t<T;t++){ for(int j=0;j<H;j++){
            float az=0,ar=0,ac=0;
            for(int i=0;i<D;i++){ az+=P[0+j*D+i]*x[t*D+i]; ar+=P[30+j*D+i]*x[t*D+i]; ac+=P[60+j*D+i]*x[t*D+i]; }
            float hp=(t==0)?0:hh[(t-1)*H+j];
            for(int k=0;k<H;k++){ float pv=(t==0)?0:hh[(t-1)*H+k]; az+=P[90+j*H+k]*pv; ar+=P[126+j*H+k]*pv; ac+=P[162+j*H+k]*(zz[t*H+k]*pv); }
            az+=P[198+j]; ar+=P[204+j]; ac+=P[210+j];
            zz[t*H+j]=1.0f/(1.0f+expf(-az));
            float rrv=1.0f/(1.0f+expf(-ar)); (void)rrv;
            float nv=tanhf(ac);
            hh[t*H+j]=(1.0f-zz[t*H+j])*hp+zz[t*H+j]*nv;
        }}
        printf("  manual h1=%.6f h2=%.6f hlast=%.6f\n", hh[1*H], hh[2*H], hh[(T-1)*H]);
        free(hh); free(zz);
    }

    /* analytic grad compare */
    float *dy=calloc(T*od,4); for(int i=0;i<T*od;i++)dy[i]=1.0f;
    float *dxg=malloc(T*D*4), *dxc=malloc(T*D*4);
    gru_zero_grad(rg); gru_backward(rg,T,dy,dxg);
    gruC_zero_grad(rc); gruC_backward(rc,T,dy,dxc);
    float *gg=gru_grad(rg), *gc=gruC_grad(rc);
    int np=gru_num_params(rg);
    float mg=0; for(int i=0;i<np;i++){float d=fabsf(gg[i]-gc[i]); if(d>mg)mg=d;}
    printf("GRAD maxdiff=%.3e (%d params) %s\n", mg, np, mg<1e-3?"PASS":"FAIL");
    float md=0; for(int i=0;i<T*D;i++){float d=fabsf(dxg[i]-dxc[i]); if(d>md)md=d;}
    printf("DX maxdiff=%.3e %s\n", md, md<1e-3?"PASS":"FAIL");

    int ok = mf<1e-3 && mg<1e-3 && md<1e-3;
    printf(ok?"OVERALL: PASS\n":"OVERALL: FAIL\n");
    return ok?0:1;
}
