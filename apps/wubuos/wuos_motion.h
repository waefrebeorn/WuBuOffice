/* wuos_motion.h — tiny easing + tween helpers for micro-interactions.
 * Self-contained C11. Backs the emotional/animation layer (GUI_EXCELLENCE
 * paradigm 3, GUI_MATHEMATICS paradigm 9): smooth tab underline slide, button
 * press feedback, caret blink. Honors prefers-reduced-motion at the call site. */
#ifndef WUOS_MOTION_H
#define WUOS_MOTION_H

/* Standard easing curves (research-grounded: Material/Base/motion.dev).
 * t is normalized progress in [0,1]; all return eased progress in [0,1]
 * except ease_out_back which overshoots (spring feel, clamp to [0,1.1]). */
float wuos_ease_linear(float t);
float wuos_ease_out_quad(float t);   /* hover: 100-150ms, fast settle */
float wuos_ease_in_out_cubic(float t);/* menus, panels: 200ms, symmetric */
float wuos_ease_out_back(float t);    /* slight overshoot: delight pop */

/* A one-shot tween toward a target over a fixed duration. Call advance each
 * frame with dt (seconds); value() returns the eased current value. */
typedef struct {
    float from, to;
    float dur;        /* seconds */
    float elapsed;
    int   active;
    int   kind;       /* 0 linear, 1 out_quad, 2 in_out_cubic, 3 out_back */
} WuosTween;

void wuos_tween_start(WuosTween *tw, float from, float to, float dur, int kind);
void wuos_tween_advance(WuosTween *tw, float dt);
float wuos_tween_value(const WuosTween *tw);   /* returns eased current */
int   wuos_tween_done(const WuosTween *tw);

#endif /* WUOS_MOTION_H */
