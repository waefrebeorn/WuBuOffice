/* wubuanimation.h — keyframe animation engine for slide objects. Stores
 * keyframes and, given a timeline position, computes an eased progress in
 * [0,1] for a target's animation (the renderer uses it to interpolate opacity
 * or transform). Supports appear/fade/fly-in/bounce/spin. */
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

/* Eased progress of `target`'s animation at timeline `t` (s). Returns:
 *   0.0  before its delay,
 *   eased value in (0,1) during the animation,
 *   1.0  after it completes (or 0.0 if it does not repeat, else cycles).
 * Returns -1 on bad input. `progress==1` means fully on-screen. */
double wubuanimation_progress(const wubuanimation *a, const char *target, double t);

#endif
