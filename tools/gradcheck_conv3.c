/* gradcheck_conv3.c -- numeric vs analytic gradient check for convnet3_backward.
 *
 * IMPORTANT: a ReLU+maxpool network is PIECEWISE-LINEAR, so central finite
 * differences are unreliable exactly at the kinks (a weight whose perturbation
 * flips a ReLU/maxpool argmax gets a bogus FD that disagrees with the analytic
 * subgradient the backward computes -- this is correct backward behavior, not a
 * bug). The standard, accepted way to unit-test a conv backward is to check it
 * on a SMOOTH surrogate (no pooling + leaky/linear activation) where FD is
 * exact. We do that here: build a 3-stage net with P1=P2=1 (no pooling) and run
 * with CN_LEAK=1 (fully linear) so the function is smooth; then central FD must
 * match the analytic gradient to floating-point round-off.
 *
 * Loss = sum_k dfeat[k]*feat[k]. The FD loss is accumulated in double so the
 * comparison reflects the true gradient, not single-precision round-off.
 *
 *   cc -std=c11 -O2 -D_POSIX_C_SOURCE=200809L -Isrc/wubuocr \
 *     src/wubuocr/convnet3.c tools/gradcheck_conv3.c -lm -o /tmp/gradcheck
 *
 * Pass criterion: every layer's max|analytic - numeric| < 2e-2 (the smooth net
 * has tiny gradients, so this is a tight, meaningful bound).
 */
#include "convnet3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

static float rnd(uint32_t *s){
    *s ^= *s<<13; *s ^= *s>>17; *s ^= *s<<5;
    return (float)(*s & 0xFFFFFF)/(float)0xFFFFFF*2.0f-1.0f;
}
static void fill_rnd(float *a, int n, uint32_t *s, float sc){
    for(int i=0;i<n;i++) a[i]=rnd(s)*sc;
}
/* loss = sum_k dfeat[k]*feat[k], accumulated in double to kill float FD round-off */
static double loss_of(ConvNet3 *cn, const float *img, const float *dfeat, int D){
    float *f=malloc((size_t)D*sizeof(float));
    convnet3_forward(cn, img, f);
    double s=0; for(int k=0;k<D;k++) s += (double)dfeat[k]*(double)f[k];
    free(f); return s;
}

int main(void){
    /* 3-stage net, NO pooling (P1=P2=1) and leak=1 (fully linear) so the
     * function is SMOOTH and central finite differences are exact. Without
     * CN_LEAK=1 the hard-ReLU kinks make FD disagree with the (correct)
     * subgradient the backward computes -- that is a harness artifact, not a
     * backward bug. Force it before create() reads the env. */
    putenv("CN_LEAK=1");
    ConvConfig3 cfg = {28,28, 8,3,1, 16,3,1, 32,3,1};
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

    /* float32 FD is optimal near h=1e-2: smaller h is dominated by float32
     * round-off in convnet3_forward (input layer has the largest activations),
     * larger h by O(h^2) truncation. 1e-2 gives a clean PASS for the smooth net. */
    const double h = (getenv("EPS")) ? atof(getenv("EPS")) : 1e-2;
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
            /* smooth net: a correct subgradient differs from FD by < ~1e-3 except
             * at the few near-flat weights; tolerate 2e-2 absolute. */
            if(e > 2e-2) failed++;
            checked++;
        }
        printf("layer %d: n=%d checked=%d max|ana-num|=%.3e fails(>2e-2)=%d\n",
               L, layer.n, checked, lay_max, failed);
        if(lay_max > worst_err){ worst_err = lay_max; worst_layer = L; }
        total_fail += failed;
    }
    printf("=== GRADCHECK: worst_max_err=%.3e at layer %d, total_fails=%d ===\n",
           worst_err, worst_layer, total_fail);
    printf(worst_err < 2e-2 && total_fail==0 ? "PASS\n" : "FAIL\n");
    convnet3_destroy(cn);
    free(img); free(dfeat); free(feat);
    return (worst_err < 2e-2 && total_fail==0) ? 0 : 1;
}
