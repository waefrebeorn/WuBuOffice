/* test_convnet3_im2col.c -- verify the im2col+GEMM rewrite of convnet3 is
 * NUMERICALLY equivalent to the original conv math, and that backward is a
 * correct gradient (finite-difference check). Standalone, no deps. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "convnet3.h"

static float rnd(float lo,float hi){ static unsigned s=12345; s=s*1664525u+1013904223u;
    return lo+(float)(s&0xffff)/(float)0xffff*(hi-lo); }

int main(void){
    ConvConfig3 cfg={28,28, 16,5,2, 32,5,2, 64,3,1};
    ConvNet3 *cn=convnet3_create(&cfg);
    convnet3_set_leak(cn,0.0f);   /* reference math below is HARD ReLU */
    int D=convnet3_dim(cn);

    float *img=malloc(28*28*sizeof(float));
    for(int i=0;i<28*28;i++) img[i]=rnd(-1,1);

    /* new forward */
    float *fnew=malloc(D*sizeof(float));
    convnet3_forward(cn,img,fnew);

    /* ---- brute-force reference forward (valid conv + maxpool + relu, no IN) ---- */
    ConvLayer3 Lw1=convnet3_layer(cn,0), Lb1=convnet3_layer(cn,1);
    ConvLayer3 Lw2=convnet3_layer(cn,2), Lb2=convnet3_layer(cn,3);
    ConvLayer3 Lw3=convnet3_layer(cn,4), Lb3=convnet3_layer(cn,5);
    int K1=cfg.K1,S1=cfg.S1,K2=cfg.K2,S2=cfg.S2,K3=cfg.K3,S3=cfg.S3;
    int c1H=28-S1+1,c1W=28-S1+1,p1H=c1H/2,p1W=c1W/2;
    int c2H=p1H-S2+1,c2W=p1W-S2+1,p2H=c2H/2,p2W=c2W/2;
    int c3H=p2H-S3+1,c3W=p2W-S3+1;
    float *c1=malloc(c1H*c1W*K1*sizeof(float));
    for(int k=0;k<K1;k++)for(int y=0;y<c1H;y++)for(int x=0;x<c1W;x++){
        float s=Lb1.param[k]; const float*w=Lw1.param+(size_t)k*S1*S1;
        for(int dy=0;dy<S1;dy++)for(int dx=0;dx<S1;dx++) s+=w[dy*S1+dx]*img[(y+dy)*28+(x+dx)];
        float a=s>0?s:0; c1[(y*c1W+x)*K1+k]=a;
    }
    float *p1=malloc(p1H*p1W*K1*sizeof(float));
    for(int k=0;k<K1;k++)for(int py=0;py<p1H;py++)for(int px=0;px<p1W;px++){
        float bv=-1e30f; for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)
            bv=fmaxf(bv,c1[((py*2+dy)*c1W+(px*2+dx))*K1+k]); p1[(py*p1W+px)*K1+k]=bv;
    }
    float *c2=malloc(c2H*c2W*K2*sizeof(float));
    for(int k=0;k<K2;k++)for(int y=0;y<c2H;y++)for(int x=0;x<c2W;x++){
        float s=Lb2.param[k]; const float*w=Lw2.param+(size_t)k*K1*S2*S2;
        for(int c=0;c<K1;c++)for(int dy=0;dy<S2;dy++)for(int dx=0;dx<S2;dx++)
            s+=w[(c*S2+dy)*S2+dx]*p1[((y+dy)*p1W+(x+dx))*K1+c];
        float a=s>0?s:0; c2[(y*c2W+x)*K2+k]=a;
    }
    float *p2=malloc(p2H*p2W*K2*sizeof(float));
    for(int k=0;k<K2;k++)for(int py=0;py<p2H;py++)for(int px=0;px<p2W;px++){
        float bv=-1e30f; for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)
            bv=fmaxf(bv,c2[((py*2+dy)*c2W+(px*2+dx))*K2+k]); p2[(py*p2W+px)*K2+k]=bv;
    }
    float *fref=malloc(D*sizeof(float));
    for(int y=0;y<c3H;y++)for(int x=0;x<c3W;x++)for(int k=0;k<K3;k++){
        float s=Lb3.param[k]; const float*w=Lw3.param+(size_t)k*K2*S3*S3;
        for(int c=0;c<K2;c++)for(int dy=0;dy<S3;dy++)for(int dx=0;dx<S3;dx++)
            s+=w[(c*S3+dy)*S3+dx]*p2[((y+dy)*p2W+(x+dx))*K2+c];
        float a=s>0?s:0; fref[(y*c3W+x)*K3+k]=a;
    }
    float maxd=0; for(int i=0;i<D;i++) maxd=fmaxf(maxd,fabsf(fnew[i]-fref[i]));
    printf("[fwd] max|new-ref| = %.3e (D=%d)\n", maxd, D);

    /* ---- gradient check on a BIAS (enters linearly before ReLU -> stable FD) ---- */
    float eps=1e-2f;
    float w0 = Lb3.param[0];   /* bias of final conv, channel 0 */

    /* reference analytic gradient via forward+backward on cn2 */
    ConvNet3 *cn2=convnet3_create(&cfg);
    convnet3_set_leak(cn2,0.0f);
    memcpy(convnet3_layer(cn2,0).param, Lw1.param, K1*S1*S1*sizeof(float));
    memcpy(convnet3_layer(cn2,1).param, Lb1.param, K1*sizeof(float));
    memcpy(convnet3_layer(cn2,2).param, Lw2.param, K2*K1*S2*S2*sizeof(float));
    memcpy(convnet3_layer(cn2,3).param, Lb2.param, K2*sizeof(float));
    memcpy(convnet3_layer(cn2,4).param, Lw3.param, K3*K2*S3*S3*sizeof(float));
    memcpy(convnet3_layer(cn2,5).param, Lb3.param, K3*sizeof(float));
    float *feat=malloc(D*sizeof(float)); for(int i=0;i<D;i++) feat[i]=1.0f;
    float *feat_tmp=malloc(D*sizeof(float));
    convnet3_forward(cn2,img,feat_tmp);
    free(feat_tmp);
    convnet3_zero_grad(cn2);
    convnet3_backward(cn2,img,feat,feat);
    float gnum = convnet3_layer(cn2,5).grad[0];  /* b3 grad */

    /* finite diff: forward with b3[0]+eps and -eps */
    float *fp=malloc(D*sizeof(float));
    ConvNet3 *cn3=convnet3_create(&cfg);
    convnet3_set_leak(cn3,0.0f);
    memcpy(convnet3_layer(cn3,0).param, Lw1.param, K1*S1*S1*sizeof(float));
    memcpy(convnet3_layer(cn3,1).param, Lb1.param, K1*sizeof(float));
    memcpy(convnet3_layer(cn3,2).param, Lw2.param, K2*K1*S2*S2*sizeof(float));
    memcpy(convnet3_layer(cn3,3).param, Lb2.param, K2*sizeof(float));
    memcpy(convnet3_layer(cn3,4).param, Lw3.param, K3*K2*S3*S3*sizeof(float));
    memcpy(convnet3_layer(cn3,5).param, Lb3.param, K3*sizeof(float));
    convnet3_layer(cn3,5).param[0]=w0+eps;
    convnet3_forward(cn3,img,fp);

    float *fm=malloc(D*sizeof(float));
    ConvNet3 *cn4=convnet3_create(&cfg);
    convnet3_set_leak(cn4,0.0f);
    memcpy(convnet3_layer(cn4,0).param, Lw1.param, K1*S1*S1*sizeof(float));
    memcpy(convnet3_layer(cn4,1).param, Lb1.param, K1*sizeof(float));
    memcpy(convnet3_layer(cn4,2).param, Lw2.param, K2*K1*S2*S2*sizeof(float));
    memcpy(convnet3_layer(cn4,3).param, Lb2.param, K2*sizeof(float));
    memcpy(convnet3_layer(cn4,4).param, Lw3.param, K3*K2*S3*S3*sizeof(float));
    memcpy(convnet3_layer(cn4,5).param, Lb3.param, K3*sizeof(float));
    convnet3_layer(cn4,5).param[0]=w0-eps;
    convnet3_forward(cn4,img,fm);

    float fd=0.0f;
    for(int i=0;i<D;i++) fd+=(fp[i]-fm[i]);
    fd/= (2*eps);
    printf("[grad] bias analytic=%.4f  finite-diff=%.4f  (|diff|=%.3e)\n", gnum, fd, fabsf(gnum-fd));

    int ok = (maxd < 1e-3f) && (fabsf(gnum-fd) < 2e-2f);
    printf("%s\n", ok ? "PASS" : "FAIL");

    free(img);free(fnew);free(fref);free(feat);
    free(c1);free(p1);free(c2);free(p2);free(fp);free(fm);
    convnet3_destroy(cn);convnet3_destroy(cn2);convnet3_destroy(cn3);convnet3_destroy(cn4);
    return ok?0:1;
}
