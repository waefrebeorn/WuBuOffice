/* crnn_train.c -- synthetic sequence-learning sanity test for the CRNN.
 *
 * We synthesize "line" sequences: each target character picks one of C-1 class
 * templates (a random D-dim vector per class, fixed). A sequence of characters
 * produces a sequence of templates; we add light noise so the CRNN must learn
 * the TEMPLATE->CLASS mapping AND the CTC alignment. If loss drops and the
 * decoded output starts matching the target, the whole conv->LSTM->CTC->Adam
 * chain is working end-to-end.
 *
 * This is a capability proof, not a real document dataset (see ROADMAP_OCR.md
 * Phase 2 for real multilingual corpus wiring).
 */
#include "crnn.h"
#include "image.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

static float rnd(uint32_t*s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return (float)(*s&0xFFFFFF)/(float)0xFFFFFF*2.0f-1.0f;}

#define C 11      /* 0=blank, 1..10 = 10 classes */
#define T 12
#define HID 24
#define STRIP 16

int main(void){
    uint32_t rs=(uint32_t)time(NULL);
    /* class templates (D-dim) for classes 1..C-1 */
    CRNN *m0 = crnn_create(&(ConvConfig3){STRIP,STRIP,4,2,2,8,2,2,16,1,1}, STRIP, HID, C, 1, 12345);
    if(!m0){ printf("crnn_create failed\n"); return 1; }
    int D = crnn_feat_dim(m0);
    float (*tmpl)[D] = malloc(sizeof(float)*C*D);
    for(int c=1;c<C;c++) for(int d=0;d<D;d++) tmpl[c][d]=rnd(&rs);

    ConvConfig3 cfg; memset(&cfg,0,sizeof cfg);
    cfg.inH=STRIP; cfg.inW=STRIP; cfg.K1=4; cfg.S1=2; cfg.P1=2;
    cfg.K2=8; cfg.S2=2; cfg.P2=2; cfg.K3=16; cfg.S3=1; cfg.P3=1; /* 3-stage */
    CRNN *m = crnn_create(&cfg, STRIP, HID, C, 1, 12345);
    if(!m){ printf("crnn_create failed\n"); return 1; }
    crnn_free(m0);

    /* generate training examples */
    #define NTR 200
    float *seqs = malloc((size_t)NTR*T*D*sizeof(float));
    int *tgts = malloc((size_t)NTR*T*sizeof(int)); /* max len T; L stored separately */
    int *lens = malloc(NTR*sizeof(int));
    for(int n=0;n<NTR;n++){
        int L = 2 + (n%5); if(L>T) L=T;
        lens[n]=L;
        for(int i=0;i<L;i++){
            int cls = 1 + (int)(rnd(&rs)*0.5f+0.5f*(C-1)); if(cls>=C)cls=C-1; if(cls<1)cls=1;
            tgts[n*T+i]=cls;
            for(int d=0;d<D;d++) seqs[((size_t)n*T + i)*D + d] = tmpl[cls][d] + 0.1f*rnd(&rs);
        }
    }

    float prev=1e30f, best=1e30f; int step=0;
    for(int epoch=0; epoch<60; epoch++){
        float tot=0;
        for(int n=0;n<NTR;n++){
            float *seq = seqs + (size_t)n*T*D;
            tot += crnn_train_step(m, T, seq, lens[n], tgts+n*T, NULL);
            crnn_adam(m, 0.006f, ++step);
        }
        float mean=tot/NTR; if(mean<best) best=mean;
        if(epoch%5==0||epoch==59) printf("epoch %2d  mean_loss=%.4f\n", epoch, mean);
        prev=mean;
    }
    printf("final mean_loss=%.4f  best=%.4f  (start ~21, lower=better)\n", prev, best);
    crnn_free(m);
    free(seqs); free(tgts); free(lens);
    return 0;
}
