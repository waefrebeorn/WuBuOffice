#include "wubuanimation.h"
#include <stdlib.h>
#include <string.h>

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
