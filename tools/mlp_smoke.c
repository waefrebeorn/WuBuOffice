/* mlp_smoke.c -- does the optimizer actually MOVE weights?
 * Two tests:
 *  A) real training path: accumulate grads via mlp_backward_smooth over a
 *     synthetic separable dataset, scale by 1/cnt, Adam-update via mlp_layer().
 *  B) trivial mlp_apply_plain SGD on the same data.
 * If W1[0] changes -> optimizer OK; if frozen -> optimizer write is broken. */
#include "mlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static float rnd(uint32_t *s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return (float)(*s&0xFFFFFF)/(float)0xFFFFFF*2.0f-1.0f;}

static void gen_sample(float *z, int *lab, uint32_t *s){
    int c = (int)(rnd(s)*26.0f); *lab = c;          /* random class */
    for(int i=0;i<256;i++) z[i] = rnd(s)*0.5f + (c==(i%26)?0.6f:0.0f);
}

int main(void){
    MLP *m = mlp_create(256, 64, 64, 26, 0x1234ABCDu);
    float z[256]; int lab; uint32_t rs=99;

    /* ---- TEST A: real training path (Adam) ---- */
    MLPLayer L0 = mlp_layer(m,0);
    float w1_0_start = L0.param[0];
    float mvel[40000]; float mmsso[40000]; /* big enough for W1 only; we only touch layer0 via direct */
    /* simpler: use optimizer buffers sized per-layer via mlp_layer */
    float *v[6], *mm[6];
    for(int g=0;g<6;g++){ MLPLayer L=mlp_layer(m,g); v[g]=calloc(L.n,sizeof(float)); mm[g]=calloc(L.n,sizeof(float)); }
    const float b1=0.9f,b2=0.999f,eps=1e-8f; long tstep=0;
    int N=3000;
    for(int it=0; it<40; it++){
        gen_sample(z,&lab,&rs);
        mlp_zero_grad(m);
        mlp_backward_smooth(m, z, lab, 0.1f);
        mlp_scale_grad(m, 1.0f/1.0f);
        tstep++; float corr=sqrtf(1.0f-powf(b2,(float)tstep))/(1.0f-powf(b1,(float)tstep));
        float lr=0.01f;
        for(int g=0;g<6;g++){ MLPLayer L=mlp_layer(m,g);
            for(int i=0;i<L.n;i++){ float gv=L.grad[i]; v[g][i]=b1*v[g][i]+(1-b1)*gv; mm[g][i]=b2*mm[g][i]+(1-b2)*gv*gv; float uh=v[g][i]/(sqrtf(mm[g][i])+eps); L.param[i]-=lr*corr*uh; } }
    }
    float w1_0_afterA = mlp_layer(m,0).param[0];
    printf("TEST A (Adam via mlp_layer): W1[0] start=%.6f after=%.6f moved=%s\n",
           w1_0_start, w1_0_afterA, (w1_0_afterA!=w1_0_start)?"YES":"NO");

    /* ---- TEST B: plain SGD via mlp_apply_plain ---- */
    MLP *m2 = mlp_create(256,64,64,26,0x1234ABCDu);
    float w1_0_startB = mlp_layer(m2,0).param[0];
    for(int it=0; it<40; it++){ gen_sample(z,&lab,&rs); mlp_train_step(m2,z,lab); mlp_apply_plain(m2,0.05f); }
    float w1_0_afterB = mlp_layer(m2,0).param[0];
    printf("TEST B (mlp_apply_plain)   : W1[0] start=%.6f after=%.6f moved=%s\n",
           w1_0_startB, w1_0_afterB, (w1_0_afterB!=w1_0_startB)?"YES":"NO");

    /* quick accuracy sanity on the synthetic data after A */
    int ok=0, T=200; uint32_t rs2=7;
    for(int i=0;i<T;i++){ gen_sample(z,&lab,&rs2); float sc[26]; mlp_forward(m,z,sc); int best=0; for(int c=1;c<26;c++) if(sc[c]>sc[best])best=c; if(best==lab)ok++; }
    printf("TEST A synthetic train acc ~ %.1f%%\n", 100.0f*ok/T);

    return (w1_0_afterA!=w1_0_start) ? 0 : 1;
}
