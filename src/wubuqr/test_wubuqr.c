#include "wubuqr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void){
    int size = 0;
    char *qr = wubuqr_render_ascii("HELLO WUBU", &size);
    CK(qr != NULL, "encode text");
    if (qr){
        CK(size >= 21, "min version 1 = 21 modules");
        CK(strlen(qr) == (size_t)size*(size_t)(size+1), "exact ascii size");
        /* top-left finder pattern should be present in first rows */
        CK(strncmp(qr, "#######", 7)==0, "finder pattern corner");
        CK(strchr(qr,'.') != NULL, "has empty modules");
    }
    free(qr);

    /* too long for v1..v7 rejects */
    char big[2000]; memset(big,'A',sizeof big); big[sizeof big-1]=0;
    CK(wubuqr_render_ascii(big, &size) == NULL, "reject oversized payload");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuqr (QR encode -> ASCII matrix, finder pattern present)\n");
    return 0;
}
