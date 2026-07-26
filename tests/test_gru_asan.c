#include "gru.h"
#include "gpu_blas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
    int D=80,H=48,T=300;
    GRU *r=gru_create(D,H,0,12345);
    float *x=calloc((size_t)T*D,sizeof(float));
    for(int i=0;i<T*D;i++) x[i]=((i*7)%97)/97.0f-0.5f;
    float *y=calloc((size_t)T*H,sizeof(float));
    float *dx=calloc((size_t)T*D,sizeof(float));
    gru_forward(r,T,x);
    gru_get_output(r,y);
    float *dy=calloc((size_t)T*H,sizeof(float)); for(int i=0;i<T*H;i++)dy[i]=1.0f;
    gru_zero_grad(r); gru_backward(r,T,dy,dx);
    printf("OK T=%d H=%d D=%d  outsum=%.3f\n", T,H,D, y[0]+y[T*H-1]);
    /* vary T (like training) */
    for(int tt=50; tt<=400; tt+=50){
        float *x2=calloc((size_t)tt*D,sizeof(float)); for(int i=0;i<tt*D;i++)x2[i]=((i*3)%53)/53.0f-0.5f;
        float *y2=calloc((size_t)tt*H,sizeof(float));
        float *dy2=calloc((size_t)tt*H,sizeof(float)); for(int i=0;i<tt*H;i++)dy2[i]=1.0f;
        float *dx2=calloc((size_t)tt*D,sizeof(float));
        gru_forward(r,tt,x2); gru_get_output(r,y2);
        gru_zero_grad(r); gru_backward(r,tt,dy2,dx2);
        free(x2); free(y2); free(dy2); free(dx2);
    }
    printf("VARY OK\n");
    return 0;
}
