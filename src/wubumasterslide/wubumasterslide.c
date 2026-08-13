#include "wubumasterslide.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

struct wubumasterslide {
    char bg[8];
    double fontsize;
};

wubumasterslide *wubumasterslide_create(void) {
    wubumasterslide *m = (wubumasterslide *)calloc(1, sizeof(wubumasterslide));
    if (m) { strcpy(m->bg, "FFFFFF"); m->fontsize = 28.0; }
    return m;
}

void wubumasterslide_destroy(wubumasterslide *m) { free(m); }

int wubumasterslide_set_theme(wubumasterslide *m, const char *bg, double fontsize) {
    if (!m || !bg || strlen(bg) != 6 || fontsize <= 0) return -1;
    strncpy(m->bg, bg, 6); m->bg[6] = '\0';
    m->fontsize = fontsize;
    return 0;
}

const char *wubumasterslide_background(const wubumasterslide *m) { return m ? m->bg : NULL; }
double wubumasterslide_fontsize(const wubumasterslide *m) { return m ? m->fontsize : 0; }

/* sRGB luminance (0..1) of a 0..255 triple (linearized, correct WCAG math). */
static double lum(unsigned char r, unsigned char g, unsigned char b) {
    double f[3] = { r/255.0, g/255.0, b/255.0 };
    for (int i=0;i<3;i++) f[i] = (f[i] <= 0.03928) ? f[i]/12.92 : pow((f[i]+0.055)/1.055, 2.4);
    return 0.2126*f[0] + 0.7152*f[1] + 0.0722*f[2];
}

int wubumasterslide_resolve(const wubumasterslide *m, wubums_palette *out) {
    if (!m || !out) return -1;
    long c = strtol(m->bg, NULL, 16);
    unsigned char br = (unsigned char)((c >> 16) & 0xff);
    unsigned char bg = (unsigned char)((c >> 8) & 0xff);
    unsigned char bb = (unsigned char)(c & 0xff);

    out->surface[0] = br; out->surface[1] = bg; out->surface[2] = bb;

    /* text: choose dark or light by contrast to surface (WCAG luminance). */
    double L = lum(br, bg, bb);
    if (L > 0.5) { out->text[0]=32; out->text[1]=32; out->text[2]=32; }
    else          { out->text[0]=235; out->text[1]=235; out->text[2]=235; }

    /* accent: fixed high-chroma brand blue {94,135,255} — ONE accent only
     * (Von Restorff), reads on both light and dark surfaces. */
    out->accent[0] = 94; out->accent[1] = 135; out->accent[2] = 255;
    (void)M_PI;
    return 0;
}
