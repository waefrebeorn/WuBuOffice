/* gradcheck_pool_valid.c -- single-stage conv+pool backward, REAL (fdim>0), no INORM.
 * Isolates the conv+maxpool backward chain. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "convnet3.h"

static uint32_t s_rng=0x1234ABCDu;
static float s_uni(void){ /* xorshift32 -> [-1,1) */
    uint32_t x=s_rng; x^=x<<13; x^=x>>17; x^=x<<5; s_rng=x;
    return (float)((int32_t)(x&0x7FFFFFFF))/(float)0x7FFFFFFF*2.0f - 1.0f;
}
static void fill(float*a,int n,uint32_t*rs,float sc){ for(int i=0;i<n;i++) a[i]=s_uni()*sc; }

int main(void){
    ConvConfig3 cfg={28,28, 16,3,2, 0,1,1, 0,1,1};  /* single stage, K1=16, pool2, fdim=13*13*16>0 */
    ConvNet3*cn=convnet3_create(&cfg);
    convnet3_set_leak(cn, 1.0f);   /* linear: avoids ReLU kinks */
    /* He-style weights via layer accessor (ConvNet3 is opaque) */
    int n1=cfg.K1*1*cfg.S1*cfg.S1;
    float s1=sqrtf(2.0f/1.0f);
    ConvLayer3 L1=convnet3_layer(cn,0);
    for(int i=0;i<n1;i++) L1.param[i]=s_uni()*s1;
    int D=convnet3_dim(cn);
    float*img=malloc(28*28*sizeof(float)),*df=malloc((size_t)D*sizeof(float));
    uint32_t rs=99; fill(img,28*28,&rs,0.5f); fill(df,D,&rs,0.7f);
    float*feat=malloc((size_t)D*sizeof(float));
    convnet3_forward(cn,img,feat); convnet3_zero_grad(cn); convnet3_backward(cn,img,feat,df);
    const double h=1e-2;
    int nl=convnet3_layer_count(cn); int worst=-1; double we=0; int tot=0;
    for(int L=0;L<nl;L++){
        ConvLayer3 ly=convnet3_layer(cn,L); if(ly.n==0)continue;
        int step=ly.n>600?ly.n/300:1; int chk=0,fail=0; double lm2=0;
        for(int i=0;i<ly.n;i+=step){
            float w0=ly.param[i]; ly.param[i]=w0+(float)h; double lp=0,lm=0;
            { float ft[30000]; convnet3_forward(cn,img,ft); for(int q=0;q<D;q++) lp+=df[q]*ft[q]; }
            ly.param[i]=w0-(float)h;
            { float ft[30000]; convnet3_forward(cn,img,ft); for(int q=0;q<D;q++) lm+=df[q]*ft[q]; }
            ly.param[i]=w0;
            double num=(lp-lm)/(2*h); double ana=(double)ly.grad[i]; double e=fabs(num-ana);
            if(e>lm2)lm2=e; if(e>5e-2)fail++; chk++;
            if(L==1 && i<3) printf("  dbg bias L1 i=%d ana=%.5f num=%.5f\n",i,ana,num);
        }
        printf("layer %d: n=%d checked=%d maxerr=%.4e fails=%d\n",L,ly.n,chk,lm2,fail);
        if(lm2>we){we=lm2;worst=L;} tot+=fail;
    }
    printf("POOL-VALID GRADCHECK: worst=%.4e layer %d fails=%d\n",we,worst,tot);
    return tot?1:0;
}
