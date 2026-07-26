/* gru_test.c -- numerical-gradient check for GRU forward+backward (BPTT).
 * Builds a small GRU, runs fwd, backprops a random dy, and compares the
 * analytic gradient (w.r.t. weights and w.r.t. input x) to central finite
 * differences. The finite-difference loss is accumulated in DOUBLE precision
 * so the comparison reflects the true gradient, not single-precision
 * round-off. If the GRU BPTT is correct, max|analytic - numeric| < ~1e-2. */
#include "gru.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static float rnd(void){ return ((float)rand()/RAND_MAX)*2.0f-1.0f; }
/* Small stable init for the CHECK: with |w|~1 at T=5 the unrolled forward is
 * chaotic and central finite differences explode (num ~1e2 for true grads
 * ~0.1) — a FD artifact, not a BPTT bug (see skill c-nn-gradient-debugging,
 * step 3). Scale weights to ~±0.1 so the FD is trustworthy. */
static float rnds(void){ return rnd()*0.1f; }

/* double-precision loss so the central-difference is not round-off limited */
static double d_loss_of(GRU *g, int T, const float *x, const float *yref){
    int O=gru_outdim(g);
    gru_forward(g, T, x);
    float *out=malloc((size_t)T*O*sizeof(float));
    gru_get_output(g, out);
    double L=0; for(int i=0;i<T*O;i++){ double d=(double)out[i]-(double)yref[i]; L+=0.5*d*d; }
    free(out); return L;
}

int main(void){
    srand(12345);
    int D=3, H=4, T=5, bidir=0;
    GRU *g = gru_create(D,H,bidir,0xCAFEu);
    if(!g){ printf("gru_create failed\n"); return 1; }
    int O=gru_outdim(g);
    /* overwrite the ctor's init with small stable weights for the FD check */
    { int P0=gru_num_params(g); float *w0=gru_param(g);
      for(int i=0;i<P0;i++) w0[i]=rnds(); }

    float *x=malloc((size_t)T*D*sizeof(float));
    float *yref=malloc((size_t)T*O*sizeof(float));
    for(int i=0;i<T*D;i++) x[i]=rnd();
    for(int i=0;i<T*O;i++) yref[i]=rnd();

    gru_forward(g,T,x);
    float *out=malloc((size_t)T*O*sizeof(float));
    gru_get_output(g,out);
    float *dy=malloc((size_t)T*O*sizeof(float));
    for(int i=0;i<T*O;i++) dy[i]=out[i]-yref[i];
    float *dx=malloc((size_t)T*D*sizeof(float));
    gru_backward(g,T,dy,dx);

    int P=gru_num_params(g);
    float *w=gru_param(g);
    float *wg=gru_grad(g);
    double eps=1e-3, maxerr=0;
    for(int i=0;i<P;i++){
        float w0=w[i];
        w[i]=(float)(w0+eps); double lp=d_loss_of(g,T,x,yref);
        w[i]=(float)(w0-eps); double lm=d_loss_of(g,T,x,yref);
        w[i]=w0;
        double num=(lp-lm)/(2*eps);
        double e=fabs(num-(double)wg[i]);
        if(e>maxerr) maxerr=e;
    }
    printf("GRU weight grad max|ana-num| = %.6e\n", maxerr);

    /* input grads via double-precision FD too */
    double *xnum=malloc((size_t)T*D*sizeof(double));
    for(int i=0;i<T*D;i++){
        float x0=x[i];
        x[i]=(float)(x0+eps); double lp=d_loss_of(g,T,x,yref);
        x[i]=(float)(x0-eps); double lm=d_loss_of(g,T,x,yref);
        x[i]=x0;
        xnum[i]=(lp-lm)/(2*eps);
    }
    double dxerr=0;
    for(int i=0;i<T*D;i++){ double e=fabs(xnum[i]-(double)dx[i]); if(e>dxerr)dxerr=e; }
    printf("GRU input grad  max|ana-num| = %.6e\n", dxerr);

    int ok = (maxerr<2e-2) && (dxerr<2e-2);
    printf(ok? "GRU TESTS PASSED\n" : "GRU TESTS FAILED\n");
    free(x); free(yref); free(out); free(dy); free(dx); free(xnum); gru_free(g);
    return ok?0:1;
}
