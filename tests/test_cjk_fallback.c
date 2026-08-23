/* test_cjk_fallback.c -- N4: on-screen CJK rasterization via font fallback.
 * Draws CJK text through wuos_font_draw into a framebuffer; asserts
 * non-blank pixels appear (glyphs actually rendered, not tofu/skip). */
#include "../../apps/wubuos/wuos_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

int main(void){
    if (wuos_font_init() != 0){
        fprintf(stderr, "SKIP no fonts\n");
        return 0;
    }
    wuos_font_set_size(24);
    int W = 400, H = 80;
    unsigned char *fb = calloc((size_t)W*H*4, 1);

    /* latin control: must render */
    int n1 = wuos_font_draw("Hello", 10, 50, 0, 255,255,255, fb, W, H);
    (void)n1;
    long lit = 0;
    for (long i = 0; i < (long)W*H*4; i++) if (fb[i]) lit++;
    ck(lit > 200, "latin glyphs rasterize");

    /* CJK: fallback path */
    memset(fb, 0, (size_t)W*H*4);
    int n2 = wuos_font_draw("\xe4\xb8\x96\xe7\x95\x8c", /* 世界 */
                            10, 60, 0, 255,255,255, fb, W, H);
    (void)n2;
    long cjk = 0;
    for (long i = 0; i < (long)W*H*4; i++) if (fb[i]) cjk++;
    printf("[cjk non-blank bytes=%ld]\n", cjk);
    ck(cjk > 500, "CJK glyphs rasterize via fallback");

    /* width measurement also accounts for CJK */
    int w_cjk = wuos_font_text_width("\xe4\xb8\x96\xe7\x95\x8c", 24);
    ck(w_cjk > 20, "CJK width measured");

    free(fb);
    fprintf(stderr, bad ? "CJK_FALLBACK FAIL\n" : "CJK_FALLBACK PASS\n");
    return bad ? 1 : 0;
}
