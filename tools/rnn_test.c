/* rnn_test.c -- finite-difference check of LSTM forward+backward.
 * We chain LSTM -> a small linear head -> scalar loss = sum(out^2), and verify
 * (a) the analytic gradient w.r.t. inputs/weights matches central differences.
 */
#include "rnn.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static float rnd(uint32_t*s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return ((float)(*s&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f;}

/* head: W (outdim x hid_total) + b ; loss = sum y^2 */
static int DIN=3, HID=4, TSTEPS=5;
static LSTM *R;
static float *Whead, *bhead; /* outdim x (2*HID if bidir) */
static int OUT;

static float head_out_buf[256];
static float loss_of(const float *xin, const float *w, const float *b){
    lstm_forward(R, TSTEPS, xin);
    lstm_get_output(R, head_out_buf);
    float s=0;
    for(int t=0;t<TSTEPS;t++) for(int j=0;j<OUT;j++){
        float acc=b[j]; for(int k=0;k<OUT;k++) acc+=0; /* placeholder */
        (void)acc;
    }
    s=0;
    for(int t=0;t<TSTEPS;t++) for(int j=0;j<OUT;j++){
        float o = b[j];
        for(int k=0;k<OUT;k++) o += w[j*OUT+k]*head_out_buf[t*OUT+k];
        s += o*o;
    }
    return s;
}

int main(void){
    int bidir=1;
    R = lstm_create(DIN, HID, bidir, 12345);
    OUT = lstm_outdim(R);
    Whead = malloc((size_t)OUT*OUT*sizeof(float));
    bhead = malloc(OUT*sizeof(float));
    uint32_t rs=7;
    for(int i=0;i<OUT*OUT;i++) Whead[i]=rnd(&rs)*0.3f;
    for(int i=0;i<OUT;i++) bhead[i]=rnd(&rs)*0.3f;

    float *x = malloc((size_t)TSTEPS*DIN*sizeof(float));
    for(int i=0;i<TSTEPS*DIN;i++) x[i]=rnd(&rs);

    /* gradcheck over a sample of inputs and weights */
    int fails=0, checked=0;
    float h=1e-3f;
    /* inputs */
    for(int s=0;s<8;s++){
        int idx = (s*37)%(TSTEPS*DIN);
        float save=x[idx];
        x[idx]=save+h; float lp=loss_of(x,Whead,bhead);
        x[idx]=save-h; float lm=loss_of(x,Whead,bhead);
        x[idx]=save;
        float num=(lp-lm)/(2*h);
        /* analytic: run forward+backward to get dx */
        lstm_forward(R,TSTEPS,x);
        lstm_get_output(R,head_out_buf);
        float *dy=malloc((size_t)TSTEPS*OUT*sizeof(float));
        /* build dy from head gradient: dL/dy[t][j] = 2*o*b_j? no: o = b_j+sum w_jk y_k; dL/do_j=2o_j;
           but head maps y->o_j=b_j+sum_k w_jk y_{tk}; dL/dy_{tk} = sum_j 2 o_j w_jk */
        float *ybuf=malloc((size_t)TSTEPS*OUT*sizeof(float));
        memcpy(ybuf,head_out_buf,(size_t)TSTEPS*OUT*sizeof(float));
        for(int t=0;t<TSTEPS;t++) for(int k=0;k<OUT;k++){
            float acc=0; for(int j=0;j<OUT;j++){
                float o=bhead[j]; for(int kk=0;kk<OUT;kk++) o+=Whead[j*OUT+kk]*ybuf[t*OUT+kk];
                acc += 2*o*Whead[j*OUT+k];
            }
            dy[t*OUT+k]=acc;
        }
        float *dx=malloc((size_t)TSTEPS*DIN*sizeof(float));
        lstm_zero_grad(R);
        lstm_backward(R,TSTEPS,dy,dx);
        float ana=dx[idx];
        free(dy); free(dx); free(ybuf);
        float rel=fabsf(num-ana)/((fabsf(num)+fabsf(ana)+1e-5f));
        if(fabsf(num)<1e-3f && fabsf(ana)<1e-3f) continue;  /* skip ~zero grads (FD noise) */
        if(rel>0.05f){ if(fails<6) printf("  input idx=%d num=%.5f ana=%.5f rel=%.3f\n",idx,num,ana,rel); fails++; }
        checked++;
    }
    /* weights: check a few via perturbing lstm_param directly */
    float *P=lstm_param(R); int nP=lstm_num_params(R);
    for(int s=0;s<8;s++){
        int idx=(s*101)%nP;
        float save=P[idx];
        P[idx]=save+h; lstm_zero_grad(R); float lp=loss_of(x,Whead,bhead);
        P[idx]=save-h; lstm_zero_grad(R); float lm=loss_of(x,Whead,bhead);
        P[idx]=save;
        float num=(lp-lm)/(2*h);
        lstm_forward(R,TSTEPS,x); lstm_get_output(R,head_out_buf);
        float *dy=malloc((size_t)TSTEPS*OUT*sizeof(float));
        float *ybuf=malloc((size_t)TSTEPS*OUT*sizeof(float)); memcpy(ybuf,head_out_buf,(size_t)TSTEPS*OUT*sizeof(float));
        for(int t=0;t<TSTEPS;t++) for(int k=0;k<OUT;k++){ float acc=0; for(int j=0;j<OUT;j++){ float o=bhead[j]; for(int kk=0;kk<OUT;kk++)o+=Whead[j*OUT+kk]*ybuf[t*OUT+kk]; acc+=2*o*Whead[j*OUT+k]; } dy[t*OUT+k]=acc; }
        float *dx=malloc((size_t)TSTEPS*DIN*sizeof(float));
        lstm_zero_grad(R); lstm_backward(R,TSTEPS,dy,dx);
        float ana=lstm_grad(R)[idx];
        free(dy); free(dx); free(ybuf);
        float rel=fabsf(num-ana)/((fabsf(num)+fabsf(ana)+1e-5f));
        if(fabsf(num)<1e-3f && fabsf(ana)<1e-3f) continue;  /* skip ~zero grads (FD noise) */
        if(rel>0.05f){ if(fails<12) printf("  weight idx=%d num=%.5f ana=%.5f rel=%.3f\n",idx,num,ana,rel); fails++; }
        checked++;
    }
    printf("LSTM gradcheck: checked=%d fails(>5%%)=%d\n", checked, fails);
    printf("%s\n", fails==0?"LSTM TESTS PASSED":"LSTM TESTS FAILED");
    free(Whead); free(bhead); free(x); lstm_free(R);
    return fails==0?0:1;
}
