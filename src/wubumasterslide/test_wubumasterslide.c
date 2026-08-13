#include "wubumasterslide.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubumasterslide *m = wubumasterslide_create();
    CK(strcmp(wubumasterslide_background(m),"FFFFFF")==0, "default white");
    CK(wubumasterslide_fontsize(m) == 28.0, "default 28pt");
    CK(wubumasterslide_set_theme(m,"1F3B8C",24.0) == 0, "set theme");
    CK(strcmp(wubumasterslide_background(m),"1F3B8C")==0 && wubumasterslide_fontsize(m)==24.0, "theme values");
    CK(wubumasterslide_set_theme(m,"XYZ",24.0) == -1, "reject bad color");
    CK(wubumasterslide_set_theme(m,"FFFFFF",0) == -1, "reject 0 size");

    /* REAL engine: resolve a harmonized, drawable palette from the bg. */
    wubums_palette p;
    CK(wubumasterslide_resolve(m, &p) == 0, "resolve palette");
    /* surface must equal the bg we set */
    CK(p.surface[0]==0x1F && p.surface[1]==0x3B && p.surface[2]==0x8C, "surface == bg");
    /* dark bg => light text */
    CK(p.text[0] > 200, "dark bg => light text");
    /* accent is the fixed brand blue, distinct from bg */
    CK(!(p.accent[0]==p.surface[0] && p.accent[1]==p.surface[1] && p.accent[2]==p.surface[2]),
       "accent distinct from surface");

    /* white bg => dark text */
    wubumasterslide_set_theme(m,"FFFFFF",28.0);
    wubums_palette pw;
    wubumasterslide_resolve(m, &pw);
    CK(pw.text[0] < 80, "light bg => dark text");

    CK(wubumasterslide_resolve(NULL, &p) == -1, "null guard");

    wubumasterslide_destroy(m);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubumasterslide (master theme + resolved harmonized palette)\n");
    return 0;
}
