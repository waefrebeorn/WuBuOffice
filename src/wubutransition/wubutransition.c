#include "wubutransition.h"
#include <math.h>

int wubutransition_init(wubutransition *t) {
    if (!t) return -1;
    t->type = WUBU_TR_NONE; t->speed = 1.0; t->advance = 0; t->delay = 0.0;
    return 0;
}

int wubutransition_set(wubutransition *t, wubutr_type type, double speed, int advance, double delay) {
    if (!t || speed < 0 || delay < 0) return -1;
    t->type = type; t->speed = speed; t->advance = advance ? 1 : 0; t->delay = delay;
    return 0;
}

double wubutransition_progress(const wubutransition *t, double elapsed) {
    if (!t || elapsed < 0) return -1.0;
    if (t->type == WUBU_TR_NONE) return 1.0;

    double dur = (t->speed > 0.0) ? t->speed : 0.0;
    double p = (dur > 0.0) ? (elapsed / dur) : 1.0;
    if (p >= 1.0) return 1.0;

    switch (t->type) {
    case WUBU_TR_FADE:
    case WUBU_TR_MORPH:
        /* linear cross-fade (morph shares the same eased envelope) */
        return p;
    case WUBU_TR_SLIDE:
    case WUBU_TR_WIPE:
        /* ease-in-out cubic for a smoother slide/wipe */
        return p < 0.5 ? 4*p*p*p : 1 - pow(-2*p + 2, 3)/2;
    case WUBU_TR_BLINK:
        /* on/off blink: visible during even half-cycles */
        return (fmod(elapsed * 2.0 / (dur > 0 ? dur : 1.0), 2.0) < 1.0) ? 1.0 : 0.0;
    case WUBU_TR_RANDOM: {
        /* deterministic per-call pseudo-random step (stable in [0,1]) */
        long s = (long)(elapsed * 1000.0) ^ 0x9e3779b9L;
        s = (s * 1103515245L + 12345L) & 0x7fffffffL;
        return (double)(s & 0xffff) / 65535.0;
    }
    default:
        return p;
    }
}
