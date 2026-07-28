/* wuos_theme.h -- WuBuOffice shared chrome color tokens (GUI overhaul).
 *
 * Mirrors the WuBu GUI Spec (ref/GUI_SPEC.md §1): neutral surfaces
 * differentiated by LIGHTNESS, one brand accent, dark + light variants.
 * Used by the live shell (main.c) AND the headless screenshot tool
 * (viewshot.c) so both render identically. Clean C11. */
#ifndef WUBUOFFICE_WUOS_THEME_H
#define WUBUOFFICE_WUOS_THEME_H

typedef struct { unsigned char r, g, b; } WuosRGB;

/* Dark variant (default for the office shell). */
#define WUOS_DARK_TAB_BAR    {32, 36, 43}    /* chrome ground (SURFACE_2)   */
#define WUOS_DARK_TAB        {42, 47, 56}    /* inactive tab (SURFACE_2+)  */
#define WUOS_DARK_TAB_ON     {54, 60, 71}    /* active tab (SURFACE_3)     */
#define WUOS_DARK_CONTENT    {235, 237, 240} /* editor/doc canvas (light)  */
#define WUOS_DARK_TABTEXT    {150, 156, 168} /* inactive tab text (dim)    */
#define WUOS_DARK_TABTEXT_ON {232, 235, 240} /* active tab text (primary)   */
#define WUOS_DARK_STATUS     {28, 31, 38}    /* status bar (SURFACE_2)      */
#define WUOS_DARK_STATUSTX   {178, 183, 192} /* status text (dim, >=3:1)    */
#define WUOS_DARK_BORDER     {60, 66, 78}    /* 1px dividers               */
#define WUOS_DARK_ACCENT     {94, 135, 255}  /* the single brand accent     */
#define WUOS_DARK_ACCENT_ON  {255, 255, 255}
#define WUOS_DARK_OVERLAY    {26, 29, 36}    /* dialogs/palette/toasts      */
#define WUOS_DARK_OVERLAY_BD {70, 76, 90}
#define WUOS_DARK_SCRIM      {0, 0, 0}

/* Light variant. */
#define WUOS_LIGHT_TAB_BAR    {238, 240, 244}
#define WUOS_LIGHT_TAB        {228, 231, 237}
#define WUOS_LIGHT_TAB_ON     {255, 255, 255}
#define WUOS_LIGHT_CONTENT    {255, 255, 255}
#define WUOS_LIGHT_TABTEXT    {96, 104, 116}
#define WUOS_LIGHT_TABTEXT_ON {28, 32, 38}
#define WUOS_LIGHT_STATUS     {245, 246, 249}
#define WUOS_LIGHT_STATUSTX   {96, 104, 116}
#define WUOS_LIGHT_BORDER     {208, 213, 221}
#define WUOS_LIGHT_ACCENT     {46, 98, 224}
#define WUOS_LIGHT_ACCENT_ON  {255, 255, 255}
#define WUOS_LIGHT_OVERLAY    {255, 255, 255}
#define WUOS_LIGHT_OVERLAY_BD {200, 206, 214}
#define WUOS_LIGHT_SCRIM      {0, 0, 0}

#endif /* WUBUOFFICE_WUOS_THEME_H */
