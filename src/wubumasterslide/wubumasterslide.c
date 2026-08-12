#include "wubumasterslide.h"
#include <stdlib.h>
#include <string.h>

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
