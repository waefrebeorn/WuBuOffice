#include "show.h"
#include <stdio.h>
#include <stdlib.h>

int wubushow_main(int argc, char **argv) {
    const char *outpath = (argc > 1) ? argv[1] : "WuBuOffice.pptx";
    wubushow_pres *p = wubushow_create();
    wubushow_slide(p, "WuBu Office", "A ground-up C11 SLERM of PowerPoint.");
    wubushow_slide(p, "No Forks", "Every byte written from scratch.");
    wubushow_slide(p, "Next", "Embed in a game. No runtime. No telemetry.");
    if (wubushow_assemble(p, outpath) != 0) { fprintf(stderr, "wubushow: assemble failed\n"); wubushow_free(p); return 1; }
    wubushow_free(p);
    fprintf(stderr, "wubushow: wrote %s\n", outpath);
    return 0;
}
