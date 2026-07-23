/* featprobe.c -- inspect conv3 feature statistics to detect INORM collapse.
 * Build: cc -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc/wubuocr featprobe.c \
 *        src/wubuocr/convnet3.c -lm -o /tmp/featprobe
 * Confirms whether conv features are (a) constant across inputs (dead INORM)
 * or (b) discriminative. */
#include "convnet3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static uint32_t rs=12345;
static float rnd(void){ rs^=rs<<13; rs^=rs>>17; rs^=rs<<5; return (float)(rs&0xFFFFFF)/16777216.0f*2-1; }
int main(void){
    putenv("CN_INORM=1"); putenv("CN_LEAK=0.1");
    ConvConfig3 cfg=CONV_MED_PAD;
    ConvNet3 *cn=convnet3_create(&cfg);
    int D=convnet3_dim(cn);
    printf("D=%d (features)\n", D);
    /* build 8 random "images" and look at feature spread per channel */
    float *feats=malloc((size_t)D*8*sizeof(float));
    for(int s=0;s<8;s++){
        float im[28*28]; for(int i=0;i<28*28;i++) im[i]=rnd()*0.5f+0.5f;
        float imp[32*32]; for(int y=0;y<32;y++) for(int x=0;x<32;x++){
            int sy=y-2,sx=x-2; imp[y*32+x]=(sy>=0&&sy<28&&sx>=0&&sx<28)?im[sy*28+sx]:0;
        }
        convnet3_forward(cn,imp,feats+s*D);
    }
    /* per-channel: mean over 8 samples, and std across samples */
    for(int d=0;d<D;d+=1){
        float m=0; for(int s=0;s<8;s++) m+=feats[s*D+d]; m/=8;
        float v=0; for(int s=0;s<8;s++){float d_=feats[s*D+d]-m; v+=d_*d_;} v/=8;
        if(d<6 || d>D-6) printf("  ch%4d: mean=%.4f std_across_samples=%.4f\n", d, m, sqrtf(v));
    }
    /* overall: how much do features vary between samples? */
    float global_std=0; int cnt=0;
    for(int d=0;d<D;d++){ float m=0; for(int s=0;s<8;s++) m+=feats[s*D+d]; m/=8;
        for(int s=0;s<8;s++){float d_=feats[s*D+d]-m; global_std+=d_*d_; cnt++;} }
    global_std=sqrtf(global_std/cnt);
    printf("Global feature std across 8 random inputs: %.5f\n", global_std);
    printf("(if ~0 -> INORM collapsed to constant; if >>0 -> features vary, conv works)\n");
    convnet3_destroy(cn); free(feats);
    return 0;
}
