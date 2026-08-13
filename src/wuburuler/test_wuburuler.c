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

    /* REAL engine: resolve to PIXELS at 96 dpi so a ruler can draw it. */
    double x, y, pw, ph;
    CK(wuburuler_content_rect(&r, 96.0, &x, &y, &pw, &ph) == 0, "pixel rect");
    /* 36pt * (96/72) = 48px origin; 540pt * (96/72) = 720px width */
    CK(x == 48.0 && y == 48.0, "pixel origin 48,48");
    CK(pw == 720.0 && ph == 960.0, "pixel size 720x960");
    /* bad dpi rejected */
    CK(wuburuler_content_rect(&r, 0, &x, &y, &pw, &ph) == -1, "reject 0 dpi");

    CK(wuburuler_set_margins(&r, 400, 400, 0, 0) == -1, "reject oversize margins");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wuburuler (page margins -> content box + pixel geometry @dpi)\n");
    return 0;
}
