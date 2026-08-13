/* test_motion.c — easing + tween correctness. */
#include "wuos_motion.h"
#include <stdio.h>
#include <math.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ printf("FAIL: %s\n", m); fails++; } }while(0)
#define CLOSE(a,b) (fabsf((a)-(b)) < 0.02f)

int main(void){
    CK(CLOSE(wuos_ease_linear(0.0f),0.0f) && CLOSE(wuos_ease_linear(1.0f),1.0f), "linear endpoints");
    CK(wuos_ease_linear(0.5f)==0.5f, "linear mid");
    CK(CLOSE(wuos_ease_out_quad(0.0f),0.0f) && CLOSE(wuos_ease_out_quad(1.0f),1.0f), "out_quad endpoints");
    CK(wuos_ease_out_quad(0.5f) > 0.5f, "out_quad accelerates early (ease-out)");
    CK(wuos_ease_in_out_cubic(0.0f)==0.0f && CLOSE(wuos_ease_in_out_cubic(1.0f),1.0f), "in_out_cubic endpoints");
    CK(CLOSE(wuos_ease_in_out_cubic(0.5f),0.5f), "in_out_cubic symmetric mid");
    /* out_back overshoots past 1.0 near the end (spring pop) */
    CK(wuos_ease_out_back(0.9f) > 1.0f, "out_back overshoots (delight pop)");

    /* tween: from 0 to 100 over 0.2s, ease_out_quad */
    WuosTween tw = {0};
    wuos_tween_start(&tw, 0, 100, 0.2f, 1);
    CK(wuos_tween_value(&tw)==0.0f, "tween starts at from");
    wuos_tween_advance(&tw, 0.1f);  /* half duration */
    float mid = wuos_tween_value(&tw);
    CK(mid > 0 && mid < 100, "tween mid between");
    CK(mid > 50, "ease-out mid > half (fast early)");
    wuos_tween_advance(&tw, 0.2f);  /* past end */
    CK(!wuos_tween_done(&tw) || CLOSE(wuos_tween_value(&tw),100.0f), "tween reaches target");
    CK(CLOSE(wuos_tween_value(&tw),100.0f), "tween clamps to target");

    /* clamp input */
    CK(wuos_ease_linear(-1.0f)==0.0f, "linear clamps negative");
    CK(wuos_ease_linear(2.0f)==1.0f, "linear clamps >1");

    if (fails == 0) printf("MOTION TESTS PASSED\n");
    else printf("MOTION TESTS FAILED (%d)\n", fails);
    return fails ? 1 : 0;
}
