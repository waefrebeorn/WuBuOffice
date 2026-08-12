/* wubuconnector.h — diagram connector (line between shapes) model. */
#ifndef WUBUCONNECTOR_H
#define WUBUCONNECTOR_H
#include <stddef.h>

typedef struct wubuconnector wubuconnector;

/* Add a connector between shape `from` (output port) and shape `to` (input port). */
int wubuconnector_add(wubuconnector *c, const char *from, const char *fromport,
                      const char *to, const char *toport);
size_t wubuconnector_count(const wubuconnector *c);
const char *wubuconnector_from(const wubuconnector *c, size_t i);
const char *wubuconnector_to(const wubuconnector *c, size_t i);

wubuconnector *wubuconnector_create(void);
void wubuconnector_destroy(wubuconnector *c);

#endif
