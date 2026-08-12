#include "wubutransition.h"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubutransition t;
    CK(wubutransition_init(&t) == 0, "init");
    CK(t.type == WUBU_TR_NONE && t.advance == 0, "defaults");
    CK(wubutransition_set(&t, WUBU_TR_FADE, 0.5, 1, 2.0) == 0, "set fade auto");
    CK(t.type == WUBU_TR_FADE && t.advance == 1 && t.delay == 2.0, "values");
    CK(wubutransition_set(&t, WUBU_TR_WIPE, -1, 0, 0) == -1, "reject neg speed");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubutransition (slide transition type/speed/advance/delay)\n");
    return 0;
}
