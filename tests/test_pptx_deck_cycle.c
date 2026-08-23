/* test_pptx_deck_cycle.c -- N2 end-to-end: multi-slide deck round-trips
 * through write_multi -> read_multi, and slide data survives per-slide. */
#include "../../src/wubuoxml/pptx_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

#define PATH "/tmp/wb_deck.pptx"

int main(void){
    const char *b0[2] = { "agenda", "goals" };
    const char *b1[1] = { "mid content" };
    const char *b2[2] = { "result one", "result two" };
    PptxSlide src[3] = {
        { "Kickoff",  b0, 2 },
        { "Details",  b1, 1 },
        { "Wrap",     b2, 2 },
    };
    ck(wubuoxml_pptx_write_multi(PATH, src, 3) == 0, "write deck");

    PptxSlideData out[8];
    int n = wubuoxml_pptx_read_multi(PATH, out, 8);
    ck(n == 3, "read back 3 slides");
    if (n == 3){
        ck(strcmp(out[0].title, "Kickoff") == 0, "slide 1 title");
        ck(out[0].nbullets == 2 && strcmp(out[0].bullets[1], "goals") == 0,
           "slide 1 bullets");
        ck(strcmp(out[1].title, "Details") == 0 && out[1].nbullets == 1,
           "slide 2 intact");
        ck(strcmp(out[2].title, "Wrap") == 0 &&
           strcmp(out[2].bullets[1], "result two") == 0, "slide 3 bullets");
    }

    fprintf(stderr, bad ? "DECK_CYCLE FAIL\n" : "DECK_CYCLE PASS\n");
    return bad ? 1 : 0;
}
