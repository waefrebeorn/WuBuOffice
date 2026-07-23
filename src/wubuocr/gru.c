/* gru.c -- minimal scalar GRU, forward + BPTT (see gru.h).
 *
 * Single layer, optional bidirectional. Gates z (update), r (reset), candidate
 * n. Equations (forward):
 *   z_t = sig(Wz·x_t + Uz·h_{t-1} + Bz)
 *   r_t = sig(Wr·x_t + Ur·h_{t-1} + Br)
 *   n_t = tanh(Wh·x_t + Uh·(r_t ⊙ h_{t-1}) + Bh)
 *   h_t = (1 - z_t) ⊙ h_{t-1} + z_t ⊙ n_t
 *
 * Layout contract: ALL weights (and their grad) live in one flat buffer per
 * direction, laid out by a SINGLE shared offset table (OFFS). The optimizer
 * touches flat param[]/grad[]; forward and backward both index through the same
 * table, so param[i] and grad[i] ALWAYS correspond to the same weight. The
 * original bug was the backward using a different layout than the forward, so
 * gradients landed on the wrong weights. That is now structurally impossible.
 */
#include "gru.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define SIG(x) (1.0f/(1.0f+expf(-(x))))
static inline float mtanh(float x){ if(x>8)return 1; if(x<-8)return -1; return tanhf(x); }

struct GRU {
    int din, hid, bidir, outdim, Tcap;
    /* flat weight store: [fwd block][bwd block] (bwd only if bidir). */
    float *P;
    float *grad;          /* same layout as P */
    float *hf,*hb;        /* hidden caches  Tcap x hid (fwd / bwd) */
    float *zf,*rf;        /* z,r caches     Tcap x hid (fwd) */
    float *zb,*rb;        /* z,r caches     Tcap x hid (bwd) */
    float *xcache;        /* Tcap x din */
};

/* ---- single shared offset table (per direction block) ----
 * Order: Wz, Wr, Wh, Uz, Ur, Uh, Bz, Br, Bh  (3 input, 3 hidden, 3 bias). */
typedef struct { int Wz,Wr,Wh,Uz,Ur,Uh,Bz,Br,Bh,block; } GRUOffs;
static GRUOffs gru_offs(int H,int D){
    GRUOffs o;
    o.Wz=0;            o.Wr=o.Wz+H*D; o.Wh=o.Wr+H*D;
    o.Uz=o.Wh+H*D;     o.Ur=o.Uz+H*H; o.Uh=o.Ur+H*H;
    o.Bz=o.Uh+H*H;     o.Br=o.Bz+H;     o.Bh=o.Br+H;
    o.block=o.Bh+H;
    return o;
}

static uint32_t gru_rng=0x9E3779B9u;
static float gru_rndf(void){ gru_rng^=gru_rng<<13; gru_rng^=gru_rng>>17; gru_rng^=gru_rng<<5; return ((float)(gru_rng&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f; }

/* debug mask (env GRUDBG_MASK): 1=skip Uz carry, 2=skip Uh carry, 4=skip (1-z) carry,
 * 8=skip r-gate carry, 16=print bwd pass1 internals, 32=print fwd internals. */
static int g_gdbg = -1;
static int gru_dbg(void){ if(g_gdbg<0) g_gdbg = getenv("GRUDBG_MASK")? atoi(getenv("GRUDBG_MASK")) : 0; return g_gdbg; }

GRU *gru_create(int din, int hid, int bidir, uint32_t seed){
    if(din<=0||hid<=0) return NULL;
    gru_rng = seed? seed : 0x9E3779B9u;
    GRU *r = calloc(1,sizeof *r); if(!r) return NULL;
    r->din=din; r->hid=hid; r->bidir=bidir; r->outdim=hid*(bidir?2:1); r->Tcap=0;
    int H=hid,D=din;
    GRUOffs o = gru_offs(H,D);
    size_t total=(size_t)o.block*2;   /* fwd + (bwd if needed) */
    float *P=calloc(total,sizeof(float)); if(!P){ free(r); return NULL; }
    r->grad=calloc(total,sizeof(float)); if(!r->grad){ free(P); free(r); return NULL; }
    r->P=P;
    float s=sqrtf(1.0f/(float)(H+D));
    void gu_(float*p,int n,float sc){ for(int i=0;i<n;i++) p[i]=gru_rndf()*sc; }
    gu_(P+o.Wz,3*H*D,s); gu_(P+o.Uz,3*H*H,s);
    if(bidir){ float *Q=P+o.block; gu_(Q+o.Wz,3*H*D,s); gu_(Q+o.Uz,3*H*H,s); }
    for(int j=0;j<H;j++){
        P[o.Bz+j]=0.1f; P[o.Br+j]=0.1f; P[o.Bh+j]=0.1f;
        if(bidir){ float *Q=P+o.block; Q[o.Bz+j]=0.1f; Q[o.Br+j]=0.1f; Q[o.Bh+j]=0.1f; }
    }
    return r;
}

void gru_free(GRU *r){ if(!r)return; free(r->P); free(r->grad);
    free(r->hf); free(r->hb); free(r->zf); free(r->rf); free(r->zb); free(r->rb); free(r->xcache); free(r); }

int  gru_outdim(const GRU *r){ return r?r->outdim:0; }
int  gru_num_params(const GRU *r){ if(!r)return 0; int H=r->hid,D=r->din; return gru_offs(H,D).block*(r->bidir?2:1); }
float *gru_param(GRU *r){ return r? r->P : NULL; }
float *gru_grad(GRU *r){ return r? r->grad : NULL; }
void   gru_zero_grad(GRU *r){ if(r) memset(r->grad,0,gru_num_params(r)*sizeof(float)); }

/* one GRU direction forward; caches z,r,h. dir==0 forward, dir==1 backward. */
static void gru_fwd_dir(GRU *r, int T, const float *x, int dir){
    int H=r->hid,D=r->din;
    GRUOffs o = gru_offs(H,D);
    const float *P = r->P + (dir?o.block:0);
    const float *Wz=P+o.Wz,*Wr=P+o.Wr,*Wh=P+o.Wh;
    const float *Uz=P+o.Uz,*Ur=P+o.Ur,*Uh=P+o.Uh;
    const float *Bz=P+o.Bz,*Br=P+o.Br,*Bh=P+o.Bh;
    float *z=dir?r->zb:r->zf, *rr=dir?r->rb:r->rf, *h=dir?r->hb:r->hf;
    for(int t=0;t<T;t++){
        int ti = dir? (T-1-t):t;
        const float *xt = x+(size_t)ti*D;
        /* pass A: compute z,r for every unit at this timestep (so the
         * candidate below sees only THIS timestep's reset gates, never a
         * stale one from t-1). */
        for(int j=0;j<H;j++){
            float az=0,ar=0;
            for(int i=0;i<D;i++){ az+=Wz[j*D+i]*xt[i]; ar+=Wr[j*D+i]*xt[i]; }
            float hp = (t==0)?0:h[(size_t)(t-1)*H+j];
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; az+=Uz[j*H+k]*pv; ar+=Ur[j*H+k]*pv; }
            az+=Bz[j]; ar+=Br[j];
            z[t*H+j]=SIG(az); rr[t*H+j]=SIG(ar);
        }
        /* pass B: candidate n and hidden h, using the now-complete r[t]. */
        for(int j=0;j<H;j++){
            float zv=z[t*H+j], rv=rr[t*H+j];
            float hp = (t==0)?0:h[(size_t)(t-1)*H+j];
            float ac=0; for(int i=0;i<D;i++) ac+=Wh[j*D+i]*xt[i];
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; ac+=Uh[j*H+k]*(rr[t*H+k]*pv); }
            ac+=Bh[j];
            float hv=mtanh(ac);
            if(gru_dbg()&32) printf("  FWD t=%d j=%d ac=%9.5f hv=%9.5f hp=%9.5f zv=%9.5f rv=%9.5f h=%9.5f\n", t,j,ac,hv,hp,zv,rv,(1.0f-zv)*hp+zv*hv);
            h[t*H+j] = (1.0f-zv)*hp + zv*hv;
        }
    }
}

void gru_forward(GRU *r, int T, const float *x){
    if(!r||T<=0) return;
    if(T>r->Tcap){
        r->Tcap=T;
        r->hf=realloc(r->hf,(size_t)T*r->hid*sizeof(float)); r->hb=realloc(r->hb,(size_t)T*r->hid*sizeof(float));
        r->zf=realloc(r->zf,(size_t)T*r->hid*sizeof(float)); r->rf=realloc(r->rf,(size_t)T*r->hid*sizeof(float));
        r->zb=realloc(r->zb,(size_t)T*r->hid*sizeof(float)); r->rb=realloc(r->rb,(size_t)T*r->hid*sizeof(float));
        r->xcache=realloc(r->xcache,(size_t)T*r->din*sizeof(float));
    }
    memcpy(r->xcache,x,(size_t)T*r->din*sizeof(float));
    gru_fwd_dir(r,T,x,0);
    if(r->bidir) gru_fwd_dir(r,T,x,1);
}

void gru_get_output(const GRU *r, float *y){
    if(!r) return;
    int H=r->hid;
    for(int t=0;t<r->Tcap;t++){
        if(!r->bidir) memcpy(y+(size_t)t*H, r->hf+(size_t)t*H, H*sizeof(float));
        else { memcpy(y+(size_t)t*2*H, r->hf+(size_t)t*H, H*sizeof(float));
               memcpy(y+(size_t)t*2*H+H, r->hb+(size_t)t*H, H*sizeof(float)); }
    }
}

/* BPTT for one GRU direction. Accumulates into grad; writes dL/dx into dx.
 * Indexes EVERYTHING through the shared GRUOffs table, so grad[k] always
 * matches param[k]. Standard GRU backward (see gru.h header for equations). */
static void gru_bwd_dir(GRU *r, int T, const float *dy, float *dx, int dir){
    int gdbg = gru_dbg();
    int H=r->hid,D=r->din;
    GRUOffs o = gru_offs(H,D);
    const float *P = r->P + (dir?o.block:0);
    const float *Wz=P+o.Wz,*Wr=P+o.Wr,*Wh=P+o.Wh;
    const float *Uz=P+o.Uz,*Ur=P+o.Ur,*Uh=P+o.Uh;
    float *g  = r->grad + (dir?o.block:0);
    float *gWz=g+o.Wz,*gWr=g+o.Wr,*gWh=g+o.Wh;
    float *gUz=g+o.Uz,*gUr=g+o.Ur,*gUh=g+o.Uh;
    float *gBz=g+o.Bz,*gBr=g+o.Br,*gBh=g+o.Bh;
    float *z=dir?r->zb:r->zf, *rr=dir?r->rb:r->rf, *h=dir?r->hb:r->hf;
    float *dh = calloc((size_t)T*H,sizeof(float));
    for(int t=0;t<T;t++) for(int j=0;j<H;j++)
        dh[(size_t)t*H+j] = dy[(size_t)t*r->outdim + (dir? H+j : j)];
    float *drg = calloc((size_t)H,sizeof(float));
    for(int t=T-1;t>=0;t--){
        memset(drg,0,(size_t)H*sizeof(float));
        /* pass 1: z/candidate/Uh-channel grads */
        for(int j=0;j<H;j++){
            float hprev = (t==0)?0:h[(size_t)(t-1)*H+j];
            float zv=z[t*H+j], rv=rr[t*H+j];
            int ti = dir? (T-1-t):t;
            const float *xt = r->xcache + (size_t)ti*D;
            /* Recompute the candidate preactivation EXACTLY as the forward did:
             * ac = Wh·x + Uh·(r⊙h_{t-1}) + Bh.  Omitting Wh·x here was the bug
             * that corrupted dna/dnz (and thus Bz/Wz/Bh/Wh grads). */
            float a=0; for(int i=0;i<D;i++) a+=Wh[j*D+i]*xt[i];
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; a+=Uh[j*H+k]*(rr[t*H+k]*pv); }
            a+=P[o.Bh+j];
            float nv=mtanh(a);
            float dht = dh[t*H+j];
            float dnz = dht*(nv - hprev);
            float dnn = dht*zv;
            float dna = dnn*(1.0f-nv*nv);
            float daz = dnz*zv*(1.0f-zv);
            if(gdbg&16) printf("  t=%d j=%d dht=%9.5f nv=%9.5f hprev=%9.5f zv=%9.5f dnz=%9.5f daz=%9.5f\n", t,j,dht,nv,hprev,zv,dnz,daz);
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; drg[k]+=dna*Uh[j*H+k]*pv; }
            for(int i=0;i<D;i++){ gWz[j*D+i]+=daz*xt[i]; gWh[j*D+i]+=dna*xt[i]; }
            gBz[j]+=daz; gBh[j]+=dna;
            for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; float rk=rr[t*H+k]; gUh[j*H+k]+=dna*(rk*pv); gUz[j*H+k]+=daz*pv; }
            for(int i=0;i<D;i++) dx[(size_t)ti*D+i] += daz*Wz[j*D+i] + dna*Wh[j*D+i];
            if(t>0){ if(!(gdbg&4)) dh[(size_t)(t-1)*H+j] += dht*(1.0f-zv);
                     for(int k=0;k<H;k++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k]; float rk=rr[t*H+k];
                         if(!(gdbg&1)) dh[(size_t)(t-1)*H+k] += daz*Uz[j*H+k];
                         if(!(gdbg&2)) dh[(size_t)(t-1)*H+k] += dna*Uh[j*H+k]*rk; } }
        }
        /* pass 2: r-gate grads from drg[k] = dL/dr[k] */
        for(int k=0;k<H;k++){
            float rv=rr[t*H+k];
            float dar = drg[k]*rv*(1.0f-rv);
            int ti = dir? (T-1-t):t;
            const float *xt = r->xcache + (size_t)ti*D;
            for(int i=0;i<D;i++){ gWr[k*D+i]+=dar*xt[i]; dx[(size_t)ti*D+i]+=dar*Wr[k*D+i]; }
            gBr[k]+=dar;
            for(int k2=0;k2<H;k2++){ float pv=(t==0)?0:h[(size_t)(t-1)*H+k2]; gUr[k*H+k2]+=dar*pv;
                if(t>0 && !(gdbg&8)) dh[(size_t)(t-1)*H+k2]+=dar*Ur[k*H+k2]; }
        }
    }
    free(dh); free(drg);
}

void gru_backward(GRU *r, int T, const float *dy, float *dx){
    if(!r||T<=0) return;
    memset(dx,0,(size_t)T*r->din*sizeof(float));
    gru_bwd_dir(r,T,dy,dx,0);
    if(r->bidir) gru_bwd_dir(r,T,dy,dx,1);
}
