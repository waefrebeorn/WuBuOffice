/* charsetgen.c -- for a given font, scan the WuBuOffice SCRIPT_RANGES and emit
 * the codepoints that actually rasterize to a non-empty glyph as a CHARS=
 * string ("U+XXXX,U+YYYY,..."). This is the per-language alphabet the trainer
 * needs so it doesn't train on the wrong (Latin) charset.
 * Usage: charsetgen FONT.ttf [FONT2.ttf ...]
 * Prints "<font>:<n> U+0041,U+0042,..." per line (n = count). */
#include "image.h"
#include "wubufont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *key; unsigned lo, hi; } ScriptRange;
static const ScriptRange SCRIPT_RANGES[] = {
    {"latin",      0x0041, 0x005A}, {"latin",   0x0061, 0x007A},
    {"latin",      0x00C0, 0x024F},
    {"greek",      0x0370, 0x03FF},
    {"cyrillic",   0x0400, 0x052F},
    {"hebrew",     0x0590, 0x05FF},
    {"arabic",     0x0600, 0x06FF}, {"arabic",   0x0750, 0x077F}, {"arabic", 0x08A0, 0x08FF},
    {"devanagari", 0x0900, 0x097F},
    {"bengali",    0x0980, 0x09FF},
    {"gurmukhi",   0x0A00, 0x0A7F},
    {"gujarati",   0x0A80, 0x0AFF},
    {"oriya",      0x0B00, 0x0B7F},
    {"tamil",      0x0B80, 0x0BFF},
    {"telugu",     0x0C00, 0x0C7F},
    {"kannada",    0x0C80, 0x0CFF},
    {"malayalam",  0x0D00, 0x0D7F},
    {"thai",       0x0E00, 0x0E7F},
    {"lao",        0x0E80, 0x0EFF},
    {"tibetan",    0x0F00, 0x0FFF},
    {"myanmar",    0x1000, 0x109F},
    {NULL,0,0}
};

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
    for(int a=1;a<argc;a++){
        size_t n; uint8_t *fb=my_readf(argv[a],&n);
        Font *font=fb?font_open(fb,n):NULL;
        if(!font){ printf("%s : OPEN FAIL\n", argv[a]); continue; }
        char *out=NULL; size_t cap=0, len=0;
        int cnt=0;
        for(int s=0; SCRIPT_RANGES[s].key; s++){
            for(unsigned cp=SCRIPT_RANGES[s].lo; cp<=SCRIPT_RANGES[s].hi; cp++){
                if(!renders(font,cp)) continue;
                char buf[16]; int bl=snprintf(buf,sizeof buf,"U+%04X,", cp);
                if(len+bl+1>cap){ cap=cap?cap*2:4096; out=realloc(out,cap); }
                memcpy(out+len,buf,bl); len+=bl; cnt++;
            }
        }
        if(out&&len) out[len-1]='\0'; /* strip trailing comma */
        printf("%s:%d %s\n", argv[a], cnt, out?out:"");
        free(out); font_free(font); free(fb);
    }
    return 0;
}
