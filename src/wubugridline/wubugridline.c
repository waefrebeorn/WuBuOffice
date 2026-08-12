#include "wubugridline.h"

int wubugridline_init(wubugridline *g) {
    if (!g) return -1;
    g->show = 1; g->color = 0xFFE0E0E0; g->print = 0;
    return 0;
}

int wubugridline_toggle(wubugridline *g) {
    if (!g) return -1;
    g->show = !g->show;
    return 0;
}

int wubugridline_set(wubugridline *g, int show, int print) {
    if (!g) return -1;
    g->show = show ? 1 : 0;
    g->print = print ? 1 : 0;
    return 0;
}
