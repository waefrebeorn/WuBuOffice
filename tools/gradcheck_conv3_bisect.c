/* gradcheck_conv3_bisect.c -- isolate the MED bug: stage1 pool -> stage2 conv */
#include "convnet3.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void fill_rnd(float *a, int n, uint32_t *s, float scale){
    for(int i=0;i<n;i++){ *s^=*s<<13; *s^=*s>>17; *s^=*s<<5; float u=(float)*s/4294967296.0f; a[i]=(u-0.5f)*scale; }
}
static double loss_of(ConvNet3 *cn, float *img, float *dfeat, int D){
    float *feat=malloc((size_t)D*sizeof(float));
    convnet3_forward(cn, img, feat);
    double L=0; for(int i=0;i<D;i++) L += (double)dfeat[i]*feat[i];
    free(feat);
    return L;
}

int main(){
    ConvConfig3 cfg = {28,28, 16,5,2, 32,5,2, 64,3,1};  /* CONV_MED */
    ConvNet3 *cn = convnet3_create(&cfg);
    int D = convnet3_dim(cn);
    float *img=malloc(28*28*sizeof(float));
    float *dfeat=malloc((size_t)D*sizeof(float));
    uint32_t rs=12345;
    fill_rnd(img, 28*28, &rs, 0.5f);
    fill_rnd(dfeat, D, &rs, 0.7f);

    convnet3_zero_grad(cn);
    float *feat=malloc((size_t)D*sizeof(float));
    convnet3_forward(cn, img, feat);
    convnet3_zero_grad(cn);
    convnet3_backward(cn, img, feat, dfeat);

    const double h = 1e-3;
    int nlayers = convnet3_layer_count(cn);
    int worst_layer = -1; double worst_err = 0; int total_fail = 0;

    for(int L=0; L<nlayers; L++){
        ConvLayer3 layer = convnet3_layer(cn, L);
        if(layer.n==0) continue;
        int step = layer.n > 400 ? layer.n/200 : 1;
        int checked=0, failed=0; double lay_max=0;
        for(int i=0; i<layer.n; i+=step){
            float w0 = layer.param[i];
            layer.param[i] = w0 + (float)h; double lp = loss_of(cn, img, dfeat, D);
            layer.param[i] = w0 - (float)h; double lm = loss_of(cn, img, dfeat, D);
            layer.param[i] = w0;
            double num = (lp - lm)/(2.0*h);
            double ana = (double)layer.grad[i];
            double e = fabs(num - ana);
            if(e > lay_max) lay_max = e;
            if(e > 5e-1) failed++;
            checked++;
        }
        printf("layer %d: n=%d checked=%d max|ana-num|=%.3e fails(>5e-1)=%d\n",
               L, layer.n, checked, lay_max, failed);
        if(lay_max > worst_err){ worst_err = lay_max; worst_layer = L; }
        total_fail += failed;
    }
    printf("=== GRADCHECK_MED: worst_max_err=%.3e at layer %d, total_fails=%d ===\n",
           worst_err, worst_layer, total_fail);
    printf(worst_err < 5e-1 && total_fail==0 ? "PASS\n" : "FAIL\n");
    convnet3_destroy(cn);
    free(img); free(dfeat); free(feat);
    return (worst_err < 5e-1 && total_fail==0) ? 0 : 1;
}