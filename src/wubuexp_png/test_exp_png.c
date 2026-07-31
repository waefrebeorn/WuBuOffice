/* test_exp_png.c */
#include "exp_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    int W=8,H=8; uint8_t *px = malloc((size_t)W*H*4);
    for (int i=0;i<W*H;i++){ px[i*4]=255; px[i*4+1]=0; px[i*4+2]=0; px[i*4+3]=255; }
    CK(exp_png_write("/tmp/test_exp_png.png", 32, px, W, H)==0,"write");
    /* verify it produced a PNG (8-byte signature) */
    FILE *f = fopen("/tmp/test_exp_png.png","rb");
    CK(f!=NULL,"file");
    if (f){ uint8_t sig[8]; size_t r=fread(sig,1,8,f); fclose(f);
        CK(r==8 && sig[0]==0x89 && sig[1]=='P' && sig[2]=='N' && sig[3]=='G',"png sig"); }
    /* null-path test must run BEFORE freeing px */
    CK(exp_png_write(NULL,32,px,W,H)==-1,"null path fails");
    free(px);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: exp_png (write + PNG signature)\n"); return 0;
}
