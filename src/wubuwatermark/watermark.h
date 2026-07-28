/* watermark.h -- page watermark config (DOC-71): text, angle (deg), opacity
 * (0..1), and an enabled flag. Pure config struct (no document format assumed);
 * the layout consumes it. Opaque. */
#ifndef WUBUWATERMARK_H
#define WUBUWATERMARK_H

typedef struct Watermark Watermark;

Watermark *watermark_create(void);
void       watermark_destroy(Watermark *w);

void watermark_set_text(Watermark *w, const char *text);
void watermark_set_angle(Watermark *w, int degrees);
void watermark_set_opacity(Watermark *w, float opacity);  /* clamped 0..1 */
void watermark_set_enabled(Watermark *w, int on);

const char *watermark_text(const Watermark *w);
int        watermark_angle(const Watermark *w);
float      watermark_opacity(const Watermark *w);
int        watermark_enabled(const Watermark *w);

#endif /* WUBUWATERMARK_H */
