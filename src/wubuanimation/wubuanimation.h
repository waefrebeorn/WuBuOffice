/* wubuanimation.h — keyframe animation model for slide objects. */
#ifndef WUBUANIMATION_H
#define WUBUANIMATION_H
#include <stddef.h>

typedef enum {
    WUBU_AN_NONE = 0, WUBU_AN_APPEAR, WUBU_AN_FADE, WUBU_AN_FLYIN,
    WUBU_AN_BOUNCE, WUBU_AN_SPIN
} wubuan_type;

typedef struct {
    char     target[64]; /* name of the animated object */
    wubuan_type type;
    double   duration;
    double   delay;
    int      repeat;
} wubuan_key;

typedef struct wubuanimation wubuanimation;

wubuanimation *wubuanimation_create(void);
void wubuanimation_destroy(wubuanimation *a);

int wubuanimation_add(wubuanimation *a, const char *target, wubuan_type type, double dur, double delay, int repeat);
size_t wubuanimation_count(const wubuanimation *a);
const wubuan_key *wubuanimation_get(const wubuanimation *a, size_t i);

#endif
