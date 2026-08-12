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

    wubumasterslide_destroy(m);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubumasterslide (master slide theme: bg color + default font)\n");
    return 0;
}
