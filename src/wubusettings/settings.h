/* settings.h -- WuBuOffice persistent configuration (opaque, self-contained).
 *
 * One small module that owns the user's preferences: UI zoom, dark/light
 * theme, autosave interval, UI language, base font size. Backed by a JSON
 * file (~/.wubuos/settings.json) via wubujson. The struct is opaque; callers
 * go through the accessors so the storage format can change freely. C11. */
#ifndef WUBUSETTINGS_H
#define WUBUSETTINGS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WubuSettings WubuSettings;

/* Opaque preferences holder. One instance per process (see settings_shared). */
WubuSettings *wubusettings_create(void);
void          wubusettings_destroy(WubuSettings *s);

/* Load from / save to a JSON file. load() with a NULL path uses the default
 * (~/.wubuos/settings.json) and silently no-ops if absent (factory defaults).
 * save() returns 0 on success. */
int  wubusettings_load(WubuSettings *s, const char *path);
int  wubusettings_save(const WubuSettings *s, const char *path);

/* Accessors (all have sane factory defaults). */
double wubusettings_zoom(const WubuSettings *s);            /* 1.0 = 100% */
void   wubusettings_set_zoom(WubuSettings *s, double z);    /* clamped 0.5..3.0 */
int    wubusettings_dark(const WubuSettings *s);            /* 1 dark, 0 light */
void   wubusettings_set_dark(WubuSettings *s, int dark);
int    wubusettings_autosave_ms(const WubuSettings *s);     /* 0 = disabled */
void   wubusettings_set_autosave_ms(WubuSettings *s, int ms);
const char *wubusettings_language(const WubuSettings *s);   /* e.g. "en" */
void   wubusettings_set_language(WubuSettings *s, const char *lang);
int    wubusettings_font_size(const WubuSettings *s);       /* px */
void   wubusettings_set_font_size(WubuSettings *s, int px);

/* UXA-41: high-contrast mode (maximally distinct fg/bg, WCAG-style). 1 on. */
int    wubusettings_high_contrast(const WubuSettings *s);
void   wubusettings_set_high_contrast(WubuSettings *s, int on);

/* Process-wide singleton (lazy). Returns NULL only on OOM. */
WubuSettings *wubusettings_shared(void);

#ifdef __cplusplus
}
#endif

#endif /* WUBUSETTINGS_H */
