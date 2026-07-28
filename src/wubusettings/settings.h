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

/* UI-26: soft word-wrap toggle (1 on) + editor tab-width in spaces.
 * Consumed by the Editor + Document views so the preference is live. */
int    wubusettings_word_wrap(const WubuSettings *s);     /* 1 = wrap */
void   wubusettings_set_word_wrap(WubuSettings *s, int on);
int    wubusettings_tab_width(const WubuSettings *s);     /* spaces, 1..16 */
void   wubusettings_set_tab_width(WubuSettings *s, int w);

/* UXA-41: high-contrast mode (maximally distinct fg/bg, WCAG-style). 1 on. */
int    wubusettings_high_contrast(const WubuSettings *s);
void   wubusettings_set_high_contrast(WubuSettings *s, int on);

/* DOC-43: prefers-reduced-motion (disable animations/transitions). 1 on. */
int    wubusettings_reduced_motion(const WubuSettings *s);
void   wubusettings_set_reduced_motion(WubuSettings *s, int on);

/* DOC-45: UI chrome scale, independent of document zoom (1.0 = 100%). */
double wubusettings_ui_scale(const WubuSettings *s);
void   wubusettings_set_ui_scale(WubuSettings *s, double us);

/* UI-30: first-run splash flag. 1 on a fresh install; the shell clears it
 * (and persists) once the onboarding overlay is dismissed. */
int  wubusettings_first_run(const WubuSettings *s);
void wubusettings_set_first_run(WubuSettings *s, int on);

/* INT-15: preferred font family (FreeType family_name string; persisted so a
 * restart restores the same face even if font indices shift). "" = default. */
const char *wubusettings_font_family(const WubuSettings *s);
void        wubusettings_set_font_family(WubuSettings *s, const char *family);

/* UI-39: recent-documents jump list (persisted). Returns count, the i-th path,
 * and adds a path (deduped, most-recent-first, capped at 16). */
int  wubusettings_recents_count(const WubuSettings *s);
const char *wubusettings_recent(const WubuSettings *s, int i);
void wubusettings_add_recent(WubuSettings *s, const char *path);

/* Process-wide singleton (lazy). Returns NULL only on OOM. */
WubuSettings *wubusettings_shared(void);

#ifdef __cplusplus
}
#endif

#endif /* WUBUSETTINGS_H */
