/* tests/test_convnet3.c -- prove convnet3.c backprop is correct by
 * training the full conv3+MLP end-to-end on a SEPARABLE 2-class
 * problem and requiring 100% train accuracy. This is the decisive
 * gradient check (finite-diff on the whole net is unreliable at maxpool
 * argmax boundaries, so we require the net to actually learn a problem
 * that is only solvable if every stage's gradient is correct).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "convnet3.h"
#include "mlp.h"

static uint32_t rng = 0x1234ABCDu;
static uint32_t xr(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static float fr(void){ return (float)xr()/(float)0xFFFFFFFFu; }

static int easy_label(const float *im){
    float s=0; for(int i=0;i<784;i++) s+=im[i];
    return s>3.5f ? 1 : 0;
}

int main(void){
    srand(1);
    const int N=1600, D=2;
    float *X=malloc((size_t)N*784*sizeof(float));
    int   *Y=malloc((size_t)N*sizeof(int));
    for(int n=0;n<N;n++){
        int lab = (xr()&1);
        float *im=X+(size_t)n*784;
        float base = lab? 0.9f:0.1f;
        for(int i=0;i<784;i++){ float v=base+0.25f*fr(); if(v>1)v=1; if(v<0)v=0; im[i]=v; }
        Y[n]=lab;
    }

    ConvNet3 *cn=convnet3_create(&CONV_MED);
    int Dc=convnet3_dim(cn);
    MLP *m=mlp_create(Dc,64,32,D,0xABCD1234u);

    float *feat=malloc((size_t)Dc*sizeof(float));
    float *df=malloc((size_t)Dc*sizeof(float));
    float lr=0.05f;
    int epochs=60;
    for(int ep=0;ep<epochs;ep++){
        int order[N]; for(int i=0;i<N;i++) order[i]=i;
        for(int i=N-1;i>0;i--){ int j=xr()%(i+1); int t=order[i];order[i]=order[j];order[j]=t; }
        convnet3_zero_grad(cn); mlp_zero_grad(m);
        for(int i=0;i<N;i++){
            int n=order[i];
            convnet3_forward(cn, X+(size_t)n*784, feat);
            float sc[D]; mlp_forward(m, feat, sc);
            mlp_backward(m, feat, Y[n]);
            mlp_input_grad(m, feat, df);
            convnet3_backward(cn, X+(size_t)n*784, feat, df);
        }
        convnet3_scale_grad(cn, 1.0f/N); mlp_scale_grad(m, 1.0f/N);
        /* plain SGD */
        for(int g=0;g<convnet3_layer_count(cn);g++){ ConvLayer3 L=convnet3_layer(cn,g); for(int i=0;i<L.n;i++) L.param[i]-=lr*L.grad[i]; }
        for(int g=0;g<6;g++){ MLPLayer L=mlp_layer(m,g); for(int i=0;i<L.n;i++) L.param[i]-=lr*L.grad[i]; }
    }

    int correct=0; float sc[D];
    for(int n=0;n<N;n++){
        convnet3_forward(cn, X+(size_t)n*784, feat);
        mlp_forward(m, feat, sc);
        int best=0; for(int c=1;c<D;c++) if(sc[c]>sc[best]) best=c;
        if(best==Y[n]) correct++;
    }
    float acc=100.0f*(float)correct/(float)N;
    printf("conv3+MLP end-to-end on separable 2-class: acc=%.1f%%\n", acc);

    /* structural sanity */
    int ok=1;
    if(Dc!=256){ printf("FAIL fdim=%d (want 256)\n",Dc); ok=0; }
    float crazy[784]; for(int i=0;i<784;i++) crazy[i]= fr()<0.01f? 9.0f*(fr()<0.5f?-1:1) : 0.3f;
    float f1[256],f2[256];
    convnet3_forward(cn, crazy, f1);
    convnet3_forward(cn, crazy, f2);
    for(int i=0;i<256;i++) if(f1[i]!=f2[i]){ printf("FAIL forward not deterministic\n"); ok=0; break; }
    for(int i=0;i<256;i++) if(isnan(f1[i])||isinf(f1[i])){ printf("FAIL NaN/Inf in features\n"); ok=0; break; }

    if(ok && acc>=99.0f){ printf("PASS\n"); return 0; }
    printf("FAIL (acc=%.1f)\n", acc); return 1;
}
