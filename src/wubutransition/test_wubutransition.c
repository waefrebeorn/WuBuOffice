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

    /* REAL engine: progress must blend from 0 -> 1 over the duration. */
    CK(wubutransition_set(&t, WUBU_TR_FADE, 1.0, 0, 0) == 0, "fade 1s");
    CK(wubutransition_progress(&t, 0.0) == 0.0, "fade start = 0");
    double mid = wubutransition_progress(&t, 0.5);
    CK(mid > 0.0 && mid < 1.0, "fade midpoint in (0,1)");
    CK(wubutransition_progress(&t, 1.0) == 1.0, "fade end = 1");
    CK(wubutransition_progress(&t, 5.0) == 1.0, "fade clamped after dur");

    /* NONE is instant. */
    CK(wubutransition_set(&t, WUBU_TR_NONE, 1.0, 0, 0) == 0, "set none");
    CK(wubutransition_progress(&t, 0.0) == 1.0, "none instant = 1");

    /* slide uses ease-in-out: midpoint should differ from linear 0.5. */
    CK(wubutransition_set(&t, WUBU_TR_SLIDE, 1.0, 0, 0) == 0, "set slide");
    double sm = wubutransition_progress(&t, 0.5);
    CK(sm > 0.0 && sm < 1.0, "slide midpoint in (0,1)");

    CK(wubutransition_progress(NULL, 0.0) < 0.0, "null guard");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubutransition (slide transition engine: per-frame blend factor)\n");
    return 0;
}
