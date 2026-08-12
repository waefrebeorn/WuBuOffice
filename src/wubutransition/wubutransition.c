#include "wubutransition.h"

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
