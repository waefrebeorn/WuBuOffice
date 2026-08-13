/* wubusmartart.h — SmartArt-style diagram model: named layouts (process,
 * cycle, hierarchy, list) with ordered nodes. Plus a real layout pass that
 * computes a box position+size for every node so a renderer can draw it. */
#ifndef WUBUSMARTART_H
#define WUBUSMARTART_H
#include <stddef.h>

typedef enum {
    WUBU_SA_PROCESS = 0, WUBU_SA_CYCLE, WUBU_SA_HIERARCHY, WUBU_SA_LIST
} wubusa_layout;

typedef struct { float x, y, w, h; } wubusa_box;

typedef struct wubusmartart wubusmartart;

wubusmartart *wubusmartart_create(void);
void wubusmartart_destroy(wubusmartart *s);

int wubusmartart_set_layout(wubusmartart *s, wubusa_layout layout);
wubusa_layout wubusmartart_layout(const wubusmartart *s);

int wubusmartart_add_node(wubusmartart *s, const char *text);
size_t wubusmartart_count(const wubusmartart *s);
const char *wubusmartart_node(const wubusmartart *s, size_t i);

/* Compute box rectangles for every node inside a `frame_w` x `frame_h` canvas.
 * Fills `out` (caller-allocated, size >= count). Returns 0 or -1 on bad input.
 * Layouts:
 *   PROCESS  : single horizontal row, evenly spaced.
 *   LIST     : single vertical column, evenly spaced.
 *   CYCLE    : nodes placed on a circle (center, radius from frame).
 *   HIERARCHY: root centered top, remaining nodes fanned in a bottom row. */
int wubusmartart_layout_boxes(const wubusmartart *s, float frame_w, float frame_h,
                              wubusa_box *out, size_t outn);

#endif
