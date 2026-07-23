/* rnn_dx.c -- minimal, unambiguous dx check. loss = sum_t ||h(t)||^2 (fwd only). */
#include "rnn.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
static float rnd(uint32_t*s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return ((float)(*s&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f;}

static int DIN=3, HID=4, TSTEPS=5, BIDIR=0;
static LSTM *R;
static float cache[256];

static float loss_of(const float*x){
    lstm_forward(R,TSTEPS,x);
    lstm_get_output(R,cache);
    float s=0; int od=lstm_outdim(R);
    for(int t=0;t<TSTEPS;t++) for(int j=0;j<od;j++){ float v=cache[t*od+j]; s+=v*v; }
    return s;
}

int main(void){
    R=lstm_create(DIN,HID,BIDIR,99);
    float *x=malloc((size_t)TSTEPS*DIN*sizeof(float));
    uint32_t rs=3; for(int i=0;i<TSTEPS*DIN;i++) x[i]=rnd(&rs);
    int od=lstm_outdim(R);
    float *dy=malloc((size_t)TSTEPS*od*sizeof(float));
    float *dx=malloc((size_t)TSTEPS*DIN*sizeof(float));
    /* dy = 2*h (since loss=sum h^2, dL/dh=2h) */
    lstm_forward(R,TSTEPS,x); lstm_get_output(R,cache);
    for(int t=0;t<TSTEPS;t++) for(int j=0;j<od;j++) dy[t*od+j]=2.0f*cache[t*od+j];

    lstm_zero_grad(R); lstm_backward(R,TSTEPS,dy,dx);

    int fails=0; float h=1e-3f;
    for(int idx=0; idx<TSTEPS*DIN; idx++){
        float save=x[idx];
        x[idx]=save+h; float lp=loss_of(x);
        x[idx]=save-h; float lm=loss_of(x);
        x[idx]=save;
        float num=(lp-lm)/(2*h);
        float ana=dx[idx];
        float rel=fabsf(num-ana)/((fabsf(num)+fabsf(ana)+1e-5f));
        if(rel>0.02f){ if(fails<12) printf("  dx idx=%d num=%.5f ana=%.5f rel=%.3f\n",idx,num,ana,rel); fails++; }
    }
    printf("DX gradcheck (bidir=%d) fails(>2%%)=%d\n", BIDIR, fails);
    return fails?1:0;
}
