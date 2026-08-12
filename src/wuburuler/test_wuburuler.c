#include "wuburuler.h"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wuburuler r;
    CK(wuburuler_init(&r, 612, 792) == 0, "letter size");
    double w, h;
    wuburuler_content(&r, &w, &h);
    CK(w == 468 && h == 648, "1in margins -> 468x648");

    CK(wuburuler_set_margins(&r, 36, 36, 36, 36) == 0, "set 0.5in margins");
    wuburuler_content(&r, &w, &h);
    CK(w == 540 && h == 720, "540x720");

    /* margins too big rejected */
    CK(wuburuler_set_margins(&r, 400, 400, 0, 0) == -1, "reject oversize margins");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wuburuler (page size + margins -> content box)\n");
    return 0;
}
