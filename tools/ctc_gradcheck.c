/* ctc_gradcheck.c -- finite-difference check of the REAL src/wubuocr/ctc.c
 * Build: cc -std=c11 -O2 -I src/wubuocr -o /tmp/ctc_gc ctc_gradcheck.c src/wubuocr/ctc.c -lm
 * Validates ctc_loss() analytic gradient vs numerical gradient. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "ctc.h"

static float logits[64*256];
static float grad[64*256];
static int T=5, C=8, L=2;
static int tgt[32];
static int S;

static int auto_lab(int s){ return (s&1)? tgt[(s-1)/2]:0; }

int main(void){
    T=5; C=8; L=2; tgt[0]=1; tgt[1]=3; S=2*L+1;
    uint32_t rs=0xABCDEF;
    for(int i=0;i<T*C;i++){ rs^=rs<<13; rs^=rs>>17; rs^=rs<<5; logits[i]=((float)(rs&0xFFFF)/65535.0f)*2.0f-1.0f; }
    float base = ctc_loss(T,C,L,tgt,logits,grad,0.0f,0.0f);
    /* Reconstruct linear alpha/beta to compare pc */
    float P_[64][256]; for(int t=0;t<T;t++){ float mx=logits[t*C]; float su=0; for(int c=0;c<C;c++){P_[t][c]=expf(logits[t*C+c]-mx);su+=P_[t][c];} for(int c=0;c<C;c++)P_[t][c]/=su; }
    float A_[64][64],B_[64][64];
    for(int s=0;s<S;s++) A_[0][s]=0;
    if(S>=1) A_[0][0]=P_[0][0];
    if(S>=2&&L>0) A_[0][1]=P_[0][tgt[0]];
    for(int t=1;t<T;t++) for(int s=0;s<S;s++){ float a=A_[t-1][s]; if(s-1>=0)a+=A_[t-1][s-1]; if(s-2>=0)a+=A_[t-1][s-2]; A_[t][s]=a*P_[t][auto_lab(s)]; }
    for(int s=0;s<S;s++) B_[T-1][s]=0; B_[T-1][S-1]=1; if(S>=2&&L>0)B_[T-1][S-2]=1;
    for(int t=T-2;t>=0;t--) for(int s=0;s<S;s++){ float b=B_[t+1][s]*P_[t+1][auto_lab(s)]; if(s+1<S)b+=B_[t+1][s+1]*P_[t+1][auto_lab(s+1)]; if(s+2<S)b+=B_[t+1][s+2]*P_[t+1][auto_lab(s+2)]; B_[t][s]=b; }
    int bad=0;
    for(int i=0;i<T*C;i++){
        float h=1e-3f;
        float s2=logits[i];
        logits[i]=s2+h; float lp=ctc_loss(T,C,L,tgt,logits,grad,0.0f,0.0f);
        logits[i]=s2-h; float lm=ctc_loss(T,C,L,tgt,logits,grad,0.0f,0.0f);
        logits[i]=s2;
        float num=(lp-lm)/(2*h);
        int t=i/C,c=i%C;
        ctc_loss(T,C,L,tgt,logits,grad,0.0f,0.0f);
        float ana=grad[t*C+c];
        float rel=fabsf(num-ana)/((float)fabsf(num)+fabsf(ana)+1e-6f);
        if(rel>0.05f){
            int tt=t, cc=c;
            float denom=0; float acc_=0;
            for(int s=0;s<S;s++){ float p=A_[tt][s]*B_[tt][s]; denom+=p; if(auto_lab(s)==cc) acc_+=p; }
            float pc_lin = acc_/(denom>1e-30f?denom:1e-30f);
            float pc_true = P_[tt][cc]-num;
            if(bad<8) printf("  i=%d(t=%d,c=%d) num=%.5f ana=%.5f | pc_true=%.5f pc_lin=%.5f prob=%.5f\n",i,tt,cc,num,ana,pc_true,pc_lin,P_[tt][cc]);
            bad++;
        }
    }
    printf("base loss=%.4f  grad mismatches(>5%%)=%d/%d\n", base, bad, T*C);
    printf("%s\n", bad==0?"CTC GRAD CHECK PASSED":"CTC GRAD CHECK FAILED");
    return bad?1:0;
}
