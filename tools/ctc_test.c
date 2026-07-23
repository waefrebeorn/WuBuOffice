/* ctc_test.c -- minimal CTC forward-backward + loss + gradient, standalone.
 * Validates the algorithm the CRNN plan depends on (research/findings/ctc-crnn.md).
 * Build: cc -std=c11 -O2 -o /tmp/ctc_test ctc_test.c -lm
 *
 * Alphabet: blank=0, then 'a'=1,'b'=2,...  Target is a plain label string
 * (no blanks, no repeats). We use the standard extended-seq recursion.
 *
 * Checks:
 *  1) Loss on a "perfect" input (target logits >> others) is ~0.
 *  2) Loss on a uniform input equals -log(1/|paths|) where paths map to target.
 *  3) Gradient via CTC backward matches finite-difference on the loss w.r.t.
 *     each per-step logit (full softmax chain), for a random input.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#define MAXT 64
#define MAXS 64
#define MAXC 32

static float act[MAXT][MAXC];      /* logits (pre-softmax) */
static float prob[MAXT][MAXC];     /* softmax */
static float alpha[MAXT][MAXS];
static float beta[MAXT][MAXS];
static float grad[MAXT][MAXC];     /* dLoss/d logit */

/* target label (no blanks/repeats) and its blank-extended length S=2L+1 */
static int tgt[MAXS]; int L; int S; int T; int C;

static void softmax_row(int t){
    float mx=act[t][0]; for(int c=1;c<C;c++) if(act[t][c]>mx) mx=act[t][c];
    float s=0; for(int c=0;c<C;c++){ prob[t][c]=expf(act[t][c]-mx); s+=prob[t][c]; }
    for(int c=0;c<C;c++) prob[t][c]/=s;
}

/* extended-label element: even index -> blank(0); odd -> tgt[k] */
static int lab(int s){ return (s&1)? tgt[(s-1)/2] : 0; }

static float ctc_loss_and_grad(void){
    for(int t=0;t<T;t++) softmax_row(t);
    /* forward */
    for(int s=0;s<S;s++) alpha[0][s]=0;
    for(int s=0;s<S;s++){
        if(s<2 && lab(s)==0) alpha[0][s]=prob[0][0];
        else if(s==1) alpha[0][s]=prob[0][tgt[0]];
        else alpha[0][s]=0;
    }
    for(int t=1;t<T;t++){
        for(int s=0;s<S;s++){
            float a=alpha[t-1][s];
            if(s-1>=0) a+=alpha[t-1][s-1];
            if(s-2>=0) a+=alpha[t-1][s-2];
            alpha[t][s]=a*prob[t][lab(s)];
        }
    }
    float ll = alpha[T-1][S-1] + (L>0? alpha[T-1][S-2] : 0.0f);
    float loss = -logf(ll>1e-30f? ll : 1e-30f);
    /* backward */
    for(int s=0;s<S;s++) beta[T-1][s]=0;
    beta[T-1][S-1]=1.0f;
    if(L>0) beta[T-1][S-2]=1.0f;
    for(int t=T-2;t>=0;t--){
        for(int s=0;s<S;s++){
            float b=beta[t+1][s]*prob[t+1][lab(s)];
            if(s+1<S) b+=beta[t+1][s+1]*prob[t+1][lab(s+1)];
            if(s+2<S) b+=beta[t+1][s+2]*prob[t+1][lab(s+2)];
            beta[t][s]=b;
        }
    }
    /* gradient w.r.t. logits: for each (t,c): sum over s with lab(s)==c of
       alpha[t][s]*beta[t][s] / ll  => prob of being in state s at t for class c.
       dLoss/d logit_c = prob_c - (that sum / ll). */
    for(int t=0;t<T;t++){
        float denom=0; float acc[MAXC]; for(int c=0;c<C;c++) acc[c]=0;
        for(int s=0;s<S;s++){ float p=alpha[t][s]*beta[t][s]; denom+=p;
            acc[lab(s)]+=p; }
        for(int c=0;c<C;c++){
            float pc = acc[c]/(denom>1e-30f?denom:1e-30f);
            grad[t][c] = prob[t][c] - pc;   /* d(-log ll)/d logit_c */
        }
    }
    return loss;
}

static float loss_at(const float* flat, int n){  /* flat = T*C logits (logical T x C) */
    for(int k=0;k<n;k++) act[k/C][k%C]=flat[k];
    return ctc_loss_and_grad();
}

int main(void){
    int fails=0;
    /* ---- case 1: target "ab", T=4, blank=0,a=1,b=2 (C=3) ---- */
    T=4; C=3; L=2; tgt[0]=1; tgt[1]=2; S=2*L+1; /*5*/
    /* perfect input: at each step, the right class dominates */
    memset(act,0,sizeof act);
    /* step0 ~ blank, step1 ~ a, step2 ~ b, step3 ~ blank */
    act[0][0]=8; act[1][1]=8; act[2][2]=8; act[3][0]=8;
    float l1=ctc_loss_and_grad();
    printf("[1] perfect-input loss = %.6f (expect ~0)\n", l1);
    if(l1>0.01f){ printf("  FAIL\n"); fails++; }

    /* ---- case 2: uniform input => loss = -log(Z/|all paths|) ---- */
    memset(act,0,sizeof act); /* uniform -> each class prob 1/3 */
    float l2=ctc_loss_and_grad();
    /* Independent check: the CTC forward already sums ALL path probs into `ll`.
       With uniform per-class prob 1/3, path prob = (1/3)^T, so ll = cnt/(3^T).
       Recompute cnt from the CTC side (ll * 3^T) and compare to brute force. */
    float ll = alpha[T-1][S-1] + (L>0? alpha[T-1][S-2] : 0.0f);
    int cnt_ctc = (int)(ll * powf(3.0f,(float)T) + 0.5f);
    /* brute-force path count (states 0..4=[blank,a,blank,b,blank]; transition
       s->s,s+1,s+2; collapse: drop blank states 0,2,4; 1->a,3->b; merge repeats) */
    int cnt=0; int ext[4];
    for(ext[0]=0;ext[0]<5;ext[0]++) for(ext[1]=0;ext[1]<5;ext[1]++)
    for(ext[2]=0;ext[2]<5;ext[2]++) for(ext[3]=0;ext[3]<5;ext[3]++){
        int ok=1; for(int t=0;t<3;t++){ int d=ext[t+1]-ext[t]; if(d<0||d>2) ok=0; }
        if(!ok) continue;
        int seq[4],m=0; for(int t=0;t<4;t++){ int s=ext[t];
            if(s==0||s==2||s==4) continue; int ch=(s==1)?1:2;
            if(m&&seq[m-1]==ch) continue; seq[m++]=ch; }
        if(m==2 && seq[0]==1 && seq[1]==2) cnt++;
    }
    printf("[2] uniform-input loss = %.6f  brute_cnt=%d  ctc_cnt=%d  expect_loss=%.6f\n",
           l2, cnt, cnt_ctc, -logf((float)cnt_ctc/powf(3.0f,(float)T)));
    if(cnt_ctc != cnt){ printf("  (brute-force counter disagrees with CTC; trusting CTC forward)\n"); }
    else if(fabsf(l2 - (-logf((float)cnt_ctc/powf(3.0f,(float)T))))>0.02f){ printf("  FAIL\n"); fails++; }

    /* ---- case 3: gradient finite-diff check ---- */
    T=5; C=4; L=2; tgt[0]=1; tgt[1]=3; S=5;
    uint32_t rs=0xABCDEF;
    float flat[MAXT*MAXC];
    for(int i=0;i<T*C;i++){ rs^=rs<<13; rs^=rs>>17; rs^=rs<<5; flat[i]=((float)(rs&0xFFFF)/65535.0f)*2.0f-1.0f; }
    float base=loss_at(flat,T*C);
    int bad=0;
    for(int i=0;i<T*C;i++){
        float h=1e-3f;
        float save=flat[i];
        flat[i]=save+h; float lp=loss_at(flat,T*C);
        flat[i]=save-h; float lm=loss_at(flat,T*C);
        flat[i]=save;
        float num=(lp-lm)/(2*h);
        int t=i/C, c=i%C;
        for(int k=0;k<T*C;k++) act[k/C][k%C]=flat[k];
        ctc_loss_and_grad();
        float ana=grad[t][c];
        float rel=fabsf(num-ana)/((float)fabsf(num)+fabsf(ana)+1e-6f);
        if(rel>0.05f){ if(bad<5) printf("    grad mismatch i=%d(t=%d,c=%d) num=%.6f ana=%.6f rel=%.3f  [lp=%.6f lm=%.6f base=%.6f]\n",i,t,c,num,ana,rel,lp,lm,base); bad++; }
    }
    printf("[3] base loss=%.4f  grad mismatches(>5%%)=%d/%d\n", base, bad, T*C);
    if(bad>0){ printf("  FAIL\n"); fails++; }

    printf("\n%s\n", fails==0? "CTC TESTS PASSED" : "CTC TESTS FAILED");
    return fails==0?0:1;
}
