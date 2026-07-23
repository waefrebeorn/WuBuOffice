/* test_grad_chain.c -- full gradient-chain check for the im2col convnet3.
 * Compares analytic gradient (backward) vs central finite-difference for w1,w2,w3
 * and biases b1,b2,b3, using a SMOOTH fixed target loss L = sum_i t[i]*out[i]
 * (so dL/dout = t, constant -> no ReLU-kink FD noise). Reports max rel error
 * per layer to localize any wrong gradient. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "convnet3.h"

static float rnd(void){ static unsigned s=99173u; s=s*1664525u+1013904223u; return (float)(s&0xffff)/65535.0f*2-1; }

static float loss_at(ConvNet3 *net, const float *img, const float *t, int D){
    float *f=malloc(D*sizeof(float)); convnet3_forward(net,img,f);
    float s=0; for(int i=0;i<D;i++) s+=t[i]*f[i]; free(f); return s;
}

int main(void){
    ConvConfig3 cfg={28,28, 16,5,2, 32,5,2, 64,3,1};
    int K1=cfg.K1,S1=cfg.S1,K2=cfg.K2,S2=cfg.S2,K3=cfg.K3,S3=cfg.S3;
    int nw1=K1*S1*S1, nw2=K2*K1*S2*S2, nw3=K3*K2*S3*S3;

    float *img=malloc(28*28*sizeof(float));
    for(int i=0;i<28*28;i++) img[i]=rnd();

    ConvNet3 *ref=convnet3_create(&cfg);
    int D=convnet3_dim(ref);

    float *t=malloc(D*sizeof(float));
    for(int i=0;i<D;i++) t[i]=(i%7-3)*0.1f;

    float L0 = loss_at(ref,img,t,D);

    /* gradient via backward with dfeat=t */
    float *feat=malloc(D*sizeof(float)); for(int i=0;i<D;i++) feat[i]=1.0f;
    float *ftmp=malloc(D*sizeof(float)); convnet3_forward(ref,img,ftmp); free(ftmp);
    convnet3_zero_grad(ref);
    convnet3_backward(ref,img,feat,t);

    typedef struct { int li; int n; const char *nm; } Linfo;
    Linfo layers[] = { {0,nw1,"w1"}, {1,K1,"b1"}, {2,nw2,"w2"}, {3,K2,"b2"}, {4,nw3,"w3"}, {5,K3,"b3"} };
    float eps=1e-2f;
    int worst_layer=-1; float worst_err=1e-9;
    for(int L=0;L<6;L++){
        ConvLayer3 LC = convnet3_layer(ref, layers[L].li);
        int n=layers[L].n;
        float maxrel=0;
        int step = n>400 ? n/400 : 1;
        for(int j=0;j<n;j+=step){
            float w0 = LC.param[j];
            LC.param[j]=w0+eps; float lp = loss_at(ref,img,t,D);
            LC.param[j]=w0-eps; float lm = loss_at(ref,img,t,D);
            LC.param[j]=w0;
            float fd=(lp-lm)/(2*eps);
            float ga=LC.grad[j];
            float rel = fabsf(ga-fd)/(fabsf(fd)+1e-3f);
            if(rel>maxrel) maxrel=rel;
        }
        printf("[%s] n=%d  maxrel_err=%.3e\n", layers[L].nm, n, maxrel);
        if(maxrel>worst_err){ worst_err=maxrel; worst_layer=L; }
    }
    printf("WORST: %s (rel=%.3e)  -> %s\n", layers[worst_layer].nm, worst_err,
           worst_err<5e-2?"OK":"FAIL");
    free(img);free(t);free(feat);
    convnet3_destroy(ref);
    return worst_err<5e-2?0:1;
}
