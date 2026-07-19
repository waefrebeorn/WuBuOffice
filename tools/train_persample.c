/* train_persample.c -- END-TO-END conv3+MLP trainer using the PROVEN per-sample
 * SGD path (same as test_convnet3.c which hits 100%, and mlp_train_feats.c which
 * hits 98.9%). Bypasses the batched/threaded trainer whose gradient-scaling and
 * Adam paths have been unreliable. Single-thread, per-sample update, no clip.
 *
 * Env: CN_TRAIN,CN_TEST (stems under data/), CN_CLASS, CN_LABOFF, CN_LR,
 *      CN_EPOCHS, CN_AUG (deg), CN_NORM (z-score feats), CN_SUBSET (cap train).
 * Usage: train_persample <datadir>
 */
#include "convnet3.h"
#include "mlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static uint32_t rng=0x2468ACEu;
static uint32_t xr(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static float fr(void){ return (float)xr()/(float)0xFFFFFFFFu; }

static unsigned char* load_idx_img(const char*path,long*n,int*rows,int*cols){
    FILE*f=fopen(path,"rb"); if(!f){fprintf(stderr,"FATAL open %s\n",path);exit(1);}
    unsigned char h[16]; if(fread(h,1,16,f)!=16){exit(1);}
    long cnt=((long)h[4]<<24)|((long)h[5]<<16)|((long)h[6]<<8)|h[7];
    int r=((int)h[8]<<24)|((int)h[9]<<16)|((int)h[10]<<8)|h[11];
    int c=((int)h[12]<<24)|((int)h[13]<<16)|((int)h[14]<<8)|h[15];
    *n=cnt;*rows=r;*cols=c;
    unsigned char*d=malloc((size_t)cnt*r*c); if(fread(d,1,(size_t)cnt*r*c,f)!=(size_t)cnt*r*c){exit(1);}
    fclose(f); return d;
}
static unsigned char* load_idx_lab(const char*path,long*n){
    FILE*f=fopen(path,"rb"); if(!f){fprintf(stderr,"FATAL open %s\n",path);exit(1);}
    unsigned char h[8]; if(fread(h,1,8,f)!=8){exit(1);}
    long cnt=((long)h[4]<<24)|((long)h[5]<<16)|((long)h[6]<<8)|h[7]; *n=cnt;
    unsigned char*d=malloc(cnt); if(fread(d,1,cnt,f)!=(size_t)cnt){exit(1);}
    fclose(f); return d;
}
/* bilinear rotate about center by deg, in-place into out[784] */
static void rot(const float*in,float*out,float deg){
    float rad=deg*3.14159265f/180.0f, cs=cosf(rad), sn=sinf(rad); float cx=13.5f,cy=13.5f;
    for(int y=0;y<28;y++)for(int x=0;x<28;x++){
        float dx=x-cx,dy=y-cy; float sx=cs*dx+sn*dy+cx, sy=-sn*dx+cs*dy+cy;
        int x0=(int)floorf(sx),y0=(int)floorf(sy); float fx=sx-x0,fy=sy-y0; float v=0;
        for(int j=0;j<2;j++)for(int i=0;i<2;i++){int xx=x0+i,yy=y0+j; if(xx>=0&&xx<28&&yy>=0&&yy<28){float w=(i?fx:1-fx)*(j?fy:1-fy); v+=w*in[yy*28+xx];}}
        out[y*28+x]=v;
    }
}

int main(int argc,char**argv){
    const char*dir=argc>1?argv[1]:"data";
    const char*trs=getenv("CN_TRAIN")?getenv("CN_TRAIN"):"fashion/train";
    const char*tes=getenv("CN_TEST")?getenv("CN_TEST"):"fashion/t10k";
    int nclass=getenv("CN_CLASS")?atoi(getenv("CN_CLASS")):10;
    int laboff=getenv("CN_LABOFF")?atoi(getenv("CN_LABOFF")):0;
    float lr=getenv("CN_LR")?(float)atof(getenv("CN_LR")):0.01f;
    int epochs=getenv("CN_EPOCHS")?atoi(getenv("CN_EPOCHS")):40;
    float aug=getenv("CN_AUG")?(float)atof(getenv("CN_AUG")):0.0f;
    int donorm=getenv("CN_NORM")?atoi(getenv("CN_NORM")):1;
    long cap=getenv("CN_SUBSET")?atol(getenv("CN_SUBSET")):0;

    char p[512]; long ntr,nte; int r,c,rl,cl; long nl;
    snprintf(p,sizeof p,"%s/%s-images-idx3-ubyte",dir,trs); unsigned char*trI=load_idx_img(p,&ntr,&r,&c);
    snprintf(p,sizeof p,"%s/%s-labels-idx1-ubyte",dir,trs); unsigned char*trL=load_idx_lab(p,&nl);
    snprintf(p,sizeof p,"%s/%s-images-idx3-ubyte",dir,tes); unsigned char*teI=load_idx_img(p,&nte,&rl,&cl);
    snprintf(p,sizeof p,"%s/%s-labels-idx1-ubyte",dir,tes); unsigned char*teL=load_idx_lab(p,&nl);
    if(cap>0&&cap<ntr)ntr=cap;
    printf("per-sample conv3+MLP: train=%ld test=%ld lr=%.4f epochs=%d aug=%.0f norm=%d nclass=%d\n",ntr,nte,lr,epochs,aug,donorm,nclass);

    /* Conv config: default MED (16/32/64). CN_WIDE=1 -> wide (32/64/128),
     * trainable now that instance norm stabilizes deeper/wider conv. */
    ConvConfig3 ccfg = CONV_MED;
    if(getenv("CN_WIDE") && atoi(getenv("CN_WIDE"))){
        ccfg.K1=32; ccfg.K2=64; ccfg.K3=128;
    }
    if(getenv("CN_K1")) ccfg.K1=atoi(getenv("CN_K1"));
    if(getenv("CN_K2")) ccfg.K2=atoi(getenv("CN_K2"));
    if(getenv("CN_K3")) ccfg.K3=atoi(getenv("CN_K3"));
    ConvNet3*cn=convnet3_create(&ccfg); int D=convnet3_dim(cn);
    printf("conv cfg: K1=%d K2=%d K3=%d fdim=%d\n",ccfg.K1,ccfg.K2,ccfg.K3,D);
    int h1=getenv("CN_H1")?atoi(getenv("CN_H1")):256;
    int h2=getenv("CN_H2")?atoi(getenv("CN_H2")):128;
    MLP*m=mlp_create(D,h1,h2,nclass,0x1234ABCDu);
    float*feat=malloc(D*sizeof(float)),*df=malloc(D*sizeof(float));
    float*zmean=calloc(D,sizeof(float)),*zstd=calloc(D,sizeof(float));

    /* estimate z-norm stats once from 4000 raw-conv features */
    if(donorm){
        long ns=ntr<4000?ntr:4000; float*acc=calloc(D,sizeof(float)),*ac2=calloc(D,sizeof(float)); float im[784];
        for(long i=0;i<ns;i++){const unsigned char*raw=trI+i*784; for(int q=0;q<784;q++)im[q]=(float)(255-raw[q])/255.0f; convnet3_forward(cn,im,feat); for(int d=0;d<D;d++){acc[d]+=feat[d];ac2[d]+=feat[d]*feat[d];}}
        for(int d=0;d<D;d++){zmean[d]=acc[d]/ns; float v=ac2[d]/ns-zmean[d]*zmean[d]; zstd[d]=v>1e-4f?sqrtf(v):1.0f;}
        free(acc);free(ac2);
    } else { for(int d=0;d<D;d++){zmean[d]=0;zstd[d]=1;} }

    long*idx=malloc(ntr*sizeof(long)); for(long i=0;i<ntr;i++)idx[i]=i;
    float im[784],rim[784],z[1024];
    for(int ep=0;ep<epochs;ep++){
        for(long i=ntr-1;i>0;i--){long j=xr()%(i+1);long t=idx[i];idx[i]=idx[j];idx[j]=t;}
        float clr=lr; if(ep>=(int)(epochs*0.4f))clr=lr*0.3f; if(ep>=(int)(epochs*0.7f))clr=lr*0.1f; if(ep>=(int)(epochs*0.9f))clr=lr*0.03f;
        long cor=0;
        for(long ii=0;ii<ntr;ii++){
            long n=idx[ii]; const unsigned char*raw=trI+n*784;
            for(int q=0;q<784;q++)im[q]=(float)(255-raw[q])/255.0f;
            const float*src=im;
            if(aug>0){ float deg=(fr()*2-1)*aug; rot(im,rim,deg); src=rim; }
            convnet3_forward(cn,src,feat);
            for(int d=0;d<D;d++)z[d]=(feat[d]-zmean[d])/zstd[d];
            float sc[64]; mlp_forward(m,z,sc);
            int best=0; for(int k=1;k<nclass;k++)if(sc[k]>sc[best])best=k;
            int lab=trL[n]-laboff; if(best==lab)cor++;
            mlp_zero_grad(m); convnet3_zero_grad(cn);
            mlp_backward(m,z,lab);
            mlp_input_grad(m,z,df);
            for(int d=0;d<D;d++)df[d]/=zstd[d];   /* chain rule through z-norm */
            convnet3_backward(cn,src,feat,df);
            /* per-sample SGD update (separate conv/MLP LR via CN_CONVF).
             * Conv gradient is CLIPPED (per-layer L2 norm) to stop the conv
             * exploding and destroying features in joint training.
             * CN_WD = MLP weight decay (L2 reg) to close the overfit gap. */
            float cvf=getenv("CN_CONVF")?(float)atof(getenv("CN_CONVF")):1.0f;
            float cclip=getenv("CN_CCLIP")?(float)atof(getenv("CN_CCLIP")):1.0f;
            float wd=getenv("CN_WD")?(float)atof(getenv("CN_WD")):0.0f;
            float wdf=1.0f-clr*wd;
            for(int g=0;g<6;g++){MLPLayer L=mlp_layer(m,g); if(wd>0){ for(int k=0;k<L.n;k++){L.param[k]=wdf*L.param[k]-clr*L.grad[k];} } else { for(int k=0;k<L.n;k++)L.param[k]-=clr*L.grad[k]; } }
            for(int g=0;g<convnet3_layer_count(cn);g++){ConvLayer3 L=convnet3_layer(cn,g); float gn=0; for(int k=0;k<L.n;k++)gn+=L.grad[k]*L.grad[k]; gn=sqrtf(gn); float scl=(gn>cclip)?cclip/gn:1.0f; for(int k=0;k<L.n;k++)L.param[k]-=clr*cvf*scl*L.grad[k];}
        }
        /* test each epoch (subset 2000 for speed except last) */
        long tec = (ep==epochs-1)?nte:2000; long tc=0;
        for(long i=0;i<tec;i++){const unsigned char*raw=teI+i*784; for(int q=0;q<784;q++)im[q]=(float)(255-raw[q])/255.0f; convnet3_forward(cn,im,feat); for(int d=0;d<D;d++)z[d]=(feat[d]-zmean[d])/zstd[d]; float sc[64]; mlp_forward(m,z,sc); int best=0; for(int k=1;k<nclass;k++)if(sc[k]>sc[best])best=k; if(best==(teL[i]-laboff))tc++;}
        printf("ep%2d lr=%.4f train_acc=%.2f%% test_acc=%.2f%%\n",ep+1,clr,100.0*cor/ntr,100.0*tc/tec);
        fflush(stdout);
    }
    convnet3_save(cn,"data/conv3.wts");
    printf("saved conv3 -> data/conv3.wts\n");
    return 0;
}
