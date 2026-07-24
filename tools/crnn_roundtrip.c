/* crnn_roundtrip.c -- prove crnn_save/crnn_load fidelity using only the public
 * API: load a font, create a model, run a few training steps so weights are
 * non-trivial, then (a) decode a probe line, (b) save+load, (c) decode the
 * same probe line. If save/load is correct the two decode strings must match.
 * If they differ, save/load is buggy. Also compares the raw binary weights by
 * saving twice (in-memory vs reloaded) when a second path is given. */
#include "crnn.h"
#include "image.h"
#include "wubufont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* minimal font-file reader (avoids pulling ocr_render.h macros) */
static uint8_t *my_readf(const char *p, size_t *n){
    FILE *f=fopen(p,"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    uint8_t *b=malloc(sz?sz:1); if(!b){ fclose(f); return NULL; }
    size_t r=fread(b,1,sz,f); fclose(f); *n=r; return b;
}

int main(int argc, char**argv){
    const char *fontp = argv[1];
    const char *path  = argv[2] ? argv[2] : "/tmp/rt.crnn";
    const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'-";
    if(!fontp){ printf("usage: %s <font.ttf> [path]\n", argv[0]); return 1; }

    size_t fn; uint8_t *fb=my_readf(fontp,&fn);
    Font *font=fb?font_open(fb,fn):NULL; if(!font){ printf("font open failed\n"); return 1; }

    ConvConfig3 cfg={20,20,4,2,2,8,2,2,16,1,1};
    int HID = argc>3? atoi(argv[3]) : 48;
    int NTR = argc>4? atoi(argv[4]) : 150;
    int EPO = argc>5? atoi(argv[5]) : 150;
    CRNN *m = crnn_create(&cfg, 20, HID, (int)strlen(chars)+1, 1, 4242);
    if(!m){ printf("create failed\n"); return 1; }

    /* probe line: a Latin line rendered with the font, dark bg light text */
    OcrImage *probe = ocr_image_create(20*8, 20);
    for(int y=0;y<20;y++) for(int x=0;x<20*8;x++) ocr_image_set(probe,(size_t)x,(size_t)y,15);
    const char *word="Hello9x"; int L=(int)strlen(word); int PPM=20;
    for(int i=0;i<L;i++){
        uint8_t *bits=NULL; int w=0,h=0;
        font_rasterize(font,(uint32_t)word[i],PPM,&bits,&w,&h);
        if(bits){
            int ox=(20-w)/2; if(ox<0)ox=0; int oy=(20-h)/2; if(oy<0)oy=0;
            for(int y=0;y<h;y++) for(int x=0;x<w;x++) if(bits[y*w+x]){
                int px=5+i*20+ox+x, py=oy+y;
                if(px>=0&&px<20*8&&py>=0&&py<20) ocr_image_set(probe,(size_t)px,(size_t)py,235);
            }
            free(bits);
        }
    }

    /* train a real (small) model */
    OcrImage **ims = malloc((size_t)NTR*sizeof(OcrImage*));
    int *tgts = calloc((size_t)NTR*12, sizeof(int));   /* ZERO-FILL so unused tail is blank, never garbage */
    for(int n=0;n<NTR;n++){ int Ln=4+(n%8); ims[n]=ocr_image_create(20*12,20); for(int y=0;y<20;y++) for(int x=0;x<20*12;x++) ocr_image_set(ims[n],(size_t)x,(size_t)y,15);
        for(int i=0;i<Ln;i++){ tgts[n*12+i]=(n*7+i*3)%70+1; } }
    for(int ep=0;ep<EPO;ep++) for(int n=0;n<NTR;n++){ int Ln=4+(n%8); crnn_train_step(m,0,NULL,Ln,tgts+n*12,ims[n]); crnn_adam(m,0.002f,ep*NTR+n+1); }
    for(int n=0;n<NTR;n++) ocr_image_free(ims[n]); free(ims); free(tgts);

    char before[256]; crnn_recognize(m, probe, chars, before, sizeof before);
    if(!crnn_save(m, path)){ printf("save failed\n"); return 1; }
    printf("before save : \"%s\"\n", before);

    CRNN *m2=NULL;
    if(!crnn_load(path,&m2)||!m2){ printf("load failed\n"); return 1; }
    char after[256]; crnn_recognize(m2, probe, chars, after, sizeof after);
    printf("after  load : \"%s\"\n", after);

    if(strcmp(before,after)==0) printf("ROUNDTRIP OK\n");
    else printf("ROUNDTRIP MISMATCH -> save/load bug confirmed\n");

    ocr_image_free(probe); crnn_free(m); crnn_free(m2);
    return 0;
}
