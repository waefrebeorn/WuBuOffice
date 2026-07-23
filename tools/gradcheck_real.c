/* gradcheck_real.c -- gradcheck conv backward with the EXACT training config:
 * leak=0.1 (hard-ish ReLU), INORM ON, maxpool ON. Uses a LARGE fd eps (1e-1)
 * so weight perturbations rarely flip a ReLU/MaxPool argmax => FD is reliable
 * despite the kinks the smooth-net gradcheck avoided. Loss = sum feat.
 * Build: cc -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc/wubuocr gradcheck_real.c \
 *        src/wubuocr/convnet3.c -lm -o /tmp/gc_real
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
    /* REAL config: leak=0.1 (kinks), INORM on (enabled after create), pool ON */
    ConvConfig3 cfg={28,28, 8,3,2, 16,3,2, 32,3,1};  /* pooling ON to match training */
    ConvNet3*cn=convnet3_create(&cfg);
    convnet3_enable_inorm(cn);
    float leak = getenv("CN_LEAK")? (float)atof(getenv("CN_LEAK")) : 0.1f;
    convnet3_set_leak(cn, leak);
    int D=convnet3_dim(cn);
    float*img=malloc(28*28*sizeof(float)),*df=malloc((size_t)D*sizeof(float));
    uint32_t rs=99; fill(img,28*28,&rs,0.5f); fill(df,D,&rs,0.7f);
    /* DUMP net state for external numeric reference (ConvNet3 is opaque) */
    {
        ConvLayer3 L;
        printf("DUMP dims inH=28 inW=28 K1=8 S1=3 K2=16 S2=3 K3=32 S3=3 fdim=%d\n", D);
        L=convnet3_layer(cn,0); printf("w1"); for(int i=0;i<L.n;i++) printf(" %.6f",L.param[i]); printf("\n");
        L=convnet3_layer(cn,2); printf("w2"); for(int i=0;i<L.n;i++) printf(" %.6f",L.param[i]); printf("\n");
        L=convnet3_layer(cn,4); printf("w3"); for(int i=0;i<L.n;i++) printf(" %.6f",L.param[i]); printf("\n");
        printf("img"); for(int i=0;i<28*28;i++) printf(" %.6f",img[i]); printf("\n");
        printf("df"); for(int i=0;i<D;i++) printf(" %.6f",df[i]); printf("\n");
    }
    float*feat=malloc((size_t)D*sizeof(float));
    convnet3_forward(cn,img,feat); 
    if(getenv("DUMP_FWD")){
        FILE*f=fopen("/tmp/fwd_c.txt","w");
        float*c3b=malloc((size_t)D*sizeof(float));
        convnet3_dbg_c(cn,3,c3b);
        fprintf(f,"c3");
        for(int i=0;i<D;i++) fprintf(f," %.6f",c3b[i]);
        fprintf(f,"\n"); free(c3b); fclose(f);
    }
    convnet3_zero_grad(cn); convnet3_backward(cn,img,feat,df);
    { ConvLayer3 L; L=convnet3_layer(cn,0); printf("gw1"); for(int i=0;i<L.n;i++) printf(" %.6f",L.grad[i]); printf("\n");
      L=convnet3_layer(cn,2); printf("gw2"); for(int i=0;i<L.n;i++) printf(" %.6f",L.grad[i]); printf("\n");
      L=convnet3_layer(cn,4); printf("gw3"); for(int i=0;i<L.n;i++) printf(" %.6f",L.grad[i]); printf("\n"); }
    /* TEMP: also dump dc3 + inorm grads via accessor (exact float32) */
    { ConvLayer3 L;
      L=convnet3_layer(cn,10); int n=L.n; printf("dga3"); for(int i=0;i<n;i++) printf(" %.6f",L.grad[i]); printf("\n");
      L=convnet3_layer(cn,11); n=L.n; printf("dbe3"); for(int i=0;i<n;i++) printf(" %.6f",L.grad[i]); printf("\n");
      /* dc3 via a fresh accessor? not exposed; approximate via layer 4 grad is gw3. Instead dump dc3 from DUMP_DC file. */ }
    const double h = getenv("GC_H")? atof(getenv("GC_H")) : 1e-4;   /* small eps => exact on piecewise-linear net */
    int nl=convnet3_layer_count(cn); int worst=-1; double we=0; int tot=0;
    for(int L=0;L<nl;L++){
        ConvLayer3 ly=convnet3_layer(cn,L); if(ly.n==0)continue;
        int step=ly.n>600?ly.n/300:1; int chk=0,fail=0; double lm2=0;
        for(int i=0;i<ly.n;i+=step){
            float w0=ly.param[i]; ly.param[i]=w0+(float)h; double lp=loss_of(cn,img,df,D);
            ly.param[i]=w0-(float)h; double lm=loss_of(cn,img,df,D); ly.param[i]=w0;
            double num=(lp-lm)/(2*h); double ana=(double)ly.grad[i]; double e=fabs(num-ana);
            if(e>lm2)lm2=e; if(e>5e-2)fail++; chk++;
            if((L==0||L==2||L==4) && (i==0)) printf("  dbg L%d i=%d  ana=%.5f num=%.5f\n",L,i,ana,num);
        }
        printf("layer %d: n=%d checked=%d maxerr=%.4e fails=%d\n",L,ly.n,chk,lm2,fail);
        if(lm2>we){we=lm2;worst=L;} tot+=fail;
    }
    printf("=== REAL-CFG GRADCHECK: worst=%.4e layer %d fails=%d ===\n",we,worst,tot);
    printf(we<5e-2&&tot==0?"PASS\n":"FAIL\n");
    return (we<5e-2&&tot==0)?0:1;
}
