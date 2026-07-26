/* ocl_gru_test.c -- verifies the OpenCL GRU forward (#98) matches the scalar
 * CPU GRU forward exactly. Builds a random GRU, runs the CPU path (gru_forward
 * + gru_get_output) and the OpenCL path (ocl_gru_dir both directions + manual
 * output assembly), and asserts the two outputs agree within a tight tolerance.
 * SKIPs cleanly (exit 0) if no OpenCL device is present. */
#include "gru.h"
#include "gru_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static float rnd(void){ static unsigned s=0x12345678u; s^=s<<13; s^=s>>17; s^=s<<5; return ((float)(s&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f; }

int main(void){
    int H=64, D=32, T=20, bidir=1;
    GRU *r = gru_create(D, H, bidir, 12345);
    if (!r) { printf("gru_create failed\n"); return 1; }
    /* randomize weights */
    float *P = gru_param(r);
    int n = gru_num_params(r);
    for (int i=0;i<n;i++) P[i] = rnd()*0.5f;

    float *x = malloc((size_t)T*D*sizeof(float));
    for (int i=0;i<T*D;i++) x[i] = rnd();

    /* CPU reference */
    gru_forward(r, T, x);
    int od = gru_outdim(r);
    float *ycpu = malloc((size_t)T*od*sizeof(float));
    gru_get_output(r, ycpu);

    /* GPU path: replicate gru_forward's cache fill + gru_get_output assembly */
    int Tcap = T;
    float *zf=calloc((size_t)Tcap*H,sizeof(float)), *rf=calloc((size_t)Tcap*H,sizeof(float)), *hf=calloc((size_t)Tcap*H,sizeof(float));
    float *zb=calloc((size_t)Tcap*H,sizeof(float)), *rb=calloc((size_t)Tcap*H,sizeof(float)), *hb=calloc((size_t)Tcap*H,sizeof(float));
    int gpu = ocl_gru_dir(P, H, D, T, x, 0, zf, rf, hf);
    if (bidir) gpu = gpu && ocl_gru_dir(P, H, D, T, x, 1, zb, rb, hb);
    if (!gpu) {
        printf("SKIP: no OpenCL device\n");
        free(x); free(ycpu);
        free(zf); free(rf); free(hf); free(zb); free(rb); free(hb);
        gru_free(r);
        return 0;
    }
    float *ygpu = malloc((size_t)T*od*sizeof(float));
    for (int t=0;t<T;t++){
        if (!bidir) memcpy(ygpu+(size_t)t*H, hf+(size_t)t*H, H*sizeof(float));
        else { memcpy(ygpu+(size_t)t*2*H,   hf+(size_t)t*H, H*sizeof(float));
               memcpy(ygpu+(size_t)t*2*H+H, hb+(size_t)t*H, H*sizeof(float)); }
    }

    float maxd = 0.0f;
    for (int i=0;i<T*od;i++){ float d=fabsf(ycpu[i]-ygpu[i]); if(d>maxd) maxd=d; }
    printf("max abs diff CPU vs OpenCL: %g\n", maxd);

    int rc = (maxd < 1e-3f) ? 0 : 1;
    if (rc) printf("FAIL: OpenCL output diverges from CPU\n");
    else    printf("PASS: OpenCL GRU forward matches CPU\n");

    free(x); free(ycpu); free(ygpu);
    free(zf); free(rf); free(hf); free(zb); free(rb); free(hb);
    gru_free(r);
    return rc;
}
