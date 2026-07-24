/* crnn_reload.c -- load an existing saved model, decode a probe, re-save,
 * reload, decode again. Isolates whether an ON-DISK model survives a
 * load/save/load cycle (catches format drift between old/new binaries). */
#include "crnn.h"
#include "image.h"
#include "wubufont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *my_readf(const char *p, size_t *n){
    FILE *f=fopen(p,"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    uint8_t *b=malloc(sz?sz:1); if(!b){ fclose(f); return NULL; }
    size_t r=fread(b,1,sz,f); fclose(f); *n=r; return b;
}

int main(int argc, char**argv){
    const char *model = argv[1];
    const char *fontp = argv[2];
    const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'-";
    if(!model||!fontp){ printf("usage: %s <model.crnn> <font.ttf>\n", argv[0]); return 1; }

    size_t fn; uint8_t *fb=my_readf(fontp,&fn);
    Font *font=fb?font_open(fb,fn):NULL; if(!font){ printf("font open failed\n"); return 1; }

    CRNN *m=NULL;
    if(!crnn_load(model,&m)||!m){ printf("load failed\n"); return 1; }
    printf("loaded model from %s\n", model);

    /* probe line */
    OcrImage *probe = ocr_image_create(20*8,20);
    for(int y=0;y<20;y++) for(int x=0;x<20*8;x++) ocr_image_set(probe,(size_t)x,(size_t)y,15);
    const char *word="Hello9x"; int L=(int)strlen(word); int PPM=20;
    for(int i=0;i<L;i++){
        uint8_t *bits=NULL; int w=0,h=0;
        font_rasterize(font,(uint32_t)word[i],PPM,&bits,&w,&h);
        if(bits){ int ox=(20-w)/2; if(ox<0)ox=0; int oy=(20-h)/2; if(oy<0)oy=0;
            for(int y=0;y<h;y++) for(int x=0;x<w;x++) if(bits[y*w+x]){
                int px=5+i*20+ox+x, py=oy+y;
                if(px>=0&&px<20*8&&py>=0&&py<20) ocr_image_set(probe,(size_t)px,(size_t)py,235); }
            free(bits); } }
    char a[256]; crnn_recognize(m, probe, chars, a, sizeof a);
    printf("decode A (just loaded) : \"%s\"\n", a);

    if(!crnn_save(m, "/tmp/reload.crnn")){ printf("re-save failed\n"); return 1; }
    crnn_free(m);

    CRNN *m2=NULL;
    if(!crnn_load("/tmp/reload.crnn",&m2)||!m2){ printf("reload failed\n"); return 1; }
    char b[256]; crnn_recognize(m2, probe, chars, b, sizeof b);
    printf("decode B (reloaded)    : \"%s\"\n", b);

    if(strcmp(a,b)==0) printf("RELOAD OK\n"); else printf("RELOAD MISMATCH -> on-disk model corrupted by load/save cycle\n");

    ocr_image_free(probe); crnn_free(m2);
    return 0;
}
