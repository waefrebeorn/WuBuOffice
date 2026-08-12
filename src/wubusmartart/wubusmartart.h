/* wubusmartart.h — SmartArt-style diagram model: named layouts (process,
 * cycle, hierarchy, list) with ordered nodes. */
#ifndef WUBUSMARTART_H
#define WUBUSMARTART_H
#include <stddef.h>

typedef enum {
    WUBU_SA_PROCESS = 0, WUBU_SA_CYCLE, WUBU_SA_HIERARCHY, WUBU_SA_LIST
} wubusa_layout;

typedef struct wubusmartart wubusmartart;

wubusmartart *wubusmartart_create(void);
void wubusmartart_destroy(wubusmartart *s);

int wubusmartart_set_layout(wubusmartart *s, wubusa_layout layout);
wubusa_layout wubusmartart_layout(const wubusmartart *s);

int wubusmartart_add_node(wubusmartart *s, const char *text);
size_t wubusmartart_count(const wubusmartart *s);
const char *wubusmartart_node(const wubusmartart *s, size_t i);

#endif
