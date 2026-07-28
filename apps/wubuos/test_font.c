/* test_font.c -- headless unit test for the INT-15 font enumeration/picker.
 * Verifies FreeType init + family scan + runtime family selection. No GUI.
 */
#include "wuos_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void){
    int fails = 0;
    if (wuos_font_init() != 0){ fprintf(stderr, "[font init]\n"); return 1; }

    int n = wuos_font_family_count();
    if (n <= 0){ fprintf(stderr, "[no families enumerated]\n"); fails++; }
    else fprintf(stderr, "[font] %d families enumerated\n", n);

    /* every enumerated family must have a non-empty label */
    for (int i=0;i<n;i++){
        const char *nm = wuos_font_family_name(i);
        if (!nm || !*nm){ fprintf(stderr, "[family %d empty name]\n", i); fails++; }
    }

    /* selecting family 0 must succeed and report current_family == 0 */
    if (n > 0){
        if (wuos_font_set_family(0) != 0){ fprintf(stderr, "[set_family 0]\n"); fails++; }
        else if (wuos_font_current_family() != 0){ fprintf(stderr, "[current_family 0] got %d\n", wuos_font_current_family()); fails++; }
    }

    /* selecting an out-of-range family must fail gracefully */
    if (wuos_font_set_family(99999) != -1){ fprintf(stderr, "[set_family OOB should fail]\n"); fails++; }

    /* drawing must produce a positive advance on a real framebuffer */
    {
        int W=400,H=80; unsigned char *fb = calloc((size_t)W*H*4, 1);
        int adv = wuos_font_draw("Hello", 0, 40, 0, 200,200,200, fb, W, H);
        free(fb);
        if (adv <= 0){ fprintf(stderr, "[draw advance 0]\n"); fails++; }
    }

    /* selecting by name round-trips: pick family k, read its name, find it */
    if (n > 1){
        const char *target = wuos_font_family_name(n-1);
        int found = -1;
        for (int i=0;i<n;i++) if (!strcmp(wuos_font_family_name(i), target)){ found = i; break; }
        if (found < 0 || wuos_font_set_family(found) != 0){ fprintf(stderr, "[set by name]\n"); fails++; }
        else if (wuos_font_current_family() != found){ fprintf(stderr, "[current mismatch]\n"); fails++; }
    }

    wuos_font_quit();
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: font (init, scan, %d families, set/current, draw, OOB-reject)\n", n);
    return 0;
}
