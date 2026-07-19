/* conv_feats.c -- load a trained conv3, dump its 256-d features for the test
 * set to a text file (one row per image, space-separated), so numpy can do
 * the linear-probe / separability analysis. Uses the REAL convnet3 forward
 * (no reimplementation) so the numbers are authoritative. */
#include "convnet3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int load_idx(const char*path, unsigned char**out, long*n, int skip){
    FILE*f=fopen(path,"rb"); if(!f) return -1;
    fseek(f,0,SEEK_END); long fs=ftell(f); fseek(f,0,SEEK_SET);
    unsigned char*hdr=malloc(16); fread(hdr,1,16,f);
    long cnt=((long)hdr[4]<<24)|((long)hdr[5]<<16)|((long)hdr[6]<<8)|hdr[7];
    *n=cnt; *out=malloc((size_t)cnt*784); fread(*out,1,(size_t)cnt*784,f);
    free(hdr); fclose(f); (void)skip; return 0;
}

int main(int argc, char**argv){
    /* argv: <conv.wts> <test-images> <out.txt> */
    if(argc<4){ printf("usage: conv_feats <conv.wts> <test-images> <out.txt>\n"); return 1; }
    ConvNet3 *cn=NULL; ConvConfig3 cfg;
    if(convnet3_load(argv[1],&cn,&cfg)!=0){ printf("load fail\n"); return 1; }
    unsigned char *img=NULL; long n=0;
    if(load_idx(argv[2],&img,&n,16)!=0){ printf("img fail\n"); return 1; }
    int D=convnet3_dim(cn);
    FILE*fo=fopen(argv[3],"w");
    float im[784], ft[1024];
    for(long i=0;i<n;i++){
        const unsigned char *raw=img+(size_t)i*784;
        for(int q=0;q<784;q++) im[q]=(float)(255-raw[q])/255.0f;
        convnet3_forward(cn,im,ft);
        for(int d=0;d<D;d++) fprintf(fo,"%s%g", d?",":"", ft[d]);
        fprintf(fo,"\n");
    }
    fclose(fo); free(img); convnet3_destroy(cn);
    printf("dumped %ld x %d features -> %s\n", n, D, argv[3]);
    return 0;
}
