/* wubutransition.h — slide transition engine. Beyond the model it computes the
 * per-frame visual blend factor the renderer uses (0 = fully previous slide,
 * 1 = fully next slide), supporting fade / slide / wipe / blink / morph. */
#ifndef WUBUTRANSITION_H
#define WUBUTRANSITION_H
#include <stddef.h>

typedef enum {
    WUBU_TR_NONE = 0, WUBU_TR_FADE, WUBU_TR_SLIDE, WUBU_TR_WIPE,
    WUBU_TR_BLINK, WUBU_TR_RANDOM, WUBU_TR_MORPH
} wubutr_type;

typedef struct {
    wubutr_type type;    /* transition effect */
    double      speed;   /* seconds */
    int         advance; /* 0=manual, 1=auto */
    double      delay;   /* auto-advance delay (s) */
} wubutransition;

int wubutransition_init(wubutransition *t);
int wubutransition_set(wubutransition *t, wubutr_type type, double speed, int advance, double delay);

/* Compute the blend factor in [0,1] for a given elapsed time `t` (seconds).
 * 0 => previous slide fully visible; 1 => next slide fully visible.
 * For WUBU_TR_NONE returns 1 (instant). Returns -1 on bad input. */
double wubutransition_progress(const wubutransition *t, double elapsed);

#endif
