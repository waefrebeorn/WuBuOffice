/* wubumasterslide.h — master slide / slide layout model + real palette resolve.
 * Beyond storing bg/fontsize, it derives a harmonized 3-stop palette (surface,
 * text, accent) from the master background so a slide renderer has actual
 * drawable colors, not a raw hex string. */
#ifndef WUBUMASTERSLIDE_H
#define WUBUMASTERSLIDE_H
#include <stddef.h>

typedef struct wubumasterslide wubumasterslide;

/* A resolved 3-stop palette (0-255 RGB) for drawing a slide. */
typedef struct {
    unsigned char surface[3]; /* page background */
    unsigned char text[3];    /* primary text on surface */
    unsigned char accent[3];  /* single brand accent (Von Restorff: one accent) */
} wubums_palette;

wubumasterslide *wubumasterslide_create(void);
void wubumasterslide_destroy(wubumasterslide *m);

/* Set the master's background color (RRGGBB) and default font size (pt). */
int wubumasterslide_set_theme(wubumasterslide *m, const char *bg, double fontsize);
const char *wubumasterslide_background(const wubumasterslide *m);
double wubumasterslide_fontsize(const wubumasterslide *m);

/* Resolve a harmonized palette from the current bg. Picks text color by
 * luminance (dark text on light bg, light text on dark bg) and a perceptually
 * distinct accent (hue-rotated from the bg, fixed high chroma). Returns 0. */
int wubumasterslide_resolve(const wubumasterslide *m, wubums_palette *out);

#endif
