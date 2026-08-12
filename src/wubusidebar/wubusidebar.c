#include "wubusidebar.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *title; } panel;

struct wubusidebar {
    panel *panels;
    size_t n, cap;
    size_t active;
    int visible;
};

wubusidebar *wubusidebar_create(void) {
    wubusidebar *s = (wubusidebar *)calloc(1, sizeof(wubusidebar));
    if (s) s->visible = 1;
    return s;
}

void wubusidebar_destroy(wubusidebar *s) {
    if (!s) return;
    for (size_t k = 0; k < s->n; k++) free(s->panels[k].title);
    free(s->panels);
    free(s);
}

int wubusidebar_add_panel(wubusidebar *s, const char *title) {
    if (!s || !title) return -1;
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 4;
        panel *np = (panel *)realloc(s->panels, nc * sizeof(panel));
        if (!np) return -1;
        s->panels = np; s->cap = nc;
    }
    s->panels[s->n].title = strdup(title);
    if (!s->panels[s->n].title) return -1;
    s->n++;
    return 0;
}

size_t wubusidebar_count(wubusidebar *s) { return s ? s->n : 0; }
const char *wubusidebar_title(wubusidebar *s, size_t i) {
    return (s && i < s->n) ? s->panels[i].title : NULL;
}

int wubusidebar_set_active(wubusidebar *s, size_t i) {
    if (!s || i >= s->n) return -1;
    s->active = i;
    return 0;
}

size_t wubusidebar_active(wubusidebar *s) { return s ? s->active : 0; }

int wubusidebar_show(wubusidebar *s, int show) {
    if (!s) return -1;
    s->visible = show ? 1 : 0;
    return 0;
}

int wubusidebar_visible(wubusidebar *s) { return s ? s->visible : 0; }
