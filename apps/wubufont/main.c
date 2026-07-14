/* wubufont main -- sfnt (TTF/OTF) -> SVG <font> document.
 * Usage: wubufont_cli <in.ttf> [sample] > out.svg
 * This is the AGI-usable backbone hook: WuBuOS can call this to turn a font
 * into an editable/convertible SVG. Native C11, no deps. */
#include "wubufont.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long file_size(FILE *f) {
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    long n = ftell(f);
    if (n < 0) return -1;
    rewind(f);
    return n;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <in.ttf|otf> [sample]\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    long n = file_size(f);
    if (n <= 0) { fclose(f); fprintf(stderr, "bad file size\n"); return 1; }
    uint8_t *buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return 1; }
    fclose(f);

    Font *font = font_open(buf, (size_t)n);
    if (!font) { free(buf); fprintf(stderr, "not a valid sfnt font\n"); return 1; }

    const char *sample = argc > 2 ? argv[2] : "Ag";
    char *svg = font_to_svg(font, sample);
    if (!svg) { font_free(font); free(buf); return 1; }
    fputs(svg, stdout);
    free(svg);
    font_free(font);
    free(buf);
    return 0;
}
