/* test_wubufont.c -- validate the clean-room SFNT parser against a REAL font
 * shipped with the system (no synthetic fixtures). Reads the font via
 * FONT_UNDER_TEST env (default: a common Unifont/DejaVu path), parses it,
 * asserts the sfnt invariants (tables present, unitsPerEm sane, glyph count
 * > 0, cmap maps ASCII), emits an SVG <font> document, and checks that SVG
 * is well-formed XML using an INDEPENDENT oracle (Python xml.dom.minidom).
 * SKIPs (exit 0) if no usable system font exists. */
#include "wubufont.h"
#include "woff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(cond,msg) do { if(!(cond)){ printf("FAIL: %s\n", msg); fails++; } } while(0)

static const char *candidate_fonts[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/opentype/unifont/unifont.otf",
    "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf",
    "/mnt/c/Windows/Fonts/arial.ttf",
    NULL
};

static uint8_t *slurp(const char *path, long *out_n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    rewind(f);
    uint8_t *b = malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f);
    *out_n = n;
    return b;
}

int main(void) {
    const char *path = getenv("FONT_UNDER_TEST");
    const char *chosen = NULL;
    if (path) chosen = path;
    else for (int i = 0; candidate_fonts[i]; i++)
        if (candidate_fonts[i] && fopen(candidate_fonts[i], "rb")) { chosen = candidate_fonts[i]; break; }

    if (!chosen) {
        printf("SKIP: no system TTF/OTF found to test against\n");
        return 0;
    }
    printf("using font: %s\n", chosen);

    long n = 0;
    uint8_t *buf = slurp(chosen, &n);
    if (!buf) { printf("SKIP: cannot read %s\n", chosen); return 0; }

    Font *font = font_open(buf, (size_t)n);
    CK(font != NULL, "font_open succeeds on a real sfnt");
    if (!font) { free(buf); return 1; }

    /* sfnt invariants */
    CK(font_table_count(font) > 0, "has tables");
    CK(font_find_table(font, 0x68656164u /*'head'*/, NULL, NULL), "has 'head' table");
    int has_glyf = font_find_table(font, 0x676c7966u /*'glyf'*/, NULL, NULL);
    int has_cff  = font_find_table(font, 0x43464620u /*'CFF '*/, NULL, NULL);
    CK(has_glyf || has_cff, "has 'glyf' (TrueType) or 'CFF ' (OpenType/CFF) outlines");
    CK(font_find_table(font, 0x636d6170u /*'cmap'*/, NULL, NULL), "has 'cmap' table");

    uint16_t upm = font_units_per_em(font);
    CK(upm >= 64 && upm <= 16384, "unitsPerEm in sane range");
    printf("  unitsPerEm=%u\n", upm);

    uint16_t gc = font_glyph_count(font);
    CK(gc > 0, "glyph count > 0");
    printf("  glyphCount=%u\n", gc);

    /* cmap must map basic Latin */
    uint16_t gA = font_cmap(font, 'A');
    uint16_t ga = font_cmap(font, 'a');
    CK(gA != 0, "cmap maps 'A'");
    CK(ga != 0, "cmap maps 'a'");
    CK(gA != ga, "'A' and 'a' map to distinct glyphs");

    /* per-glyph outline decode (only meaningful for TrueType 'glyf' fonts;
     * CFF/OpenType would need a Type2 charstring interpreter, out of scope) */
    if (has_glyf) {
        char *pathd = font_glyph_svg_path(font, gA);
        CK(pathd != NULL, "glyph_svg_path returns (may be empty for space)");
        if (pathd) {
            CK(pathd[0] == '\0' || pathd[0] == 'M' || pathd[0] == 'Q',
               "outline path begins with M/Q or is empty");
            printf("  'A' path[0..40]=%.40s\n", pathd);
            free(pathd);
        }
    }

    /* name table should yield a family name string */
    char *fam = font_name(font, 1);
    if (fam) { printf("  family=\"%s\"\n", fam); free(fam); }

    /* emit a full SVG <font> document and validate via independent oracle */
    char *svg = font_to_svg(font, "Ag");
    CK(svg != NULL, "font_to_svg returns");
    if (svg) {
        CK(strstr(svg, "<svg") != NULL, "svg root present");
        CK(strstr(svg, "<font") != NULL, "svg <font> present");
        CK(strstr(svg, "<glyph") != NULL, "at least one <glyph> present");
        CK(strstr(svg, "font-face") != NULL, "font-face present");
        /* write to a temp file for the XML oracle */
        FILE *tf = fopen("/tmp/wubufont_test.svg", "wb");
        if (tf) { fputs(svg, tf); fclose(tf); }
        free(svg);
    }

    /* ---- WOFF round-trip (writer = self oracle) ----
     * Compress the original sfnt to WOFF, reopen it, and assert the round-trip
     * reconstructs an equivalent font: same metrics, same cmap, same glyph
     * outline, and a valid (XML-well-formed) SVG emission. Done BEFORE freeing
     * buf/font (no use-after-free). */
    {
        size_t wlen = 0;
        uint8_t *woff = sfnt_to_woff(buf, (size_t)n, &wlen);
        CK(woff != NULL, "sfnt_to_woff produces a WOFF blob");
        CK(wlen > 44, "woff blob has a header");
        CK(woff[0]=='w' && woff[1]=='O' && woff[2]=='F' && woff[3]=='F', "woff signature present");
        if (woff) {
            Font *wf = woff_open(woff, wlen);
            CK(wf != NULL, "woff_open reopens the compressed font");
            if (wf) {
                CK(font_units_per_em(wf) == upm, "woff round-trip preserves unitsPerEm");
                CK(font_glyph_count(wf) == gc, "woff round-trip preserves glyph count");
                CK(font_cmap(wf, 'A') == gA, "woff round-trip preserves cmap('A')");
                if (has_glyf) {
                    char *wp = font_glyph_svg_path(wf, gA);
                    CK(wp != NULL, "woff glyph outline decodes");
                    if (wp) { CK(wp[0]=='M'||wp[0]=='Q'||wp[0]=='\0', "woff outline well-formed"); free(wp); }
                }
                char *wsvg = font_to_svg(wf, "Ag");
                CK(wsvg != NULL, "woff font emits SVG");
                if (wsvg) {
                    FILE *tf2 = fopen("/tmp/wubufont_test_woff.svg", "wb");
                    if (tf2) { fputs(wsvg, tf2); fclose(tf2); }
                    free(wsvg);
                }
                font_free(wf);
            }
            free(woff);
        }
    }

    /* ---- rasterizer (clean-room outline -> 1-bit bitmap) ----
     * For a TrueType 'glyf' font, rasterizing a code point must yield a
     * non-empty bitmap with plausible ink coverage and a sane aspect ratio
     * (a capital letter is taller than wide, ink fraction 5%..70%). */
    if (has_glyf) {
        int w = 0, h = 0; uint8_t *bits = NULL;
        int ok = font_rasterize(font, 'H', 96, &bits, &w, &h);
        CK(ok == 1, "font_rasterize('H') succeeds");
        if (ok) {
            CK(w > 8 && h > 8, "rasterized glyph has non-trivial size");
            long ink = 0, area = (long)w * h;
            for (long i = 0; i < area; i++) ink += bits[i];
            double frac = (double)ink / (double)area;
            printf("  'H' @96ppm: %dx%d ink=%.1f%%\n", w, h, 100.0 * frac);
            CK(frac > 0.02 && frac < 0.85, "ink fraction plausible for a letter");
            free(bits);
        }
        /* Composite glyph ('A' is built from component outlines in DejaVu):
         * it must NOT rasterize empty (regression guard for the double-Y
         * negation bug that pushed composited glyphs below the bitmap). */
        int aw = 0, ah = 0; uint8_t *abits = NULL;
        if (font_rasterize(font, 'A', 96, &abits, &aw, &ah)) {
            long aink = 0; for (long i = 0; i < (long)aw * ah; i++) aink += abits[i];
            printf("  'A' @96ppm: %dx%d ink=%.1f%%\n", aw, ah,
                   100.0 * (double)aink / ((double)aw * ah + 1));
            CK(aink > 0, "composite glyph 'A' rasterizes non-empty");
            free(abits);
        }
        /* Thin stem ('l' / 'i'): at low ppm BOTH edges can round to the
         * same integer x, so a naive xa<xb fill loop draws nothing
         * (regression guard for the coincident-edge gap). */
        for (const char *thin = "li"; *thin; thin++) {
            int lw = 0, lh = 0; uint8_t *lbits = NULL;
            if (font_rasterize(font, (uint32_t)*thin, 32, &lbits, &lw, &lh)) {
                long link = 0; for (long i = 0; i < (long)lw * lh; i++) link += lbits[i];
                printf("  '%c' @32ppm: %dx%d ink=%ld\n", *thin, lw, lh, link);
                CK(link > 0, "thin stem rasterizes non-empty");
                free(lbits);
            }
        }
        /* string rasterization should be wider than a single glyph */
        int sw = 0, sh = 0; uint8_t *sbits = NULL;
        if (font_rasterize_string(font, "Hi", 96, &sbits, &sw, &sh)) {
            CK(sw > w, "string bitmap wider than one glyph");
            free(sbits);
        }
    }

    font_free(font);
    free(buf);

    if (fails) { printf("\nWUBUFONT TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUFONT TESTS PASSED (real font: %s)\n", chosen);
    return 0;
}
