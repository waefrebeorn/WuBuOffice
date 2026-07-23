/* mlp_real_test.c -- does the MLP learn on REAL EMNIST conv features?
 * Loads EMNIST, runs conv3_forward to get 256-dim features, trains a pure
 * MLP (no conv) on (feat,label) with plain SGD. If acc climbs -> data+MLP OK.
 * Compile:
 *   cc -std=c11 -O2 -D_POSIX_C_SOURCE=200809L -Isrc/wubuocr \
 *     src/wubuocr/convnet3.c src/wubuocr/mlp.c tools/mlp_real_test.c -lm -o /tmp/mlprt
 */
#include "convnet3.h"
#include "mlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long idx_count(const unsigned char *hdr){ return ((long)hdr[4]<<24)|((long)hdr[5]<<16)|((long)hdr[6]<<8)|(long)hdr[7]; }
static int load_idx(const char *p, unsigned char **d, long *c){
    FILE *f=fopen(p,"rb"); if(!f) return -1;
    unsigned char h[16]; if(fread(h,1,16,f)!=16){fclose(f);return -1;}
    long n=idx_count(h); int pic=1;
    if(h[3]==3){ int r=((int)h[8]<<24)|((int)h[9]<<16)|((int)h[10]<<8)|h[11];
                 int cc=((int)h[12]<<24)|((int)h[13]<<16)|((int)h[14]<<8)|h[15]; pic=r*cc; if(pic<1)pic=1; }
    unsigned char *b=malloc((size_t)n*pic); if(!b){fclose(f);return -1;}
    size_t g=fread(b,1,(size_t)n*pic,f); *c=(long)(g/pic); *d=b; fclose(f); return 0;
}

int main(void){
    unsigned char *tr_img=NULL,*tr_lab=NULL; long ntr=0,ntl=0;
    if(load_idx("data/emnist/emnist-letters-train-images-idx3-ubyte",&tr_img,&ntr)){printf("img load fail\n");return 1;}
    if(load_idx("data/emnist/emnist-letters-train-labels-idx1-ubyte",&tr_lab,&ntl)){printf("lab load fail\n");return 1;}
    printf("loaded train: img=%ld lab=%ld\n", ntr, ntl);

    ConvNet3 *cn = convnet3_create(&CONV_MED);
    int D = convnet3_dim(cn);
    MLP *m = mlp_create(D, 64, 64, 26, 0x1234ABCDu);

    /* precompute features for first N samples (conv frozen/random) */
    long N = 8000;
    float *feat = malloc((size_t)N*D*sizeof(float));
    int *lab = malloc((size_t)N*sizeof(int));
    for(long i=0;i<N;i++){
        const unsigned char *raw=tr_img+i*784;
        float im[784]; for(int q=0;q<784;q++) im[q]=(float)raw[q]/255.0f;
        convnet3_forward(cn, im, feat+i*D);
        lab[i] = tr_lab[i]-1; /* 0..25 */
    }

    /* train pure MLP on these features with plain SGD */
    for(int ep=0; ep<10; ep++){
        for(long i=0;i<N;i++){
            mlp_train_step(m, feat+i*D, lab[i]);
            mlp_apply_plain(m, 0.1f);
        }
        /* check train acc */
        long cor=0; for(long i=0;i<N;i++){ float sc[26]; mlp_forward(m,feat+i*D,sc); int best=0; for(int c=1;c<26;c++) if(sc[c]>sc[best])best=c; if(best==lab[i])cor++; }
        printf("ep%d MLP-on-features train_acc=%.2f%%\n", ep+1, 100.0f*cor/N);
    }
    return 0;
}
