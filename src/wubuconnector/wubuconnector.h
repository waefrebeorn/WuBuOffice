/* wubuconnector.h — diagram connector (line between shapes) model with a real
 * orthogonal route. Given the rectangles of the source and target shapes, it
 * computes a routing that exits the source's right edge and enters the target's
 * left edge via an L-shaped elbow. */
#ifndef WUBUCONNECTOR_H
#define WUBUCONNECTOR_H
#include <stddef.h>

typedef struct { float x, y, w, h; } wubuc_rect;

typedef struct wubuconnector wubuconnector;

wubuconnector *wubuconnector_create(void);
void wubuconnector_destroy(wubuconnector *c);

int wubuconnector_add(wubuconnector *c, const char *from, const char *fromport,
                      const char *to, const char *toport);
size_t wubuconnector_count(const wubuconnector *c);
const char *wubuconnector_from(const wubuconnector *c, size_t i);
const char *wubuconnector_to(const wubuconnector *c, size_t i);

/* Compute a 3-point elbow polyline for connector `i` from the source rect `a`
 * to target rect `b`. Fills p[0..2] = {start, elbow, end}. Returns 0 or -1. */
int wubuconnector_route(const wubuconnector *c, size_t i,
                        const wubuc_rect *a, const wubuc_rect *b,
                        float p[6]);

#endif
