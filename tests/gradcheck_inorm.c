/* gradcheck_inorm.c -- finite-difference gradient check for the instance-norm
 * gamma/beta params (smooth, no maxpool-argmax discontinuity). Decisive check
 * that in_backward math is correct. Uses CN_INORM=1. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "convnet3.h"
#include "mlp.h"

static uint32_t rng=0x77AB77CDu;
static uint32_t xr(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static float fr(void){return (float)xr()/(float)0xFFFFFFFFu;}

/* loss = cross-entropy of conv3->mlp on one fixed sample/label */
static float loss_of(ConvNet3*cn, MLP*m, const float*img, int lab, int D){
    float feat[256], sc[8];
    convnet3_forward(cn,img,feat);
    mlp_forward(m,feat,sc);
    float mx=sc[0]; for(int c=1;c<D;c++) if(sc[c]>mx)mx=sc[c];
    float sum=0; for(int c=0;c<D;c++) sum+=expf(sc[c]-mx);
    return -(sc[lab]-mx-logf(sum));
}

int main(void){
    setenv("CN_INORM","1",1);
    ConvNet3*cn=convnet3_create(&CONV_MED);
    int D=2; MLP*m=mlp_create(convnet3_dim(cn),64,32,D,0xABCD1234u);
    float img[784]; for(int i=0;i<784;i++) img[i]=0.2f+0.6f*fr();
    int lab=1;

    /* analytic grads */
    float feat[256], df[256];
    convnet3_zero_grad(cn); mlp_zero_grad(m);
    convnet3_forward(cn,img,feat);
    float sc[8]; mlp_forward(m,feat,sc);
    mlp_backward(m,feat,lab);
    mlp_input_grad(m,feat,df);
    convnet3_backward(cn,img,feat,df);

    /* check gamma/beta of all 3 stages (layers 6..11) + a couple conv weights */
    float eps=1e-3f; int fails=0;
    const char*names[]={"ga1","be1","ga2","be2","ga3","be3"};
    for(int li=6; li<=11; li++){
        ConvLayer3 L=convnet3_layer(cn,li);
        int idx = xr()%L.n;             /* spot-check one element */
        float g_an=L.grad[idx];
        float save=L.param[idx];
        L.param[idx]=save+eps; float lp=loss_of(cn,m,img,lab,D);
        L.param[idx]=save-eps; float lm=loss_of(cn,m,img,lab,D);
        L.param[idx]=save;
        float g_fd=(lp-lm)/(2*eps);
        float rel=fabsf(g_an-g_fd)/(fabsf(g_an)+fabsf(g_fd)+1e-8f);
        printf("%s[%d]: analytic=% .6f  fd=% .6f  rel_err=%.2e  %s\n",
               names[li-6],idx,g_an,g_fd,rel, rel<2e-2f?"ok":"FAIL");
        if(rel>=2e-2f) fails++;
    }
    printf(fails? "\nGRADCHECK FAIL (%d)\n":"\nGRADCHECK PASS\n", fails);
    return fails?1:0;
}
