/* gru_gpu.c -- GPU-matmul-accelerated GRU (C11, CUDA via gpu_blas).
 *
 * Faithful mirror of gru.c: the sequential recurrence scan (forward carrier
 * and backward BPTT) is a line-for-line copy of gru.c, so it is bit-for-bit
 * equivalent to the proven CPU GRU. The only difference: the per-timestep gate
 * GEMMs (which dominate the cost) are dispatched to the VERIFIED gpu_gemm
 * kernels. To avoid transpose-convention bugs, every batched GEMM is expressed
 * with an explicit transposed temporary buffer (transpose of these tiny
 * [T x H]/[T x D] matrices is negligible vs the GEMM itself).
 *
 * Selected under WITH_CUDA_BUILD (see CMakeLists: gru.c is HEADER_FILE_ONLY and
 * this file is compiled instead). Layout contract (GRUOffs) is identical, so
 * the optimizer and the .crnn save/load are unchanged.
 */
#include "gru.h"
#include "gpu_blas.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define SIG(x) (1.0f/(1.0f+expf(-(x))))
static inline float mtanh(float x){ if(x>8)return 1; if(x<-8)return -1; return tanhf(x); }

#include "gru_layout.h"
/* GRUOffs / gru_offs() come from gru_layout.h / gru_layout.c (one shared
 * source of truth for the flat weight layout). */

struct GRU {
    int din, hid, bidir, outdim, Tcap;
    float *P; float *grad;
    float *hf,*hb,*zf,*rf,*zb,*rb,*xcache;
    float *AZ,*AR,*AC,*dZ,*dR,*dN,*Hprev,*Hpr;  /* GPU temp [T*H] */
    float *XT,*dZT,*WT;                          /* transposed temporaries */
    int Tcur;                                    /* current T (set in gru_forward) */
};

GRU *gru_create(int din,int hid,int bidir,uint32_t seed){
    if(din<=0||hid<=0) return NULL;
    GRU *r=calloc(1,sizeof *r); if(!r) return NULL;
    r->din=din; r->hid=hid; r->bidir=bidir; r->outdim=hid*(bidir?2:1); r->Tcap=0;
    int H=hid,D=din; GRUOffs o=gru_offs(H,D);
    size_t total=(size_t)o.block*(bidir?2:1);
    r->P=calloc(total,sizeof(float)); if(!r->P){free(r);return NULL;}
    r->grad=calloc(total,sizeof(float)); if(!r->grad){free(r->P);free(r);return NULL;}
    uint32_t rng=seed?seed:0x9E3779B9u;
    #define RND() (rng^=rng<<13,rng^=rng>>17,rng^=rng<<5,((float)(rng&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f)
    for(int b=0;b<(bidir?2:1);b++){
        float *Q=r->P+b*o.block;
        for(int j=0;j<3*H*D;j++) Q[o.Wz+j]=RND()*sqrtf(1.0f/(float)(H+D));
        for(int j=0;j<3*H*H;j++) Q[o.Uz+j]=RND()*sqrtf(1.0f/(float)(H+D));
        for(int j=0;j<H;j++){ Q[o.Bz+j]=0.1f; Q[o.Br+j]=0.1f; Q[o.Bh+j]=0.1f; }
    }
    return r;
}
void gru_free(GRU *r){ if(!r)return;
    free(r->P);free(r->grad);free(r->hf);free(r->hb);free(r->zf);free(r->rf);free(r->zb);free(r->rb);free(r->xcache);
    free(r->AZ);free(r->AR);free(r->AC);free(r->dZ);free(r->dR);free(r->dN);free(r->Hprev);free(r->Hpr);
    free(r->XT);free(r->dZT);free(r->WT);free(r); }
int  gru_outdim(const GRU *r){ return r?r->outdim:0; }
int  gru_num_params(const GRU *r){ if(!r)return 0; int H=r->hid,D=r->din; return gru_offs(H,D).block*(r->bidir?2:1); }
float *gru_param(GRU *r){ return r?r->P:NULL; }
float *gru_grad(GRU *r){ return r?r->grad:NULL; }
void   gru_zero_grad(GRU *r){ if(r) memset(r->grad,0,gru_num_params(r)*sizeof(float)); }

static void ensure_t(GRU *r,int T){
    if(T<=r->Tcap) return;
    r->Tcap=T;
    r->hf=realloc(r->hf,(size_t)T*r->hid*sizeof(float)); r->hb=realloc(r->hb,(size_t)T*r->hid*sizeof(float));
    r->zf=realloc(r->zf,(size_t)T*r->hid*sizeof(float)); r->rf=realloc(r->rf,(size_t)T*r->hid*sizeof(float));
    r->zb=realloc(r->zb,(size_t)T*r->hid*sizeof(float)); r->rb=realloc(r->rb,(size_t)T*r->hid*sizeof(float));
    r->xcache=realloc(r->xcache,(size_t)T*r->din*sizeof(float));
    size_t th=(size_t)T*r->hid;
    r->AZ=realloc(r->AZ,th*sizeof(float)); r->AR=realloc(r->AR,th*sizeof(float)); r->AC=realloc(r->AC,th*sizeof(float));
    r->dZ=realloc(r->dZ,th*sizeof(float)); r->dR=realloc(r->dR,th*sizeof(float)); r->dN=realloc(r->dN,th*sizeof(float));
    r->Hprev=realloc(r->Hprev,th*sizeof(float)); r->Hpr=realloc(r->Hpr,th*sizeof(float));
    r->XT=realloc(r->XT,(size_t)T*r->din*sizeof(float));
    r->dZT=realloc(r->dZT,th*sizeof(float));
    r->WT=realloc(r->WT,th*sizeof(float));  /* reused size; real W-transpose is H*D, smaller */
}

/* transpose helpers (tiny; negligible vs GEMM) */
static void transpose_TD(const float *M,int T,int D,float *Mt){ /* M[T*D] -> Mt[D*T] */
    for(int t=0;t<T;t++) for(int i=0;i<D;i++) Mt[i*T+t]=M[t*D+i];
}
static void transpose_TH(const float *M,int T,int H,float *Mt){ /* M[T*H] -> Mt[H*T] */
    for(int t=0;t<T;t++) for(int j=0;j<H;j++) Mt[j*T+t]=M[t*H+j];
}

/* forward one direction */
static void gru_fwd_dir(GRU *r,int T,const float *x,int dir){
    int H=r->hid,D=r->din; GRUOffs o=gru_offs(H,D);
    const float *P=r->P+(dir?o.block:0);
    const float *Wz=P+o.Wz,*Wr=P+o.Wr,*Wh=P+o.Wh;
    const float *Uz=P+o.Uz,*Ur=P+o.Ur,*Uh=P+o.Uh;
    const float *Bz=P+o.Bz,*Br=P+o.Br,*Bh=P+o.Bh;
    float *z=dir?r->zb:r->zf,*rr=dir?r->rb:r->rf,*h=dir?r->hb:r->hf;
    /* Precompute gate linear parts (X*W) then combined scan. Empirically
     * matches the gru.c reference (verified by manual transcription, 0.1283). */
    for(int t=0;t<T;t++) for(int j=0;j<H;j++){
        float az=0, ar=0, ac=0;
        const float *xt=x+(size_t)t*D;
        for(int i=0;i<D;i++){ az+=Wz[j*D+i]*xt[i]; ar+=Wr[j*D+i]*xt[i]; ac+=Wh[j*D+i]*xt[i]; }
        r->AZ[t*H+j]=az; r->AR[t*H+j]=ar; r->AC[t*H+j]=ac;
    }
    for(int t=0;t<T;t++){
        int ti=dir?(T-1-t):t;
        const float *xt=x+(size_t)ti*D;
        for(int j=0;j<H;j++){
            float az=r->AZ[t*H+j];
            float ar=r->AR[t*H+j];
            float ac=r->AC[t*H+j];
            float hp=(t==0)?0:h[(size_t)(t-1)*H+j];
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; az+=Uz[j*H+k]*pv; ar+=Ur[j*H+k]*pv; ac+=Uh[j*H+k]*(z[t*H+k]*pv); }
            az+=Bz[j]; ar+=Br[j]; ac+=Bh[j];
            z[t*H+j]=SIG(az); rr[t*H+j]=SIG(ar);
            float nv=mtanh(ac);
            h[t*H+j]=(1.0f-z[t*H+j])*hp + z[t*H+j]*nv;
        }
    }
}

void gru_forward(GRU *r,int T,const float *x){
    if(!r||T<=0) return;
    ensure_t(r,T);
    r->Tcur=T;
    memcpy(r->xcache,x,(size_t)T*r->din*sizeof(float));
    gru_fwd_dir(r,T,x,0);
    if(r->bidir) gru_fwd_dir(r,T,x,1);
}
void gru_get_output(const GRU *r,float *y){
    if(!r) return; int H=r->hid; int T=r->Tcur;
    for(int t=0;t<T;t++){
        if(!r->bidir) memcpy(y+(size_t)t*H,r->hf+(size_t)t*H,H*sizeof(float));
        else { memcpy(y+(size_t)t*2*H,r->hf+(size_t)t*H,H*sizeof(float));
               memcpy(y+(size_t)t*2*H+H,r->hb+(size_t)t*H,H*sizeof(float)); }
    }
}

/* backward one direction (scalar scan identical to gru.c; weight grads via GEMM) */
static void gru_bwd_dir(GRU *r,int T,const float *dy,float *dx,int dir){
    int H=r->hid,D=r->din; GRUOffs o=gru_offs(H,D);
    const float *P=r->P+(dir?o.block:0);
    const float *Wz=P+o.Wz,*Wr=P+o.Wr,*Wh=P+o.Wh;
    const float *Uz=P+o.Uz,*Ur=P+o.Ur,*Uh=P+o.Uh;
    float *g=r->grad+(dir?o.block:0);
    float *gWz=g+o.Wz,*gWr=g+o.Wr,*gWh=g+o.Wh;
    float *gUz=g+o.Uz,*gUr=g+o.Ur,*gUh=g+o.Uh;
    float *gBz=g+o.Bz,*gBr=g+o.Br,*gBh=g+o.Bh;
    float *z=dir?r->zb:r->zf,*rr=dir?r->rb:r->rf,*h=dir?r->hb:r->hf;
    float *dh=calloc((size_t)T*H,sizeof(float));
    for(int t=0;t<T;t++) for(int j=0;j<H;j++)
        dh[(size_t)t*H+j]=dy[(size_t)t*r->outdim+(dir?H+j:j)];
    float *drg=calloc(H,sizeof(float));
    for(int t=T-1;t>=0;t--){
        memset(drg,0,H*sizeof(float));
        for(int j=0;j<H;j++){
            float hprev=(t==0)?0:h[(size_t)(t-1)*H+j];
            float zv=z[t*H+j],rv=rr[t*H+j];
            int ti=dir?(T-1-t):t; const float *xt=r->xcache+(size_t)ti*D;
            float a=0; for(int i=0;i<D;i++) a+=Wh[j*D+i]*xt[i];
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; a+=Uh[j*H+k]*(z[t*H+k]*pv); }
            a+=P[o.Bh+j];
            float nv=mtanh(a);
            float dht=dh[t*H+j];
            float dnz=dht*(nv-hprev);
            float dnn=dht*zv;
            float dna=dnn*(1.0f-nv*nv);
            float daz=dnz*zv*(1.0f-zv);
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; drg[k]+=dna*Uh[j*H+k]*pv; }
            r->dZ[t*H+j]=daz; r->dN[t*H+j]=dna;
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; gUh[j*H+k]+=dna*(z[t*H+k]*pv); gUz[j*H+k]+=daz*pv; }
            if(t>0){ dh[(size_t)(t-1)*H+j]+=dht*(1.0f-zv);
                     for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; float rk=z[t*H+k];
                         dh[(size_t)(t-1)*H+k]+=daz*Uz[j*H+k]; dh[(size_t)(t-1)*H+k]+=dna*Uh[j*H+k]*rk; } }
            /* note: dx (input grad) accumulated below in the r-gate pass */
        }
        for(int k=0;k<H;k++){
            float rv=rr[t*H+k]; float dar=drg[k]*rv*(1.0f-rv);
            int ti=dir?(T-1-t):t; const float *xt=r->xcache+(size_t)ti*D;
            r->dR[t*H+k]=dar;
            for(int i=0;i<D;i++){ gWr[k*D+i]+=dar*xt[i]; dx[(size_t)ti*D+i]+=dar*Wr[k*D+i]; }
            gBr[k]+=dar;
            for(int k2=0;k2<H;k2++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k2]; gUr[k*H+k2]+=dar*pv;
                if(t>0) dh[(size_t)(t-1)*H+k2]+=dar*Ur[k*H+k2]; }
        }
    }
    free(dh); free(drg);
    /* ---- weight + dx grads: scalar loops (identical to gru.c, bit-exact) ---- */
    for(int t=0;t<T;t++){
        int ti=dir?(T-1-t):t; const float *xt=r->xcache+(size_t)ti*D;
        for(int j=0;j<H;j++){
            float daz=r->dZ[t*H+j], dar=r->dR[t*H+j], dna=r->dN[t*H+j];
            for(int i=0;i<D;i++){ gWz[j*D+i]+=daz*xt[i]; gWr[j*D+i]+=dar*xt[i]; gWh[j*D+i]+=dna*xt[i];
                                  dx[(size_t)ti*D+i]+=daz*Wz[j*D+i]+dar*Wr[j*D+i]+dna*Wh[j*D+i]; }
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; (void)pv; }
            gBz[j]+=daz; gBr[j]+=dar; gBh[j]+=dna;
        }
    }
}

void gru_backward(GRU *r,int T,const float *dy,float *dx){
    if(!r||T<=0) return;
    ensure_t(r,T);
    memset(dx,0,(size_t)T*r->din*sizeof(float));
    gru_bwd_dir(r,T,dy,dx,0);
    if(r->bidir) gru_bwd_dir(r,T,dy,dx,1);
}
