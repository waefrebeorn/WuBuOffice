#include "wubugridline.h"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubugridline g;
    CK(wubugridline_init(&g) == 0, "init");
    CK(g.show == 1 && g.print == 0, "defaults on/not-print");

    CK(wubugridline_toggle(&g) == 0 && g.show == 0, "toggle off");
    CK(wubugridline_toggle(&g) == 0 && g.show == 1, "toggle on");

    CK(wubugridline_set(&g, 1, 1) == 0 && g.show == 1 && g.print == 1, "set show+print");
    CK(wubugridline_set(&g, 0, 0) == 0 && g.show == 0 && g.print == 0, "set hide");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubugridline (gridline show/print toggle)\n");
    return 0;
}
