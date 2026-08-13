/* hive.h — data-driven UI configuration ("the hive").
 *
 * Replaces hardcoded static C arrays (toolbar buttons, menu bar, slide deck)
 * with a single declarative JSON template loaded at startup. Separating WHAT
 * is shown from HOW it's rendered (research: config-driven UI — VS Code
 * contribution points, Electron menu templates) means UI structure is data,
 * not code: no recompile to add a toolbar button or menu item, full parity
 * between surfaces by sharing one template, and a single source of truth.
 *
 * Template schema (hive.json):
 * {
 *   "toolbar": [ {"label":"New","cmd":1003}, {"label":"New","cmd":1003,
 *                "accel":"Ctrl+N"}, {"sep":true}, ... ],
 *   "menus": [ {"label":"File","items":[ {"label":"Open...","cmd":1000,
 *              "accel":"Ctrl+O"}, {"sep":true}, ... ]}, ... ],
 *   "slide": { "title":"...", "bullets":["...","..."], "chart":[40,65,50] }
 * }
 *
 * Self-contained C11; uses wubujson (already in-tree). A default template is
 * embedded so the app runs with zero external files; users can point at a
 * custom template via WUBU_HIVE env or ~/.config/wubuos/hive.json. */
#ifndef WUBUOS_HIVE_H
#define WUBUOS_HIVE_H

#include <stddef.h>

/* one toolbar/menu item */
typedef struct {
    const char *label;   /* NULL = separator */
    int         cmd;     /* action id (>=1000 = menu/palette cmd, <1000 = on_key) */
    const char *accel;   /* shortcut hint string, or NULL */
} HiveItem;

typedef struct {
    const char *label;
    HiveItem   *items;   /* array of items (label NULL = separator) */
    size_t      n;
} HiveMenu;

typedef struct HiveToolbar {
    HiveItem *items;     /* toolbar buttons (label NULL = separator) */
    size_t    n;
} HiveToolbar;

typedef struct {
    char    *title;
    char   **bullets;    /* slide bullet text */
    size_t   nbullets;
    double  *chart;      /* bar-chart values */
    size_t   nchart;
} HiveSlide;

typedef struct Hive Hive;

Hive *hive_load(void);                       /* load default then user override */
void  hive_free(Hive *h);

const HiveToolbar *hive_toolbar(const Hive *h);
const HiveMenu    *hive_menus(const Hive *h, size_t *n);
const HiveSlide   *hive_slide(const Hive *h);

#endif /* WUBUOS_HIVE_H */
