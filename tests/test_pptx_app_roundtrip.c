/* test_pptx_app_roundtrip.c -- hop 18 end-to-end: write a .pptx via the
 * assembler, read it back via wubuoxml_pptx_read (the view's load path),
 * assert title + bullets survive the FULL loop. */
#include "../../src/wubuoxml/pptx_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

#define PATH "/tmp/wb_rt2.pptx"

int main(void){
    const char *bl[3] = { "alpha", "beta & gamma", "delta" };
    ck(wubuoxml_pptx_write(PATH, "Design Review", bl, 3) == 0, "write ok");

    char title[256];
    char bullets[12][96];
    int nb = 0;
    ck(wubuoxml_pptx_read(PATH, title, sizeof title,
                          bullets, 12, &nb) == 0, "read ok");
    ck(strcmp(title, "Design Review") == 0, "title round-trips");
    ck(nb == 3, "bullet count survives");
    if (nb == 3){
        ck(strcmp(bullets[0], "alpha") == 0, "bullet 1 round-trips");
        /* XML-escaped ampersand must decode back */
        ck(strcmp(bullets[1], "beta & gamma") == 0, "escaped bullet decodes");
        ck(strcmp(bullets[2], "delta") == 0, "bullet 3 round-trips");
    }

    fprintf(stderr, bad ? "PPTX_APP_RT FAIL\n" : "PPTX_APP_RT PASS\n");
    return bad ? 1 : 0;
}
