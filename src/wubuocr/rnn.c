/* rnn.c -- minimal scalar LSTM, forward + BPTT (see rnn.h).
 *
 * Single layer, optional bidirectional. Gates i,f,g,o (input, forget, cell,
 * output). Cell: c_t = f*c_{t-1} + i*tanh(g); h_t = o*tanh(c_t).
 * Weights: W (input->gate, hid x din), U (hidden->gate, hid x hid), b (bias).
 * Gradient accumulated into a flat buffer; verified by tools/rnn_test.c.
 *
 * NOTE: the GRU implementation has been extracted into its own self-contained
 * module (src/wubuocr/gru.c + gru.h) with a single shared weight/grad offset
 * table, so its param[] and grad[] layouts can never diverge. crnn.c selects
 * between LSTM (default) and GRU (RNN_TYPE=2) via the generic seq_* wrappers.
 * rnn.c is now LSTM-only.
 */
#include "rnn.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SIG(x) (1.0f/(1.0f+expf(-(x))))
static inline float mtanh(float x){ if(x>8)return 1; if(x<-8)return -1; return tanhf(x); }

struct LSTM {
    int din, hid, bidir, outdim, Tcap;
    float *Wfi,*Wff,*Wfg,*Wfo;   /* fwd gates: hid x din */
    float *Ufi,*Uff,*Ufg,*Ufo;   /* fwd: hid x hid */
    float *Bfi,*Bff,*Bfg,*Bfo;   /* fwd bias */
    float *Wbi,*Wbf,*Wbg,*Wbo;   /* bwd gates */
    float *Ubi,*Ubf,*Ubg,*Ubo;
    float *Bbi,*Bbf,*Bbg,*Bbo;
    float *grad;                 /* flat: [fwd block][bwd block], each = W(4*hid*din)+U(4*hid*hid)+b(4*hid) */
    float *cf,*hf,*cb,*hb;       /* caches Tcap x hid */
    float *xcache;               /* Tcap x din */
    int Tcur;                    /* current T (set in lstm_forward) */
};

static uint32_t rng=0x9E3779B9u;
static float rndf(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return ((float)(rng&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f; }

LSTM *lstm_create(int din, int hid, int bidir, uint32_t seed){
    if(din<=0||hid<=0) return NULL;
    rng = seed? seed : 0x9E3779B9u;
    LSTM *r = calloc(1,sizeof *r); if(!r) return NULL;
    r->din=din; r->hid=hid; r->bidir=bidir; r->outdim=hid*(bidir?2:1); r->Tcap=0;
    int H=hid,D=din;
    int block = (4*H*D + 4*H*H + 4*H);
    size_t total = (size_t)block*2;
    float *P = calloc(total,sizeof(float)); if(!P){ free(r); return NULL; }
    r->Wfi=P+0;               r->Wff=r->Wfi+H*D;     r->Wfg=r->Wff+H*D;     r->Wfo=r->Wfg+H*D;
    r->Ufi=r->Wfo+H*D;        r->Uff=r->Ufi+H*H;     r->Ufg=r->Uff+H*H;     r->Ufo=r->Ufg+H*H;
    r->Bfi=r->Ufo+H*H;        r->Bff=r->Bfi+H;       r->Bfg=r->Bff+H;       r->Bfo=r->Bfg+H;
    float *Q = P + block;
    r->Wbi=Q+0;               r->Wbf=r->Wbi+H*D;     r->Wbg=r->Wbf+H*D;     r->Wbo=r->Wbg+H*D;
    r->Ubi=r->Wbo+H*D;        r->Ubf=r->Ubi+H*H;     r->Ubg=r->Ubf+H*H;     r->Ubo=r->Ubg+H*H;
    r->Bbi=r->Ubo+H*H;        r->Bbf=r->Bbi+H;       r->Bbg=r->Bbf+H;       r->Bbo=r->Bbg+H;
    r->grad=calloc(total,sizeof(float)); if(!r->grad){ free(P); free(r); return NULL; }
    float s=sqrtf(1.0f/(float)(H+D));
    void s_uni_(float*p,int n,float sc){ for(int i=0;i<n;i++) p[i]=rndf()*sc; }
    s_uni_(r->Wfi,4*H*D,s); s_uni_(r->Ufi,4*H*H,s);
    s_uni_(r->Wbi,4*H*D,s); s_uni_(r->Ubi,4*H*H,s);
    for(int i=0;i<H;i++){ r->Bff[i]=1.0f; r->Bbf[i]=1.0f; }
    return r;
}

void lstm_free(LSTM *r){ if(!r)return; free(r->Wfi); free(r->grad); free(r->cf); free(r->hf); free(r->cb); free(r->hb); free(r->xcache); free(r); }
int lstm_outdim(const LSTM *r){ return r?r->outdim:0; }
int lstm_num_params(const LSTM *r){ if(!r)return 0; int H=r->hid,D=r->din; return (4*H*D+4*H*H+4*H)*2; }
float *lstm_param(LSTM *r){ return r? r->Wfi : NULL; }
float *lstm_grad(LSTM *r){ return r? r->grad : NULL; }
void lstm_zero_grad(LSTM *r){ if(r) memset(r->grad,0,lstm_num_params(r)*sizeof(float)); }

/* forward one direction; caches c,h. dir=0 fwd (t=0..T-1), dir=1 bwd (reversed). */
static void dir_fwd(LSTM *r, int T, const float *x, int dir){
    int H=r->hid,D=r->din;
    float *Wf=dir?r->Wbi:r->Wfi, *Wff=dir?r->Wbf:r->Wff, *Wfg=dir?r->Wbg:r->Wfg, *Wfo=dir?r->Wbo:r->Wfo;
    float *Uf=dir?r->Ubi:r->Ufi, *Uff=dir?r->Ubf:r->Uff, *Ufg=dir?r->Ubg:r->Ufg, *Ufo=dir?r->Ubo:r->Ufo;
    float *Bf=dir?r->Bbi:r->Bfi, *Bff=dir?r->Bbf:r->Bff, *Bfg=dir?r->Bbg:r->Bfg, *Bfo=dir?r->Bbo:r->Bfo;
    float *c=dir?r->cb:r->cf, *h=dir?r->hb:r->hf;
    for(int t=0;t<T;t++){
        int ti = dir? (T-1-t):t;
        const float *xt = x+(size_t)ti*D;
        for(int j=0;j<H;j++){
            float ai=0,af=0,ag=0,ao=0;
            for(int i=0;i<D;i++){ ai+=Wf[j*D+i]*xt[i]; af+=Wff[j*D+i]*xt[i]; ag+=Wfg[j*D+i]*xt[i]; ao+=Wfo[j*D+i]*xt[i]; }
            for(int k=0;k<H;k++){ float hp=(t==0)?0:h[(size_t)(t-1)*H+k]; ai+=Uf[j*H+k]*hp; af+=Uff[j*H+k]*hp; ag+=Ufg[j*H+k]*hp; ao+=Ufo[j*H+k]*hp; }
            ai+=Bf[j]; af+=Bff[j]; ag+=Bfg[j]; ao+=Bfo[j];
            float ii=SIG(ai), ff=SIG(af), gg=mtanh(ag), oo=SIG(ao);
            float cc = ff*((t==0)?0:c[(size_t)(t-1)*H+j]) + ii*gg;
            c[(size_t)t*H+j]=cc;
            h[(size_t)t*H+j]= oo*mtanh(cc);
        }
    }
}

void lstm_forward(LSTM *r, int T, const float *x){
    if(!r||T<=0) return;
    if(T>r->Tcap){
        r->Tcap=T;
        r->cf=realloc(r->cf,(size_t)T*r->hid*sizeof(float)); r->hf=realloc(r->hf,(size_t)T*r->hid*sizeof(float));
        r->cb=realloc(r->cb,(size_t)T*r->hid*sizeof(float)); r->hb=realloc(r->hb,(size_t)T*r->hid*sizeof(float));
        r->xcache=realloc(r->xcache,(size_t)T*r->din*sizeof(float));
    }
    r->Tcur=T;
    memcpy(r->xcache,x,(size_t)T*r->din*sizeof(float));
    dir_fwd(r,T,x,0);
    if(r->bidir) dir_fwd(r,T,x,1);
}

void lstm_get_output(const LSTM *r, float *y){
    if(!r) return;
    int H=r->hid; int T=r->Tcur;
    for(int t=0;t<T;t++){
        if(!r->bidir) memcpy(y+(size_t)t*H, r->hf+(size_t)t*H, H*sizeof(float));
        else { memcpy(y+(size_t)t*2*H, r->hf+(size_t)t*H, H*sizeof(float));
               memcpy(y+(size_t)t*2*H+H, r->hb+(size_t)t*H, H*sizeof(float)); }
    }
}

/* BPTT for one direction; accumulates weight grads into g*; accumulates dL/dx into dx. */
static void dir_bwd(LSTM *r, int T, const float *dy, float *dx, int dir){
    int H=r->hid,D=r->din;
    float *Wf=dir?r->Wbi:r->Wfi, *Wff=dir?r->Wbf:r->Wff, *Wfg=dir?r->Wbg:r->Wfg, *Wfo=dir?r->Wbo:r->Wfo;
    float *Uf=dir?r->Ubi:r->Ufi, *Uff=dir?r->Ubf:r->Uff, *Ufg=dir?r->Ubg:r->Ufg, *Ufo=dir?r->Ubo:r->Ufo;
    float *Bf=dir?r->Bbi:r->Bfi, *Bff=dir?r->Bbf:r->Bff, *Bfg=dir?r->Bbg:r->Bfg, *Bfo=dir?r->Bbo:r->Bfo;
    float *c=dir?r->cb:r->cf, *h=dir?r->hb:r->hf;
    int block = 4*H*D+4*H*H+4*H;
    float *G = r->grad + (dir? block : 0);
    float *gWf=G+0, *gWff=gWf+H*D, *gWfg=gWff+H*D, *gWfo=gWfg+H*D;
    float *gUf=gWfo+H*D, *gUff=gUf+H*H, *gUfg=gUff+H*H, *gUfo=gUfg+H*H;
    float *gBf=gUfo+H*H, *gBff=gBf+H, *gBfg=gBff+H, *gBfo=gBfg+H;
    /* dh[t] gradient of output (from dy). For bwd dir, dy index uses reversed t. */
    float *dh = malloc((size_t)T*H*sizeof(float)); memset(dh,0,(size_t)T*H*sizeof(float));
    for(int t=0;t<T;t++){
        for(int j=0;j<H;j++) dh[(size_t)t*H+j] = dy[(size_t)t*r->outdim + (dir? H+j : j)];
    }
    float *dc = calloc(H,sizeof(float));       /* dL/d c_t carried to prev step */
    for(int t=T-1;t>=0;t--){
        for(int j=0;j<H;j++){
            const float *xt = r->xcache + (size_t)(dir? (T-1-t) : t)*D;
            float ai=0,af=0,ag=0,ao=0;
            for(int i=0;i<D;i++){ ai+=Wf[j*D+i]*xt[i]; af+=Wff[j*D+i]*xt[i]; ag+=Wfg[j*D+i]*xt[i]; ao+=Wfo[j*D+i]*xt[i]; }
            for(int k=0;k<H;k++){ float hp=(t==0)?0:h[(size_t)(t-1)*H+k]; ai+=Uf[j*H+k]*hp; af+=Uff[j*H+k]*hp; ag+=Ufg[j*H+k]*hp; ao+=Ufo[j*H+k]*hp; }
            ai+=Bf[j]; af+=Bff[j]; ag+=Bfg[j]; ao+=Bfo[j];
            float ii=SIG(ai), ff=SIG(af), gg=mtanh(ag), oo=SIG(ao);
            float tanhc=mtanh(c[(size_t)t*H+j]);
            float cprev = (t==0)? 0 : c[(size_t)(t-1)*H+j];
            float dho=dh[(size_t)t*H+j];
            /* dL/d c_t from the h-output path: h=o*tanh(c) */
            float dcc = dho*oo*(1.0f-tanhc*tanhc) + dc[j];
            /* gate pre-activation gradients (standard LSTM) */
            float dno = dho * tanhc * oo*(1.0f-oo);     /* output gate   */
            float dni = dcc * gg * ii*(1.0f-ii);        /* input gate    */
            float dnf = dcc * cprev * ff*(1.0f-ff);     /* forget gate   */
            float dng = dcc * ii * (1.0f-gg*gg);        /* cell input g  */
            /* weight grads (x and h contributions) */
            for(int i=0;i<D;i++){ gWf[j*D+i]+=dni*xt[i]; gWff[j*D+i]+=dnf*xt[i]; gWfg[j*D+i]+=dng*xt[i]; gWfo[j*D+i]+=dno*xt[i]; }
            for(int k=0;k<H;k++){ float hp=(t==0)?0:h[(size_t)(t-1)*H+k]; gUf[j*H+k]+=dni*hp; gUff[j*H+k]+=dnf*hp; gUfg[j*H+k]+=dng*hp; gUfo[j*H+k]+=dno*hp; }
            gBf[j]+=dni; gBff[j]+=dnf; gBfg[j]+=dng; gBfo[j]+=dno;
            /* dx for this step (source-ordered index for this dir) */
            int src = dir? (T-1-t) : t;
            for(int i=0;i<D;i++) dx[(size_t)src*D+i] += Wf[j*D+i]*dni + Wff[j*D+i]*dnf + Wfg[j*D+i]*dng + Wfo[j*D+i]*dno;
            /* carry dh to prev LSTM-time step */
            if(t>0 && !getenv("RNN_NOHC")) for(int k=0;k<H;k++) dh[(size_t)(t-1)*H+k] += Uf[j*H+k]*dni + Uff[j*H+k]*dnf + Ufg[j*H+k]*dng + Ufo[j*H+k]*dno;
            /* carry dc to prev step: c_t = f*c_{t-1}+i*g, so dL/d c_{t-1} = f*dcc */
            if(t>0) dc[j] = ff * dcc;
        }
    }
    free(dh); free(dc);
}

void lstm_backward(LSTM *r, int T, const float *dy, float *dx){
    if(!r||T<=0) return;
    memset(dx,0,(size_t)T*r->din*sizeof(float));
    dir_bwd(r,T,dy,dx,0);
    if(r->bidir) dir_bwd(r,T,dy,dx,1);
}
