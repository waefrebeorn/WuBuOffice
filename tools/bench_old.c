/* bench_old.c -- the OLD strided-gather conv (pre-im2col) implemented standalone,
 * to A/B against bench_conv3. Same MED config, same random inputs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define K1 16
#define S1 5
#define P1 2
#define K2 32
#define S2 5
#define P2 2
#define K3 64
#define S3 3

static float rnd(void){ return (float)rand()/RAND_MAX*2-1; }

int main(void){
    int c1H=28-S1+1, c1W=28-S1+1, p1H=c1H/P1, p1W=c1W/P1;
    int c2H=p1H-S2+1, c2W=p1W-S2+1, p2H=c2H/P2, p2W=c2W/P2;
    int c3H=p2H-S3+1, c3W=p2W-S3+1;
    int fd=c3H*c3W*K3;

    float *w1=malloc(K1*S1*S1*sizeof(float));
    float *w2=malloc(K2*K1*S2*S2*sizeof(float));
    float *w3=malloc(K3*K2*S3*S3*sizeof(float));
    for(int i=0;i<K1*S1*S1;i++) w1[i]=rnd();
    for(int i=0;i<K2*K1*S2*S2;i++) w2[i]=rnd();
    for(int i=0;i<K3*K2*S3*S3;i++) w3[i]=rnd();

    int N=2000;
    float *img=malloc(28*28*sizeof(float));
    for(int i=0;i<28*28;i++) img[i]=rnd();
    float *c1=malloc(c1H*c1W*K1*sizeof(float));
    float *p1=malloc(p1H*p1W*K1*sizeof(float));
    float *c2=malloc(c2H*c2W*K2*sizeof(float));
    float *p2=malloc(p2H*p2W*K2*sizeof(float));
    float *c3=malloc(c3H*c3W*K3*sizeof(float));
    float *am1=malloc(p1H*p1W*K1*sizeof(int));
    float *am2=malloc(p2H*p2W*K2*sizeof(int));

    clock_t t0=clock();
    for(int it=0;it<N;it++){
        /* stage1 */
        for(int k=0;k<K1;k++)for(int y=0;y<c1H;y++)for(int x=0;x<c1W;x++){
            float s=0; const float*w=w1+k*S1*S1;
            for(int dy=0;dy<S1;dy++)for(int dx=0;dx<S1;dx++) s+=w[dy*S1+dx]*img[(y+dy)*28+(x+dx)];
            float a=s>0?s:0; c1[(y*c1W+x)*K1+k]=a;
        }
        for(int k=0;k<K1;k++)for(int py=0;py<p1H;py++)for(int px=0;px<p1W;px++){
            int best=0; float bv=-1e30f;
            for(int dy=0;dy<P1;dy++)for(int dx=0;dx<P1;dx++){
                int iy=py*P1+dy, ix=px*P1+dx;
                float v=c1[((iy*c1W+ix)*K1)+k];
                if(v>bv){bv=v;best=iy*c1W+ix;}
            }
            p1[((py*p1W+px)*K1)+k]=bv; am1[((py*p1W+px)*K1)+k]=best;
        }
        /* stage2 */
        for(int k=0;k<K2;k++)for(int y=0;y<c2H;y++)for(int x=0;x<c2W;x++){
            float s=0; const float*w=w2+k*K1*S2*S2;
            for(int c=0;c<K1;c++)for(int dy=0;dy<S2;dy++)for(int dx=0;dx<S2;dx++)
                s+=w[(c*S2+dy)*S2+dx]*p1[((y+dy)*p1W+(x+dx))*K1+c];
            float a=s>0?s:0; c2[(y*c2W+x)*K2+k]=a;
        }
        for(int k=0;k<K2;k++)for(int py=0;py<p2H;py++)for(int px=0;px<p2W;px++){
            int best=0; float bv=-1e30f;
            for(int dy=0;dy<P2;dy++)for(int dx=0;dx<P2;dx++){
                int iy=py*P2+dy, ix=px*P2+dx;
                float v=c2[((iy*c2W+ix)*K2)+k];
                if(v>bv){bv=v;best=iy*c2W+ix;}
            }
            p2[((py*p2W+px)*K2)+k]=bv; am2[((py*p2W+px)*K2)+k]=best;
        }
        /* stage3 */
        for(int k=0;k<K3;k++)for(int y=0;y<c3H;y++)for(int x=0;x<c3W;x++){
            float s=0; const float*w=w3+k*K2*S3*S3;
            for(int c=0;c<K2;c++)for(int dy=0;dy<S3;dy++)for(int dx=0;dx<S3;dx++)
                s+=w[(c*S3+dy)*S3+dx]*p2[((y+dy)*p2W+(x+dx))*K2+c];
            float a=s>0?s:0; c3[(y*c3W+x)*K3+k]=a;
        }
    }
    clock_t t1=clock();
    double secs=(double)(t1-t0)/CLOCKS_PER_SEC;
    printf("OLD strided fwd: %d passes in %.3f s  ->  %.0f passes/sec\n", N, secs, N/secs);
    printf("(new im2col fwd+bwd does ~1288 fwd+bwd/sec; old fwd-only shown above)\n");
    return 0;
}
