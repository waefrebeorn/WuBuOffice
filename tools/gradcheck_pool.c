/* gradcheck_pool.c -- isolate the pooling+leaky backward path.
 * Single conv stage WITH maxpool, leak=1 (linear so FD exact), to test the
 * pool scatter-backward and the conv dW under real pooling. Loss = sum feat.
 * Build: cc -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc/wubuocr gradcheck_pool.c \
 *        src/wubuocr/convnet3.c -lm -o /tmp/gc_pool1
 */
#include "convnet3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
static float rnd(uint32_t *s){ *s^=*s<<13; *s^=*s>>17; *s^=*s<<5; return (float)(*s&0xFFFFFF)/16777216.0f*2-1; }
static void fill(float*a,int n,uint32_t*s,float sc){ for(int i=0;i<n;i++)a[i]=rnd(s)*sc; }
static double loss_of(ConvNet3*cn,const float*img,const float*df,int D){
    float*f=malloc((size_t)D*sizeof(float)); convnet3_forward(cn,img,f);
    double s=0; for(int k=0;k<D;k++)s+=(double)df[k]*(double)f[k]; free(f); return s;
}
int main(void){
    putenv("CN_LEAK=1");                 /* linear: FD exact even with pool */
    ConvConfig3 cfg={28,28, 8,3,2, 0,1,1, 0,1,1}; /* single stage, 8 filters 3x3 stride2, POOL 2 */
    ConvNet3*cn=convnet3_create(&cfg);
    int D=convnet3_dim(cn);
    float*img=malloc(28*28*sizeof(float)),*df=malloc((size_t)D*sizeof(float));
    uint32_t rs=99; fill(img,28*28,&rs,0.5f); fill(df,D,&rs,0.7f);
    convnet3_zero_grad(cn); float*feat=malloc((size_t)D*sizeof(float));
    convnet3_forward(cn,img,feat); convnet3_zero_grad(cn); convnet3_backward(cn,img,feat,df);
    const double h=1e-2;
    int nl=convnet3_layer_count(cn); int worst=-1; double we=0; int tot=0;
    for(int L=0;L<nl;L++){
        ConvLayer3 ly=convnet3_layer(cn,L); if(ly.n==0)continue;
        int step=ly.n>400?ly.n/200:1; int chk=0,fail=0; double lm2=0;
        for(int i=0;i<ly.n;i+=step){
            float w0=ly.param[i]; ly.param[i]=w0+(float)h; double lp=loss_of(cn,img,df,D);
            ly.param[i]=w0-(float)h; double lm=loss_of(cn,img,df,D); ly.param[i]=w0;
            double num=(lp-lm)/(2*h); double ana=(double)ly.grad[i]; double e=fabs(num-ana);
            if(e>lm2)lm2=e; if(e>2e-2)fail++; chk++;
        }
        printf("layer %d: n=%d checked=%d maxerr=%.3e fails=%d\n",L,ly.n,chk,lm2,fail);
        if(lm2>we){we=lm2;worst=L;} tot+=fail;
    }
    printf("=== POOL GRADCHECK: worst=%.3e layer %d fails=%d ===\n",we,worst,tot);
    printf(we<2e-2&&tot==0?"PASS\n":"FAIL\n");
    return (we<2e-2&&tot==0)?0:1;
}
