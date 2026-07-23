/* bisect_pool_conv.c -- test stage1 pool -> stage2 conv gradient chain */
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

/* Test just stage1 weights (w1/b1) with pool -> stage2 conv chain */
int main(){
    /* TINY config: single stage, no pool */
    ConvConfig3 cfg_tiny = {28,28, 32,5,2, 0,1,1, 0,1,1};
    ConvNet3 *cn_tiny = convnet3_create(&cfg_tiny);
    int D_tiny = convnet3_dim(cn_tiny);
    float *img=malloc(28*28*sizeof(float));
    float *dfeat=malloc((size_t)D_tiny*sizeof(float));
    uint32_t rs=12345;
    fill_rnd(img, 28*28, &rs, 0.5f);
    fill_rnd(dfeat, D_tiny, &rs, 0.7f);

    convnet3_zero_grad(cn_tiny);
    float *feat=malloc((size_t)D_tiny*sizeof(float));
    convnet3_forward(cn_tiny, img, feat);
    convnet3_zero_grad(cn_tiny);
    convnet3_backward(cn_tiny, img, feat, dfeat);

    const double h = 1e-3;
    int nlayers = convnet3_layer_count(cn_tiny);
    for(int L=0; L<nlayers; L++){
        ConvLayer3 layer = convnet3_layer(cn_tiny, L);
        if(layer.n==0) continue;
        int step = layer.n > 400 ? layer.n/200 : 1;
        int checked=0, failed=0; double lay_max=0;
        for(int i=0; i<layer.n; i+=step){
            float w0 = layer.param[i];
            layer.param[i] = w0 + (float)h; double lp = loss_of(cn_tiny, img, dfeat, D_tiny);
            layer.param[i] = w0 - (float)h; double lm = loss_of(cn_tiny, img, dfeat, D_tiny);
            layer.param[i] = w0;
            double num = (lp - lm)/(2.0*h);
            double ana = (double)layer.grad[i];
            double e = fabs(num - ana);
            if(e > lay_max) lay_max = e;
            if(e > 1e-2) failed++;
            checked++;
        }
        printf("TINY layer %d: n=%d checked=%d max|ana-num|=%.3e fails(>1e-2)=%d\n",
               L, layer.n, checked, lay_max, failed);
    }
    convnet3_destroy(cn_tiny);
    free(img); free(dfeat); free(feat);
    return 0;
}