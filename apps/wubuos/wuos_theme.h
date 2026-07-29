/* wuos_theme.h -- WuBuOffice shared chrome color tokens + spacing
 * (GUI overhaul, ref/GUI_SPEC.md).
 *
 * Semantic token palette: neutral surfaces differentiated by LIGHTNESS,
 * one brand accent, dark + light variants. Used by main.c (live shell)
 * AND viewshot.c (headless screenshots) so both render identically.
 *
 * Overlay tokens follow GUI_SPEC.md §1/§5/§7:
 *   overlays use SURFACE_3 (raised) + SURFACE_2 (ground) + accent
 *   for active/hover states.  No hard-coded hue jumps.
 *
 * Spacing constants: 4px base scale per M3/Fluent/Atlassian.
 * Clean C11. */
#ifndef WUBUOFFICE_WUOS_THEME_H
#define WUBUOFFICE_WUOS_THEME_H

typedef struct { unsigned char r, g, b; } WuosRGB;

/* ---- 4px spacing scale ---- */
#define WUOS_SPACE_2   2
#define WUOS_SPACE_4   4
#define WUOS_SPACE_8   8
#define WUOS_SPACE_12  12
#define WUOS_SPACE_16  16
#define WUOS_SPACE_20  20
#define WUOS_SPACE_22  22
#define WUOS_SPACE_24  24
#define WUOS_SPACE_26  26
#define WUOS_SPACE_32  32
#define WUOS_SPACE_48  48

/* ---- Dark variant (default) ---- */
#define WUOS_DARK_TAB_BAR     {32, 36, 43}    /* SURFACE_2 */
#define WUOS_DARK_TAB         {42, 47, 56}    /* inactive   */
#define WUOS_DARK_TAB_ON      {54, 60, 71}    /* active     */
#define WUOS_DARK_CONTENT     {235, 237, 240} /* canvas     */
#define WUOS_DARK_TABTEXT     {150, 156, 168} /* dim        */
#define WUOS_DARK_TABTEXT_ON  {232, 235, 240} /* primary    */
#define WUOS_DARK_STATUS      {28, 31, 38}    /* SURFACE_2  */
#define WUOS_DARK_STATUSTX    {178, 183, 192} /* dim text   */
#define WUOS_DARK_BORDER      {60, 66, 78}    /* 1px dividers */
#define WUOS_DARK_ACCENT      {94, 135, 255}  /* brand      */
#define WUOS_DARK_ACCENT_ON   {255, 255, 255}
#define WUOS_DARK_OVERLAY     {26, 29, 36}    /* dialog bg  (SURFACE_3) */
#define WUOS_DARK_OVERLAY_BD  {70, 76, 90}    /* dialog border            */
#define WUOS_DARK_SCRIM       {0, 0, 0}

/* Overlay-specific tokens (dark) */
#define WUOS_DARK_OVERLAY_SURFACE   {36, 40, 47}  /* SFC_3 – raised card */
#define WUOS_DARK_OVERLAY_HIGHLIGHT {94, 135, 255} /* selected row        */
#define WUOS_DARK_OVERLAY_TEXT      {220, 223, 230} /* body text on overlay */
#define WUOS_DARK_OVERLINE_TEXT     {240, 243, 250} /* headings on overlay */
#define WUOS_DARK_OVERLAY_HINTS     {150, 153, 160} /* muted hints */

/* ---- Light variant ---- */
#define WUOS_LIGHT_TAB_BAR     {238, 240, 244}
#define WUOS_LIGHT_TAB         {228, 231, 237}
#define WUOS_LIGHT_TAB_ON      {255, 255, 255}
#define WUOS_LIGHT_CONTENT     {255, 255, 255}
#define WUOS_LIGHT_TABTEXT     {96, 104, 116}
#define WUOS_LIGHT_TABTEXT_ON  {28, 32, 38}
#define WUOS_LIGHT_STATUS      {245, 246, 249}
#define WUOS_LIGHT_STATUSTX    {96, 104, 116}
#define WUOS_LIGHT_BORDER      {208, 213, 221}
#define WUOS_LIGHT_ACCENT      {46, 98, 224}
#define WUOS_LIGHT_ACCENT_ON   {255, 255, 255}
#define WUOS_LIGHT_OVERLAY     {255, 255, 255}
#define WUOS_LIGHT_OVERLAY_BD  {200, 206, 214}
#define WUOS_LIGHT_SCRIM       {0, 0, 0}

/* Overlay-specific tokens (light) */
#define WUOS_LIGHT_OVERLAY_SURFACE   {233, 236, 241} /* SFC_3 */
#define WUOS_LIGHT_OVERLAY_HIGHLIGHT {46, 98, 224}   /* brand tint */
#define WUOS_LIGHT_OVERLAY_TEXT      {28, 32, 38}    /* body */
#define WUOS_LIGHT_OVERLINE_TEXT     {60, 66, 78}    /* headings */
#define WUOS_LIGHT_OVERLAY_HINTS     {100, 110, 124} /* muted hints — AA 5.1:1 on white */

/* ---- Utility: pick dark or light token ---- */
#define WUOS_DARK(x)  ((WuosRGB)WUOS_DARK_##x)
#define WUOS_LIGHT(x) ((WuosRGB)WUOS_LIGHT_##x)
#define WUOS_TOKEN(suffix) \
    ({ int _d = wubusettings_dark(wubusettings_shared()); \
       _d ? WUOS_DARK(suffix) : WUOS_LIGHT(suffix); })

#endif /* WUBUOFFICE_WUOS_THEME_H */
