/* convnet3_cbam.c -- CBAM attention (channel + spatial) for the CRNN
 * convnet. Enabled via env CBAM=1 at create; split from convnet3.c so the
 * core conv/GEMM path stays lean. */
#include "convnet3_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static float cbam_sigf(float x){ return 1.0f/(1.0f+expf(-x)); }

int convnet3_cbam_size(const ConvNet3 *cn){
    if(!cn || !cn->use_cbam) return 0;
    return cn->cbam_r*2*cn->K2 + cn->cbam_r + cn->K2*cn->cbam_r + cn->K2 + 2;
}
void convnet3_cbam_pack(const ConvNet3 *cn, float *o){
    if(!cn||!cn->use_cbam) return;
    int K=cn->K2, r=cn->cbam_r; size_t off=0;
    memcpy(o+off,cn->ca_w1,(size_t)r*2*K*sizeof(float)); off+=r*2*K;
    memcpy(o+off,cn->ca_b1,(size_t)r*sizeof(float)); off+=r;
    memcpy(o+off,cn->ca_w2,(size_t)K*r*sizeof(float)); off+=K*r;
    memcpy(o+off,cn->ca_b2,(size_t)K*sizeof(float)); off+=K;
    memcpy(o+off,cn->sa_w,2*sizeof(float)); off+=2;
}
void convnet3_cbam_unpack(ConvNet3 *cn, const float *i){
    if(!cn||!cn->use_cbam) return;
    int K=cn->K2, r=cn->cbam_r; size_t off=0;
    memcpy(cn->ca_w1,i+off,(size_t)r*2*K*sizeof(float)); off+=r*2*K;
    memcpy(cn->ca_b1,i+off,(size_t)r*sizeof(float)); off+=r;
    memcpy(cn->ca_w2,i+off,(size_t)K*r*sizeof(float)); off+=K*r;
    memcpy(cn->ca_b2,i+off,(size_t)K*sizeof(float)); off+=K;
    memcpy(cn->sa_w,i+off,2*sizeof(float)); off+=2;
}
void convnet3_enable_cbam(ConvNet3 *cn){ if(cn) cn->use_cbam=1; }
/* Plain-SGD step on CBAM attention weights (tiny module ~ a few hundred params;
 * Adam on the trunk is overkill here). grad buffers are zeroed by convnet3_zero_grad. */
void convnet3_sgd_cbam(ConvNet3 *cn, float lr){
    if(!cn||!cn->use_cbam) return;
    int K=cn->K2, r=cn->cbam_r; size_t off=0, n;
    n=(size_t)r*2*K; for(size_t i=0;i<n;i++) cn->ca_w1[i]-=lr*cn->gca_w1[i]; off+=n;
    n=(size_t)r;     for(size_t i=0;i<n;i++) cn->ca_b1[i]-=lr*cn->gca_b1[i]; off+=n;
    n=(size_t)K*r;   for(size_t i=0;i<n;i++) cn->ca_w2[i]-=lr*cn->gca_w2[i]; off+=n;
    n=(size_t)K;     for(size_t i=0;i<n;i++) cn->ca_b2[i]-=lr*cn->gca_b2[i]; off+=n;
    n=2;             for(size_t i=0;i<n;i++) cn->sa_w[i]-=lr*cn->gsa_w[i]; off+=n;
}
int convnet3_cbam_enabled(const ConvNet3 *cn){ return cn? cn->use_cbam : 0; }

/* Enable instance norm (buffers already allocated at create time) */
void cbam_fwd(ConvNet3 *cn){
    int K=cn->K2, P=cn->p2H*cn->p2W, r=cn->cbam_r;
    float *p2=cn->p2;
    for(int k=0;k<K;k++){
        float av=0,mx=-1e30f; for(int p=0;p<P;p++){ float v=p2[(size_t)p*K+k]; av+=v; if(v>mx)mx=v; } av/=P;
        cn->cbam_avg[k]=av; cn->cbam_max[k]=mx;
    }
    for(int k=0;k<K;k++){
        float h[r>0?r:1];
        for(int i=0;i<r;i++){
            float s=cn->ca_b1[i];
            s+=cn->ca_w1[(size_t)i*2*K+k]*cn->cbam_avg[k];
            s+=cn->ca_w1[(size_t)i*2*K+K+k]*cn->cbam_max[k];
            h[i]=(s>0?s:0.0f);
        }
        float s2=cn->ca_b2[k];
        for(int i=0;i<r;i++) s2+=cn->ca_w2[(size_t)k*r+i]*h[i];
        cn->cbam_mc[k]=cbam_sigf(s2);
    }
    for(int p=0;p<P;p++){
        float av=0,mx=-1e30f; for(int k=0;k<K;k++){ float v=p2[(size_t)p*K+k]; av+=v; if(v>mx)mx=v; } av/=K;
        cn->cbam_sa[p]=cbam_sigf(cn->sa_w[0]*av + cn->sa_w[1]*mx);
    }
    for(int p=0;p<P;p++) for(int k=0;k<K;k++) p2[(size_t)p*K+k]*=cn->cbam_mc[k]*cn->cbam_sa[p];
}

void cbam_bwd(ConvNet3 *cn, float *dp2){
    int K=cn->K2, P=cn->p2H*cn->p2W, r=cn->cbam_r;
    float *p2=cn->p2;
    for(int p=0;p<P;p++) for(int k=0;k<K;k++) dp2[(size_t)p*K+k]*=cn->cbam_mc[k]*cn->cbam_sa[p];
    for(int k=0;k<K;k++){
        float dMc=0;
        for(int p=0;p<P;p++){ float p2orig=p2[(size_t)p*K+k]/(cn->cbam_mc[k]*cn->cbam_sa[p]); dMc+=dp2[(size_t)p*K+k]*p2orig*cn->cbam_sa[p]; }
        float sm=cn->cbam_mc[k]*(1.0f-cn->cbam_mc[k]);
        float dsig=dMc*sm;
        cn->gca_b2[k]+=dsig;
        float h[r>0?r:1];
        for(int i=0;i<r;i++){ float s=cn->ca_b1[i]; s+=cn->ca_w1[(size_t)i*2*K+k]*cn->cbam_avg[k]; s+=cn->ca_w1[(size_t)i*2*K+K+k]*cn->cbam_max[k]; h[i]=(s>0?s:0.0f); }
        for(int i=0;i<r;i++) cn->gca_w2[(size_t)k*r+i]+=dsig*h[i];
        for(int i=0;i<r;i++){ float dh=(h[i]>0?dsig:0.0f); cn->gca_b1[i]+=dh; cn->gca_w1[(size_t)i*2*K+k]+=dh*cn->cbam_avg[k]; cn->gca_w1[(size_t)i*2*K+K+k]+=dh*cn->cbam_max[k]; }
    }
    for(int p=0;p<P;p++){
        float av=0,mx=-1e30f; for(int k=0;k<K;k++){ float v=p2[(size_t)p*K+k]/(cn->cbam_mc[k]*cn->cbam_sa[p]); av+=v; if(v>mx)mx=v; } av/=K;
        float dSa=0; for(int k=0;k<K;k++){ float p2orig=p2[(size_t)p*K+k]/(cn->cbam_mc[k]*cn->cbam_sa[p]); dSa+=dp2[(size_t)p*K+k]*p2orig*cn->cbam_mc[k]; }
        float ss=cn->cbam_sa[p]*(1.0f-cn->cbam_sa[p]);
        float dsig=dSa*ss;
        cn->gsa_w[0]+=dsig*av; cn->gsa_w[1]+=dsig*mx;
    }
}


