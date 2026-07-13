#include "show.h"
#include <stdio.h>
#include <stdlib.h>

int wubushow_main(int argc, char **argv) {
    const char *outpath = (argc > 1) ? argv[1] : "WuBuOffice.pptx";
    wubushow_pres *p = wubushow_create();
    wubushow_slide(p, "WuBu Office",
        "A ground-up C11 SLERM of PowerPoint.\nEvery byte written from scratch.\nNo forks, no telemetry.");
    wubushow_slide(p, "No Forks",
        "Reference repos audited for format truth only.\nZero external dependencies.\nPOSIX + C11 + nothing else.");
    wubushow_slide(p, "Next",
        "Embed in a game.\nDrive a headless document pipeline.\nShip it inside the engine.");
    if (wubushow_assemble(p, outpath) != 0) {
        fprintf(stderr, "wubushow: assemble failed\n");
        wubushow_free(p);
        return 1;
    }
    wubushow_free(p);
    fprintf(stderr, "wubushow: wrote %s (multi-paragraph slides)\n", outpath);
    return 0;
}
