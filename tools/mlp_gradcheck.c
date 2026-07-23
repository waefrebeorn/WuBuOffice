/* mlp_gradcheck.c -- finite-difference vs analytic grad for mlp_backward_smooth.
 * Loss = sum_c dfeat[c]*score[c]  (score = pre-softmax logit).
 * dLoss/dW should equal mlp_backward's gW. C11.
 *   cc -std=c11 -O2 -D_POSIX_C_SOURCE=200809L -Isrc/wubuocr src/wubuocr/mlp.c tools/mlp_gradcheck.c -lm -o /tmp/mgc
 */
#include "mlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
static float rnd(uint32_t*s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return (float)(*s&0xFFFFFF)/(float)0xFFFFFF*2.0f-1.0f;}

/* loss = cross-entropy w.r.t. `target` (matches mlp_backward_smooth's internal
 * softmax+target). For smoothed, use label-smoothed CE:
 *   L = -sum_c p_c * log(softmax_c),  p_target=(1-s), p_others=s/(K-1). */
static float loss_of(MLP*m,const float*z,int target,float smooth,int K){
  float sc[26]; mlp_forward(m,z,sc);
  float mx=sc[0]; for(int c=1;c<K;c++) if(sc[c]>mx)mx=sc[c];
  float sum=0; for(int c=0;c<K;c++) sum+=expf(sc[c]-mx);
  float logp[26]; for(int c=0;c<K;c++) logp[c]=sc[c]-mx-logf(sum);
  if(smooth>0){
    float s=smooth, off=s/(K-1); float L=0;
    for(int c=0;c<K;c++){ float pc=(c==target)?(1-s):off; L-=pc*logp[c]; }
    return L;
  }
  return -logp[target];
}
int main(int argc,char**argv){
  int use_smooth = argc>1 && strcmp(argv[1],"smooth")==0;
  MLP *m = mlp_create(784,128,64,26,0x1234ABCDu);
  float z[784]; uint32_t rs=55; for(int i=0;i<784;i++)z[i]=rnd(&rs)*0.5f+0.5f;
  int target=7;

  /* mlp_backward / mlp_backward_smooth READ the activations cached by the
   * most recent mlp_forward -- you MUST forward BEFORE backward, or the
   * cached h2act/h1act are the calloc-zeroed init and every grad is 0.
   * The trainer (emnist_train_conv3.c) does forward then backward per sample;
   * this harness must do the same to test the real gradient. */
  mlp_zero_grad(m);
  { float sc[26]; mlp_forward(m, z, sc); }
  if(use_smooth) mlp_backward_smooth(m,z,target,0.1f); else mlp_backward(m,z,target);

  float h = argc>2 ? (float)atof(argv[2]) : 2e-3f;   /* FD step; optimal ~2e-3 for float32 */
  int groups[]={0,1,2,3,4,5}; int worst=-1; float werr=0; int fails=0;
  for(int gi=0;gi<6;gi++){
    MLPLayer L=mlp_layer(m,gi); int step=L.n>400?L.n/200:1; int chk=0,fl=0; float lr=0;
    for(int i=0;i<L.n;i+=step){
      float w0=L.param[i];
      L.param[i]=w0+h; float lp=loss_of(m,z,target,use_smooth?0.1f:0.0f,26);
      L.param[i]=w0-h; float lm=loss_of(m,z,target,use_smooth?0.1f:0.0f,26);
      L.param[i]=w0;
      float num=(lp-lm)/(2*h); float ana=L.grad[i];
      float rel=(fabsf(num)+fabsf(ana)>1e-2f)? fabsf(num-ana)/(fabsf(num)+fabsf(ana)+1e-6f):0;
      if(rel>lr)lr=rel; if(rel>1e-2f)fl++; chk++;
    }
    printf("grp%d n=%d chk=%d worst_rel=%.3e fails=%d\n",gi,L.n,chk,lr,fl);
    if(lr>werr){werr=lr;worst=gi;} fails+=fl;
  }
  printf("MLP GRADCHECK: worst_rel=%.3e grp%d fails=%d -> %s\n",werr,worst,fails, werr<2e-2f&&fails==0?"PASS":"FAIL");
  return werr<2e-2f&&fails==0?0:1;
}
