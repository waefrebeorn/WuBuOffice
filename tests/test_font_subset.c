/* test_font_subset.c -- N1: glyph subsetting.
 * Subset DejaVuSans to the codepoints used by a small document; assert:
 *  - subset is much smaller than the full font
 *  - subset is a valid sfnt (font_open succeeds)
 *  - kept codepoints still map to glyphs with correct advance widths
 *  - unused-glyph data is gone (glyf table shrinks) */
#include "../../src/wubufont/wubufont.h"
#include "../../src/wubufont/font_subset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

#define FONT "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

int main(void){
    FILE *f = fopen(FONT, "rb");
    ck(f != NULL, "system font found");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *raw = malloc((size_t)sz);
    size_t rd = fread(raw, 1, (size_t)sz, f); fclose(f);
    (void)rd;

    Font *full = font_open_owned(raw, (size_t)sz, 1);
    ck(full != NULL, "full font opens");

    /* document codepoints: latin + digits + CJK test chars */
    uint32_t cps[128]; size_t nc = 0;
    for (char c = 'A'; c <= 'Z'; c++) cps[nc++] = (uint32_t)c;
    for (char c = 'a'; c <= 'z'; c++) cps[nc++] = (uint32_t)c;
    for (char d = '0'; d <= '9'; d++) cps[nc++] = (uint32_t)d;
    cps[nc++] = ' '; cps[nc++] = '.';
    cps[nc++] = 0x4E16;   /* 世 */
    cps[nc++] = 0x754C;   /* 界 */

    uint8_t *sub = NULL; size_t sub_len = 0;
    int rc = wubufont_subset(full, cps, nc, &sub, &sub_len);
    ck(rc == 0 && sub != NULL, "subset produced");
    ck(sub_len > 1000, "subset non-trivial");
    { long sl = (long)sub_len;
      if (!(sl < sz / 4)){ fprintf(stderr,"FAIL subset too big: %ld vs %ld\n", sl, (long)sz); bad++; }
      else fprintf(stderr,"ok   subset is <25%% of full font (%ld vs %ld)\n", sl, (long)sz); }

    /* subset must open as a valid sfnt and keep advances for used cps */
    Font *sf = font_open_owned(sub, sub_len, 1);
    ck(sf != NULL, "subset opens as sfnt");
    if (sf){
        int adv_ok = 1;
        for (size_t i = 0; i < nc && adv_ok; i++){
            int a_full = font_advance(full, cps[i]);
            int a_sub  = font_advance(sf, cps[i]);
            if (a_full > 0 && a_sub != a_full) adv_ok = 0;
        }
        if (!adv_ok){
            for (size_t i2 = 0; i2 < nc; i2++){
                int af = font_advance(full, cps[i2]);
                int as2 = font_advance(sf, cps[i2]);
                if (af != as2){
                    uint16_t gf = font_gid(full, cps[i2]);
                    uint16_t gs2 = font_gid(sf, cps[i2]);
                    fprintf(stderr,"[dbg] U+%04X full=%d sub=%d gid_full=%d gid_sub=%d\n",
                            cps[i2], af, as2, gf, gs2);
                }
            }
        }
        ck(adv_ok, "advances match full font for all used codepoints");
        font_free(sf);
    }

    free(sub);
    font_free(full);
    fprintf(stderr, bad ? "FONT_SUBSET FAIL\n" : "FONT_SUBSET PASS\n");
    return bad ? 1 : 0;
}
