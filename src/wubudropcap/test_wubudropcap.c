#include "wubudropcap.h"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubudropcap d;
    CK(wubudropcap_init(&d, 3) == 0, "init 3 lines");
    CK(wubudropcap_lines(&d) == 3, "3 lines active");

    CK(wubudropcap_enable(&d, 5) == 0 && wubudropcap_lines(&d) == 5, "enable 5");
    CK(wubudropcap_disable(&d) == 0 && wubudropcap_lines(&d) == 0, "disable");

    /* invalid range rejected */
    CK(wubudropcap_enable(&d, 1) == -1, "reject 1 line");
    CK(wubudropcap_enable(&d, 9) == -1, "reject 9 lines");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubudropcap (first-letter drop cap, 2-5 line span, enable/disable)\n");
    return 0;
}
