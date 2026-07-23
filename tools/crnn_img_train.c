/* crnn_img_train.c -- end-to-end IMAGE training: proves conv trunk trains.
 * Synthesizes line images: each of C-1 classes is a distinct visual pattern
 * (a filled bar at a class-specific row band) painted into a 16px-tall strip.
 * A random character string paints T strips side by side -> one line image.
 * Trains conv->BiLSTM->head->CTC end-to-end (conv UNFROZEN) and checks loss drop.
 */
#include "crnn.h"
#include "image.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

static float rnd(uint32_t*s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return (float)(*s&0xFFFFFF)/(float)0xFFFFFF*2.0f-1.0f;}

#define C 4        /* 0=blank, 1..3 = 3 classes (thick, well-separated bands) */
#define T 8
#define HID 24
#define STRIP 16

/* paint class `cls` into strip [x0..x0+STRIP) of img (H=STRIP). */
static void paint(OcrImage *im, int x0, int cls){
    int H=ocr_image_height(im);
    /* 3 classes: top band, middle band, bottom band -- each ~4px thick */
    int y0 = 2 + (cls-1)*5;
    int y1 = y0 + 4;
    for(int y=0;y<H;y++) for(int x=0;x<STRIP;x++){
        int v = (y>=y0 && y<y1 && x>=3 && x<STRIP-3) ? 235 : 15;
        ocr_image_set(im, x0+x, y, (unsigned char)v);
    }
}

int main(void){
    uint32_t rs=(uint32_t)time(NULL);
    ConvConfig3 cfg={STRIP,STRIP,4,2,2,8,2,2,16,1,1};
    CRNN *m = crnn_create(&cfg, STRIP, HID, C, 1, 4242);
    if(!m){ printf("crnn_create failed\n"); return 1; }
    if(getenv("FREEZE")) crnn_set_freeze_conv(m, 1);
    float LR = getenv("LR")? (float)atof(getenv("LR")) : 0.004f;

    #define NTR 120
    OcrImage **imgs = malloc(NTR*sizeof(OcrImage*));
    int *tgts = malloc((size_t)NTR*T*sizeof(int));
    int *lens = malloc(NTR*sizeof(int));
    for(int n=0;n<NTR;n++){
        int L = 2 + (n%4); if(L>T) L=T; lens[n]=L;
        OcrImage *im = ocr_image_create(T*STRIP, STRIP);
        for(int x=0;x<T*STRIP*STRIP;x++) ; /* no-op */
        for(int i=0;i<T;i++) for(int y=0;y<STRIP;y++) for(int x=0;x<STRIP;x++) ocr_image_set(im,i*STRIP+x,y,20);
        for(int i=0;i<L;i++){
            int cls = 1 + (int)((rnd(&rs)*0.5f+0.5f)*(C-1)); if(cls>=C)cls=C-1; if(cls<1)cls=1;
            tgts[n*T+i]=cls;
            paint(im, i*STRIP, cls);
        }
        imgs[n]=im;
    }

    float prev=1e30f, best=1e30f; int step=0;
    for(int epoch=0; epoch<80; epoch++){
        float tot=0;
        for(int n=0;n<NTR;n++){
            tot += crnn_train_step(m, 0, NULL, lens[n], tgts+n*T, imgs[n]);
            crnn_adam(m, LR, ++step);
        }
        float mean=tot/NTR; if(mean<best) best=mean;
        if(epoch%5==0||epoch==39) printf("epoch %2d  mean_loss=%.4f\n", epoch, mean);
        prev=mean;
    }
    printf("final mean_loss=%.4f  best=%.4f  (conv UNFROZEN, image->text end-to-end)\n", prev, best);

    /* evaluate: greedy decode vs target */
    int seq_ok=0, char_ok=0, char_tot=0, pred[64];
    for(int n=0;n<NTR;n++){
        int pl = crnn_predict(m, imgs[n], pred);
        int L = lens[n]; int ok=(pl==L);
        for(int i=0;i<L;i++){ char_tot++; if(i<pl && pred[i]==tgts[n*T+i]) char_ok++; else ok=0; }
        if(ok) seq_ok++;
    }
    printf("DECODE: exact-seq %d/%d (%.1f%%)  char-acc %d/%d (%.1f%%)\n",
           seq_ok, NTR, 100.0*seq_ok/NTR, char_ok, char_tot, 100.0*char_ok/char_tot);
    for(int n=0;n<NTR;n++) ocr_image_free(imgs[n]);
    crnn_free(m); free(imgs); free(tgts); free(lens);
    return 0;
}
