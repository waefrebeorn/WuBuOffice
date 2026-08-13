/* wuos_motion.c — easing + tween implementation. */
#include "wuos_motion.h"

float wuos_ease_linear(float t){ return t < 0 ? 0 : (t > 1 ? 1 : t); }
float wuos_ease_out_quad(float t){
    if (t < 0) return 0;
    if (t > 1) return 1;
    return 1 - (1 - t) * (1 - t);
}
float wuos_ease_in_out_cubic(float t){
    if (t < 0) return 0;
    if (t > 1) return 1;
    return t < 0.5f ? 4*t*t*t : 1 - (float)(-2*t+2)*(-2*t+2)*(-2*t+2)/2;
}
float wuos_ease_out_back(float t){
    /* c1 = 1.70158 (back ease constant), c3 = c1+1; overshoots ~10% */
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    if (t < 0) return 0;
    float x = t > 1 ? 1 : t;
    return 1 + c3 * (x-1)*(x-1)*(x-1) + c1 * (x-1)*(x-1);
}

void wuos_tween_start(WuosTween *tw, float from, float to, float dur, int kind){
    if (!tw) return;
    tw->from = from; tw->to = to;
    tw->dur = dur > 0 ? dur : 0.2f;
    tw->elapsed = 0.0f; tw->active = 1; tw->kind = kind;
}
void wuos_tween_advance(WuosTween *tw, float dt){
    if (!tw || !tw->active) return;
    tw->elapsed += dt;
    if (tw->elapsed >= tw->dur) tw->active = 0;
}
float wuos_tween_value(const WuosTween *tw){
    if (!tw) return 0;
    if (!tw->active) return tw->to;
    float t = tw->dur > 0 ? tw->elapsed / tw->dur : 1;
    if (t > 1) t = 1;
    float e = (tw->kind == 0) ? wuos_ease_linear(t) :
              (tw->kind == 1) ? wuos_ease_out_quad(t) :
              (tw->kind == 2) ? wuos_ease_in_out_cubic(t) : wuos_ease_out_back(t);
    if (e < 0) e = 0;
    return tw->from + (tw->to - tw->from) * e;
}
int wuos_tween_done(const WuosTween *tw){ return !tw || !tw->active; }
