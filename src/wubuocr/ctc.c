/* ctc.c -- CTC forward-backward + gradient (see ctc.h). */
#include "ctc.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define MAXT 4096
#define MAXS 256
#define MAXC 96        /* max CTC alphabet size (blank + classes); raised from 64
                         * so multi-script / full-document charsets (e.g. 69-class
                         * Latin a-z A-Z 0-9 + punctuation) fit without UB. */

static float prob[MAXT][MAXC];     /* softmax per step (C<=MAXC) */
static float alpha[MAXT][MAXS];
static float beta[MAXT][MAXS];

static int lab(int s, const int *tgt, int L){
    (void)L;
    return (s & 1) ? tgt[(s-1)/2] : 0;  /* even=blank(0), odd=target class */
}

float ctc_loss(int T, int C, int L, const int *target,
               const float *logits, float *grad, float smooth, float focal){
    if(T<=0||C<=1||L<0||L>MAXS/2) return 1e30f;
    int S = 2*L+1;
    if(S>MAXS || C>MAXC || T>MAXT) return 1e30f;
    /* softmax */
    for(int t=0;t<T;t++){
        float mx=logits[t*C]; for(int c=1;c<C;c++) if(logits[t*C+c]>mx) mx=logits[t*C+c];
        float s=0; for(int c=0;c<C;c++){ prob[t][c]=expf(logits[t*C+c]-mx); s+=prob[t][c]; }
        for(int c=0;c<C;c++) prob[t][c]/=s;
    }
    /* forward */
    for(int s=0;s<S;s++) alpha[0][s]=0;
    if(S>=1) alpha[0][0] = (lab(0,target,L)==0)? prob[0][0] : 0;
    if(S>=2 && L>0) alpha[0][1] = (lab(1,target,L)==target[0])? prob[0][target[0]] : 0;
    for(int t=1;t<T;t++){
        for(int s=0;s<S;s++){
            float a=alpha[t-1][s];
            if(s-1>=0) a+=alpha[t-1][s-1];
            if(s-2>=0 && lab(s,target,L)!=0 && lab(s,target,L)!=lab(s-2,target,L)) a+=alpha[t-1][s-2];
            alpha[t][s]=a*prob[t][lab(s,target,L)];
        }
    }
    float ll = alpha[T-1][S-1];
    if(L>0 && S>=2) ll += alpha[T-1][S-2];
    if(ll<1e-30f) ll=1e-30f;
    float loss = -logf(ll);

    /* backward */
    for(int s=0;s<S;s++) beta[T-1][s]=0;
    if(S>=1) beta[T-1][S-1]=1;
    if(S>=2 && L>0) beta[T-1][S-2]=1;
    for(int t=T-2;t>=0;t--){
        for(int s=0;s<S;s++){
            float b=beta[t+1][s]*prob[t+1][lab(s,target,L)];
            if(s+1<S) b+=beta[t+1][s+1]*prob[t+1][lab(s+1,target,L)];
            if(s+2<S && lab(s+2,target,L)!=0 && lab(s+2,target,L)!=lab(s,target,L)) b+=beta[t+1][s+2]*prob[t+1][lab(s+2,target,L)];
            beta[t][s]=b;
        }
    }
    /* gradient per logit */
    for(int t=0;t<T;t++){
        float denom=0; float acc[MAXC]; for(int c=0;c<C;c++) acc[c]=0;
        for(int s=0;s<S;s++){ float p=alpha[t][s]*beta[t][s]; denom+=p; acc[lab(s,target,L)]+=p; }
        for(int c=0;c<C;c++){
            float pc = acc[c]/(denom>1e-30f?denom:1e-30f);
            /* label smoothing: target = (1-smooth)*posterior_of_true + smooth/C */
            float tgt = pc*(1.0f-smooth) + (smooth>0? smooth/C : 0.0f);
            float g = prob[t][c] - tgt;
            /* focal CTC (Feng 2019): down-weight confident/easy targets so rare
             * classes (Zipf long tail) get stronger signal. (1-pc)^gamma. */
            if(focal>0.0f){ float fl=powf(1.0f-pc>0?1.0f-pc:0.0f, focal); g*=fl; }
            grad[t*C+c] = g;
        }
    }
    return loss;
}

int ctc_greedy_decode(int T, int C, const float *logits, int *out){
    int n=0, prev=-1;
    for(int t=0;t<T;t++){
        int best=0; float bv=logits[(size_t)t*C];
        for(int c=1;c<C;c++){ float v=logits[(size_t)t*C+c]; if(v>bv){bv=v;best=c;} }
        if(best!=0 && best!=prev) out[n++]=best;  /* drop blank + collapse repeats */
        prev=best;
    }
    return n;
}

/* ---- beam search (prefix-based, blank-aware, two-score Graves formulation) ----
 * Each live prefix carries p_b (prob of path ending in BLANK) and p_nb (path NOT
 * ending in blank). This correctly separates "prefix X + blank" from "prefix X +
 * char" so the no-repeat-without-blank CTC rule holds and short paths are not
 * spuriously favoured. Outperforms greedy on ambiguous/long words. */
typedef struct { int *seq; int len; float pb; float pnb; int last; } Prefix;
int ctc_beam_decode(int T, int C, const float *logits, int beam, int *out){
    if(beam<1) beam=1; if(beam>64) beam=64;
    Prefix A[128]; int nA=0;
    A[nA].seq=malloc(64*sizeof(int)); A[nA].len=0; A[nA].pb=1.0f; A[nA].pnb=0.0f; A[nA].last=0; nA=1;
    for(int t=0;t<T;t++){
        float mx=logits[(size_t)t*C]; for(int c=1;c<C;c++) if(logits[(size_t)t*C+c]>mx) mx=logits[(size_t)t*C+c];
        float pr[MAXC]; float s=0; for(int c=0;c<C;c++){ pr[c]=expf(logits[(size_t)t*C+c]-mx); s+=pr[c]; }
        for(int c=0;c<C;c++) pr[c]/=s;
        Prefix B[128]; int nB=0;
        for(int i=0;i<nA;i++){
            Prefix *p=&A[i];
            float lac = p->pb + p->pnb; /* prob of prefix so far (any ending) */
            /* blank extension */
            if(nB<128){
                /* merge with existing blank-end at same seq */
                int found=-1; for(int j=0;j<nB;j++) if(B[j].len==p->len && (p->len==0||B[j].seq[p->len-1]==p->last) && B[j].last==0){ found=j; break; }
                float add = lac * pr[0];
                if(found>=0){ B[found].pb += add; }
                else { B[nB].seq=malloc(64*sizeof(int)); memcpy(B[nB].seq,p->seq,p->len*sizeof(int)); B[nB].len=p->len; B[nB].pb=add; B[nB].pnb=0; B[nB].last=0; nB++; }
            }
            /* char extensions */
            for(int c=1;c<C;c++){
                float add = lac * pr[c];
                int newlen = (c==p->last)? p->len : p->len+1;
                int found=-1; for(int j=0;j<nB;j++) if(B[j].len==newlen && (newlen==0||B[j].seq[newlen-1]==c)){ found=j; break; }
                if(found>=0){
                    if(c==p->last) B[found].pnb += p->pnb * pr[c]; /* repeat: only from non-blank end */
                    else           B[found].pnb += add;
                } else if(nB<128){
                    B[nB].seq=malloc(64*sizeof(int)); memcpy(B[nB].seq,p->seq,p->len*sizeof(int));
                    if(c!=p->last) B[nB].seq[p->len]=c;
                    B[nB].len=newlen; B[nB].pb=0; B[nB].pnb = (c==p->last)? p->pnb*pr[c] : add; B[nB].last=c; nB++;
                }
            }
        }
        /* keep top-`beam` prefixes by total prob (pb+pnb) */
        for(int i=0;i<nB-1;i++) for(int j=i+1;j<nB;j++){
            if((B[j].pb+B[j].pnb) > (B[i].pb+B[i].pnb)){ Prefix tmp=B[i]; B[i]=B[j]; B[j]=tmp; }
        }
        if(nB>beam) nB=beam;
        for(int i=0;i<nA;i++) free(A[i].seq);
        for(int i=0;i<nB;i++) A[i]=B[i];
        nA=nB;
    }
    int n=0; if(nA>0){ for(int i=0;i<A[0].len;i++) out[n++]=A[0].seq[i]; free(A[0].seq); }
    for(int i=1;i<nA;i++) free(A[i].seq);
    return n;
}
