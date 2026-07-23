/* conv_feat_test.c -- test conv features for different EMNIST inputs.
 * C11, no deps. Compile:
 *   cc -std=c11 -O2 -I src/wubuocr tools/conv_feat_test.c src/wubuocr/convnet3.c -lm -o /tmp/conv_feat_test
 */
#include "convnet3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t read32(FILE *f){ uint32_t v=0; for(int i=0;i<4;i++){ int c=fgetc(f); if(c==EOF) return 0; v=(v<<8)|(unsigned char)c; } return v; }

int main(int argc, char **argv){
    const char *dir = argc>1 ? argv[1] : "data/emnist";
    char path[1024];
    snprintf(path,sizeof path,"%s/emnist-letters-train-images-idx3-ubyte", dir);
    FILE *f = fopen(path,"rb");
    if(!f){ fprintf(stderr,"can't open %s\n",path); return 1; }
    uint32_t magic=read32(f), n=read32(f), rows=read32(f), cols=read32(f);
    printf("magic=%u n=%u rows=%u cols=%u\n",magic,n,rows,cols);
    
    /* read first 10 images */
    unsigned char raw[10][784];
    for(int i=0;i<10;i++) fread(raw[i],1,784,f);
    fclose(f);
    
    /* load labels */
    snprintf(path,sizeof path,"%s/emnist-letters-train-labels-idx1-ubyte", dir);
    f = fopen(path,"rb");
    if(!f){ fprintf(stderr,"can't open %s\n",path); return 1; }
    magic=read32(f); n=read32(f);
    unsigned char lab[10];
    fread(lab,1,10,f);
    fclose(f);
    
    /* create convnet3 */
    ConvConfig3 cfg = CONV_MED;
    ConvNet3 *cn = convnet3_create(&cfg);
    if(!cn){ fprintf(stderr,"create failed\n"); return 1; }
    int D = convnet3_dim(cn);
    printf("feat_dim=%d layers=%d\n", D, convnet3_layer_count(cn));
    
    float ft[1024];
    for(int i=0;i<10;i++){
        float im[784];
        for(int q=0;q<784;q++) im[q]=(float)raw[i][q]/255.0f;
        convnet3_forward(cn, im, ft);
        float sum=0, mn=ft[0], mx=ft[0];
        int nz=0;
        for(int d=0;d<D;d++){ sum+=ft[d]; if(ft[d]<mn)mn=ft[d]; if(ft[d]>mx)mx=ft[d]; if(ft[d]!=0)nz++; }
        float mean=sum/D;
        /* compute variance */
        float var=0; for(int d=0;d<D;d++){ float d2=ft[d]-mean; var+=d2*d2; } var/=D;
        printf("img[%d] lab=%d D=%d sum=%.4f mean=%.4f var=%.4f mn=%.4f mx=%.4f nz=%d\n",
               i, lab[i], D, sum, mean, var, mn, mx, nz);
        /* first 16 features */
        printf("  feat[0..15]:");
        for(int d=0;d<16 && d<D;d++) printf(" %.4f",ft[d]);
        printf("\n");
    }
    
    convnet3_destroy(cn);
    return 0;
}
