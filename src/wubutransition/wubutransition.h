/* wubutransition.h — slide transition model. */
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

#endif
