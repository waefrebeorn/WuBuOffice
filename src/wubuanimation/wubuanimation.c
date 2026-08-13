#include "wubuanimation.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct wubuanimation {
    wubuan_key *keys;
    size_t n, cap;
};

wubuanimation *wubuanimation_create(void) { return (wubuanimation *)calloc(1, sizeof(wubuanimation)); }

void wubuanimation_destroy(wubuanimation *a) {
    if (!a) return;
    free(a->keys);
    free(a);
}

int wubuanimation_add(wubuanimation *a, const char *target, wubuan_type type, double dur, double delay, int repeat) {
    if (!a || !target || dur < 0 || delay < 0) return -1;
    if (a->n == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : 4;
        wubuan_key *nk = (wubuan_key *)realloc(a->keys, nc * sizeof(wubuan_key));
        if (!nk) return -1;
        a->keys = nk; a->cap = nc;
    }
    wubuan_key *k = &a->keys[a->n];
    strncpy(k->target, target, sizeof k->target - 1);
    k->target[sizeof k->target - 1] = '\0';
    k->type = type; k->duration = dur; k->delay = delay; k->repeat = repeat;
    a->n++;
    return 0;
}

size_t wubuanimation_count(const wubuanimation *a) { return a ? a->n : 0; }
const wubuan_key *wubuanimation_get(const wubuanimation *a, size_t i) {
    return (a && i < a->n) ? &a->keys[i] : NULL;
}

/* Ease function per animation kind. Input p in [0,1]; output in [0,1]. */
static double ease(wubuan_type type, double p) {
    switch (type) {
    case WUBU_AN_FADE:    return p;
    case WUBU_AN_APPEAR:  return p >= 1.0 ? 1.0 : 1.0; /* appear is instant at end */
    case WUBU_AN_FLYIN:   return 1.0 - pow(1.0 - p, 3);          /* ease-out cubic */
    case WUBU_AN_BOUNCE: {
        /* overshoot then settle (a simple bounce curve) */
        double x = p * 3.0;
        if (x < 1.0) return 0.5 * x * x;
        if (x < 2.0) { x -= 1.0; return 0.5 + 0.5 * (2*x - x*x); }
        x -= 2.0; return 1.0 - 0.5 * x * x;
    }
    case WUBU_AN_SPIN:    return p; /* rotation angle = p*2pi handled by caller */
    default:              return p;
    }
}

double wubuanimation_progress(const wubuanimation *a, const char *target, double t) {
    if (!a || !target || t < 0) return -1.0;
    for (size_t i = 0; i < a->n; i++) {
        if (strcmp(a->keys[i].target, target) != 0) continue;
        const wubuan_key *k = &a->keys[i];
        double local = t - k->delay;
        if (local < 0.0) return 0.0;
        double dur = (k->duration > 0.0) ? k->duration : 0.0;
        double cycle = dur;
        if (cycle <= 0.0) return 1.0;
        double p = fmod(local, cycle) / cycle;
        if (k->repeat == 0 && local >= dur) return 1.0; /* done, stays */
        return ease(k->type, p);
    }
    return -1.0; /* no such target */
}
