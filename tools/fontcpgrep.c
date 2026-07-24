/* fontcpgrep.c -- for each font given, try rasterizing probe codepoints from
 * several scripts; report which scripts render (non-empty bitmap).
 * Usage: fontcpgrep FONT1 [FONT2 ...] */
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
static int renders(Font *font, uint32_t cp){
    uint8_t *bits=NULL; int w=0,h=0;
    int ok=font_rasterize(font,cp,20,&bits,&w,&h);
    int any=0; if(bits){ for(int i=0;i<w*h;i++) if(bits[i]){ any=1; break; } free(bits); }
    return ok&&any;
}
int main(int argc, char**argv){
    /* script probe table: codepoint -> short label */
    struct { uint32_t cp; const char *name; } probes[] = {
        {0x0041,"Lat"}, {0x0391,"Gre"}, {0x0410,"Cyr"}, {0x03B1,"gre"},
        {0x0430,"cyr"}, {0x0627,"Ara"}, {0x05D0,"Heb"}, {0x0905,"Dev"},
        {0x0E01,"Tha"}, {0x3041,"Jpn"}, {0xAC00,"Kor"}, {0x4E00,"Han"},
        {0x0A95,"Gur"}, {0x0B85,"Tam"}, {0x1E00,"Vie"}, {0x0100,"LatE"}, {0,0}
    };
    for(int a=1;a<argc;a++){
        size_t n; uint8_t *fb=my_readf(argv[a],&n);
        Font *font=fb?font_open(fb,n):NULL;
        if(!font){ printf("%s : OPEN FAIL\n", argv[a]); continue; }
        printf("%s:", argv[a]);
        for(int i=0;probes[i].cp;i++)
            printf(" %s=%d", probes[i].name, renders(font,probes[i].cp));
        printf("\n");
        font_free(font); free(fb);
    }
    return 0;
}
