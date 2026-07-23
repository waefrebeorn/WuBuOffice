/* bench_conv3.c -- measure forward+backward throughput of the im2col+GEMM
 * conv engine. Pure compute, no dataset I/O. Reports passes/sec. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "convnet3.h"

int main(void){
    ConvConfig3 cfg={28,28, 16,5,2, 32,5,2, 64,3,1};
    ConvNet3 *cn=convnet3_create(&cfg);
    int D=convnet3_dim(cn);

    int N=2000;
    float *img=malloc(28*28*sizeof(float));
    float *feat=malloc(D*sizeof(float));
    float *dfeat=malloc(D*sizeof(float));
    for(int i=0;i<28*28;i++) img[i]=(float)rand()/RAND_MAX*2-1;
    for(int i=0;i<D;i++){ feat[i]=1.0f; dfeat[i]=1.0f; }

    /* warmup */
    for(int i=0;i<20;i++){ convnet3_forward(cn,img,feat); convnet3_zero_grad(cn); convnet3_backward(cn,img,feat,dfeat); }

    clock_t t0=clock();
    for(int i=0;i<N;i++){ convnet3_forward(cn,img,feat); convnet3_zero_grad(cn); convnet3_backward(cn,img,feat,dfeat); }
    clock_t t1=clock();
    double secs=(double)(t1-t0)/CLOCKS_PER_SEC;
    printf("conv3 fwd+bwd: %d passes in %.3f s  ->  %.0f passes/sec\n", N, secs, N/secs);

    clock_t f0=clock();
    for(int i=0;i<N;i++) convnet3_forward(cn,img,feat);
    clock_t f1=clock();
    double fsecs=(double)(f1-f0)/CLOCKS_PER_SEC;
    printf("conv3 fwd-only: %d passes in %.3f s  ->  %.0f passes/sec\n", N, fsecs, N/fsecs);

    free(img);free(feat);free(dfeat);
    convnet3_destroy(cn);
    return 0;
}
