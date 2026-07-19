/* mlp_train_feats.c -- isolate the C MLP: train it on pre-extracted conv
 * features (text dump) + labels, report train/test acc. If this learns but
 * the full trainer doesn't, the bug is in the trainer's threading/reduction.
 * Usage: mlp_train_feats <feats.txt> <labels-idx> <epochs> */
#include "mlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void load_lab(const char*path, unsigned char**out, long*n){
    FILE*f=fopen(path,"rb"); fseek(f,0,SEEK_END); fseek(f,0,SEEK_SET);
    fread((char[8]){0},1,8,f); *out=malloc(10000); fread(*out,1,10000,f); fclose(f); *n=10000;
}
/* read features: N lines of 256 comma-sep floats */
static float* load_feats(const char*path, long*N, int*D){
    FILE*f=fopen(path,"r"); if(!f){return 0;}
    char buf[1<<16]; long n=0; int d=0; float*all=0; size_t cap=0;
    while(fgets(buf,sizeof buf,f)){
        // count commas on first line
        if(n==0){ for(char*p=buf;*p;p++) if(*p==',') d++; d++; }
        float*row=malloc((size_t)d*sizeof(float));
        int k=0; char*p=buf; char*e;
        for(;(e=strchr(p,','))||(*p);p=e?e+1:p+strlen(p)){
            if(e)*e=0; row[k++]=(float)strtod(p,NULL); if(!e)break; p=e;
        }
        if(k!=d){ /* last token */ }
        all=realloc(all,(size_t)(n+1)*d*sizeof(float));
        memcpy(all+(size_t)n*d,row,(size_t)d*sizeof(float)); free(row); n++;
        if((size_t)n>=cap+4096){cap=n;}
    }
    fclose(f); *N=n; *D=d; return all;
}

int main(int argc,char**argv){
    if(argc<4){printf("usage: %s <feats.txt> <labels> <epochs>\n",argv[0]);return 1;}
    long N; int D; float*X=load_feats(argv[1],&N,&D);
    unsigned char*lab; long nlab; load_lab(argv[2],&lab,&nlab);
    printf("feats N=%ld D=%d labels=%ld\n",N,D,nlab);
    MLP*m=mlp_create(D,256,128,10,1);
    int epochs=atoi(argv[3]);
    // z-norm
    float *mu=calloc(D,sizeof(float)),*sd=calloc(D,sizeof(float));
    for(long i=0;i<N;i++) for(int d=0;d<D;d++) mu[d]+=X[i*D+d];
    for(int d=0;d<D;d++) mu[d]/=(float)N;
    for(long i=0;i<N;i++) for(int d=0;d<D;d++){float v=X[i*D+d]-mu[d]; sd[d]+=v*v;}
    for(int d=0;d<D;d++){sd[d]=sqrtf(sd[d]/(float)N); if(sd[d]<1e-2f)sd[d]=1;}
    float lr=argc>4?(float)atof(argv[4]):0.05f;
    for(int ep=0;ep<epochs;ep++){
        long cor=0;
        for(long i=0;i<N;i++){
            float z[1024]; for(int d=0;d<D;d++) z[d]=(X[i*D+d]-mu[d])/sd[d];
            float sc[26]; mlp_forward(m,z,sc);
            int best=0; for(int c=1;c<10;c++) if(sc[c]>sc[best])best=c;
            if(best==lab[i]) cor++;
            mlp_zero_grad(m);
            mlp_backward(m,z,lab[i]);
            mlp_apply_plain(m,lr);
        }
        if(ep%5==0||ep==epochs-1) printf("  ep%d train_acc=%.2f%%\n",ep+1,100.0f*cor/N);
    }
    // test split (last 2000)
    long te=N-2000; long c2=0;
    for(long i=te;i<N;i++){ float z[1024]; for(int d=0;d<D;d++) z[d]=(X[i*D+d]-mu[d])/sd[d];
        float sc[26]; mlp_forward(m,z,sc); int best=0; for(int c=1;c<10;c++) if(sc[c]>sc[best])best=c; if(best==lab[i])c2++; }
    printf("TEST acc=%.2f%%\n",100.0f*c2/2000.0f);
    return 0;
}
