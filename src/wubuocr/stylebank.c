/* stylebank.c -- 64MB multi-style conv expert bank. See stylebank.h.
 * Plain C11, no deps, single-core scalar. Q6600-class. */
#include "stylebank.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

struct StyleBank {
    int nbest, nroll, nslots;
    ConvNet3 **cn;     /* per-slot conv  (opaque, owned) */
    MLP      **mlp;    /* per-slot mlp   (opaque, owned) */
    int      *kind;   /* SLOT_BEST / SLOT_ROLLING */
    float    *acc;    /* last-known test acc per slot (for promotion) */
    int       din;    /* conv feature dim (= mlp input dim) */
    int       nclass;
};

StyleBank *stylebank_create(int nbest, int nroll, const ConvConfig3 *cfg,
                            int mlp_h1, int mlp_h2, int nclass){
    int nslots = nbest + nroll;
    if (nslots <= 0 || !cfg) return NULL;
    StyleBank *b = (StyleBank*)calloc(1, sizeof(*b));
    if(!b) return NULL;
    b->nbest=nbest; b->nroll=nroll; b->nslots=nslots;
    b->din = cfg->inH * cfg->inW;          /* 28*28 = 784 image */
    b->nclass = nclass;
    b->cn  = calloc((size_t)nslots, sizeof(ConvNet3*));
    b->mlp = calloc((size_t)nslots, sizeof(MLP*));
    b->kind= calloc((size_t)nslots, sizeof(int));
    b->acc = calloc((size_t)nslots, sizeof(float));
    if(!b->cn||!b->mlp||!b->kind||!b->acc){ stylebank_destroy(b); return NULL; }
    for(int i=0;i<nslots;i++){
        b->cn[i]  = convnet3_create(cfg);
        b->mlp[i] = mlp_create(convnet3_dim(b->cn[i]), mlp_h1, mlp_h2, nclass, 0x1234ABCDu + (uint32_t)i*2654435761u);
        b->kind[i]= (i<nbest) ? SLOT_BEST : SLOT_ROLLING;
        if(!b->cn[i]||!b->mlp[i]){ stylebank_destroy(b); return NULL; }
    }
    return b;
}

void stylebank_destroy(StyleBank *b){
    if(!b) return;
    for(int i=0;i<b->nslots;i++){ if(b->cn[i])convnet3_destroy(b->cn[i]); if(b->mlp[i])mlp_destroy(b->mlp[i]); }
    free(b->cn); free(b->mlp); free(b->kind); free(b->acc); free(b);
}

int  stylebank_nslots(const StyleBank *b){ return b?b->nslots:0; }
int  stylebank_nbest(const StyleBank *b){ return b?b->nbest:0; }
int  stylebank_nrolling(const StyleBank *b){ return b?b->nroll:0; }
SlotKind stylebank_slot_kind(const StyleBank *b, int slot){
    if(!b||slot<0||slot>=b->nslots) return SLOT_BEST;
    return (SlotKind)b->kind[slot];
}

int stylebank_load_style(StyleBank *b, int slot,
                         const char *conv_path, const char *mlp_path){
    if(!b||slot<0||slot>=b->nslots) return -1;
    ConvNet3 *c=NULL; ConvConfig3 cfg;
    if(convnet3_load(conv_path,&c,&cfg)!=0) return -1;
    int mdim=0; MLP *m=NULL;
    if(mlp_load(mlp_path,&m,NULL,NULL,&mdim)!=0){ convnet3_destroy(c); return -1; }
    if(!m){ convnet3_destroy(c); return -1; }
    convnet3_destroy(b->cn[slot]); mlp_destroy(b->mlp[slot]);
    b->cn[slot]=c; b->mlp[slot]=m;
    return 0;
}

int stylebank_save_slot(const StyleBank *b, int slot,
                        const char *conv_path, const char *mlp_path){
    if(!b||slot<0||slot>=b->nslots) return -1;
    if(convnet3_save(b->cn[slot],conv_path)!=0) return -1;
    if(mlp_save(b->mlp[slot],NULL,NULL,0,mlp_path)!=0) return -1;
    return 0;
}

int stylebank_slot_forward(const StyleBank *b, int slot, const float *img,
                           float *out_scores){
    if(!b||slot<0||slot>=b->nslots) return -1;
    float feat[1024];
    convnet3_forward(b->cn[slot], img, feat);
    mlp_forward(b->mlp[slot], feat, out_scores);
    return 0;
}

int stylebank_forward(const StyleBank *b, const float *img, float *out_scores){
    if(!b) return -1;
    for(int c=0;c<b->nclass;c++) out_scores[c]=0;
    float sc[64];
    int n=0;
    for(int i=0;i<b->nslots;i++){
        stylebank_slot_forward(b,i,img,sc);
        for(int c=0;c<b->nclass;c++) out_scores[c]+=sc[c];
        n++;
    }
    if(n>0) for(int c=0;c<b->nclass;c++) out_scores[c]/=(float)n;
    return 0;
}

/* ---- per-slot training (reuses proven conv3+mlp math, single-threaded) ---- */
static int load_idx_simple(const char*path, unsigned char**out, long*n){
    FILE*f=fopen(path,"rb"); if(!f) return -1;
    fseek(f,0,SEEK_END); /* size not needed; rewind below */ fseek(f,0,SEEK_SET);
    unsigned char*hdr=malloc(16); fread(hdr,1,16,f);
    long cnt=((long)hdr[4]<<24)|((long)hdr[5]<<16)|((long)hdr[6]<<8)|hdr[7];
    *n=cnt; *out=malloc((size_t)cnt*784); fread(*out,1,(size_t)cnt*784,f);
    free(hdr); fclose(f); return 0;
}

int stylebank_train_slot(StyleBank *b, int slot, const char *data_dir,
                         const char *stem, int epochs, long cap){
    if(!b||slot<0||slot>=b->nslots) return -1;
    char pim[1024],plb[1024],vim[1024],vlb[1024];
    snprintf(pim,sizeof pim,"%s/%s-images-idx3-ubyte",data_dir,stem);
    snprintf(plb,sizeof plb,"%s/%s-labels-idx1-ubyte",data_dir,stem);
    snprintf(vim,sizeof vim,"%s/%s-images-idx3-ubyte",data_dir,stem); /* test = train stem + -t10k below */
    snprintf(vlb,sizeof vlb,"%s/%s-labels-idx1-ubyte",data_dir,stem);
    /* If stem has no -train suffix, assume caller passed full stem and we
     * train on the given file (smoke usage). Keep simple. */
    unsigned char *img=NULL,*lab=NULL; long n=0;
    if(load_idx_simple(pim,&img,&n)!=0) return -1;
    if(load_idx_simple(plb,&lab,&n)!=0){ free(img); return -1; }
    long ntr = (cap>0 && cap<n) ? cap : n;
    ConvNet3 *cn = b->cn[slot];
    MLP *m = b->mlp[slot];
    for(int ep=0;ep<epochs;ep++){
        for(long k=0;k<ntr;k++){
            int labv = lab[k]; if(labv<0||labv>=b->nclass) continue;
            const unsigned char *raw=img+(size_t)k*784;
            float im[784]; for(int q=0;q<784;q++) im[q]=(float)(255-raw[q])/255.0f;
            float feat[1024]; convnet3_forward(cn,im,feat);
            float sc[64]; mlp_forward(m,feat,sc); mlp_backward(m,feat,labv);
            float df[1024]; mlp_input_grad(m,feat,df);
            convnet3_backward(cn,im,feat,df);
            /* plain SGD step on this slot (slow-conv cure: small conv LR) */
            float lr=0.05f, clr=lr*0.01f;
            for(int g=0;g<6;g++){ MLPLayer L=mlp_layer(m,g); for(int i=0;i<L.n;i++) L.param[i]-=lr*L.grad[i]; }
            for(int g=0;g<convnet3_layer_count(cn);g++){ ConvLayer3 L=convnet3_layer(cn,g); for(int i=0;i<L.n;i++) L.param[i]-=clr*L.grad[i]; }
            convnet3_zero_grad(cn); mlp_zero_grad(m);
        }
    }
    free(img); free(lab);
    return 0;
}

float stylebank_slot_acc(const StyleBank *b, int slot,
                         const char *data_dir, const char *stem, int label_off){
    if(!b||slot<0||slot>=b->nslots) return 0;
    char pim[1024],plb[1024];
    snprintf(pim,sizeof pim,"%s/%s-images-idx3-ubyte",data_dir,stem);
    snprintf(plb,sizeof plb,"%s/%s-labels-idx1-ubyte",data_dir,stem);
    unsigned char *img=NULL,*lab=NULL; long n=0;
    if(load_idx_simple(pim,&img,&n)!=0) return 0;
    if(load_idx_simple(plb,&lab,&n)!=0){ free(img); return 0; }
    long correct=0; float sc[64];
    for(long i=0;i<n;i++){
        int labv=lab[i]-label_off; if(labv<0||labv>=b->nclass) continue;
        float im[784]; for(int q=0;q<784;q++) im[q]=(float)(255-img[i*784+q])/255.0f;
        stylebank_slot_forward(b,slot,im,sc);
        int best=0; for(int c=1;c<b->nclass;c++) if(sc[c]>sc[best])best=c;
        if(best==labv) correct++;
    }
    free(img); free(lab);
    return 100.0f*(float)correct/(float)n;
}

int stylebank_promote(StyleBank *b, const char *data_dir, const char *stem,
                      int label_off){
    if(!b||b->nroll==0||b->nbest==0) return -1;
    /* find best rolling slot by acc */
    int best_roll=-1; float best_acc=-1;
    for(int i=b->nbest;i<b->nslots;i++){
        float a=stylebank_slot_acc(b,i,data_dir,stem,label_off);
        b->acc[i]=a;
        if(a>best_acc){ best_acc=a; best_roll=i; }
    }
    if(best_roll<0) return -1;
    /* find weakest best slot */
    int weak_best=-1; float weak_acc=1e9f;
    for(int i=0;i<b->nbest;i++){
        if(b->acc[i]<weak_acc){ weak_acc=b->acc[i]; weak_best=i; }
    }
    if(weak_best<0) weak_best=0;
    /* promote: move the rolling slot's models into the weak best position,
     * and reset the rolling slot to a fresh init (it will be retrained). */
    ConvNet3 *ct=b->cn[best_roll]; MLP *mt=b->mlp[best_roll];
    b->cn[best_roll]=b->cn[weak_best]; b->mlp[best_roll]=b->mlp[weak_best];
    b->cn[weak_best]=ct; b->mlp[weak_best]=mt;
    b->acc[weak_best]=best_acc;
    return weak_best;
}
