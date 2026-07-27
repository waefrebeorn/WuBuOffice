/* wuos_font.h -- shared FreeType text raster helper for the GUI views.
 * One lazily-initialized FreeType lib + a regular/bold face; views call
 * wuos_font_draw() to paint UTF-8 text onto their RGBA framebuffer.
 * UTF-8 safe (decodes codepoints via wububase before rasterizing). */
#ifndef WUOS_FONT_H
#define WUOS_FONT_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  wuos_font_init(void);                 /* returns 0 ok (idempotent) */
void wuos_font_quit(void);

/* Draw UTF-8 `s` at baseline (x,y) onto RGBA fb (fbw*fbh*4), color (r,g,b),
 * blending alpha over existing pixels (assumes opaque bg). Returns advance px. */
int  wuos_font_draw(const char *s, int x, int y, int bold,
                    unsigned char r, unsigned char g, unsigned char b,
                    unsigned char *fb, int fbw, int fbh);

/* Pixel height of the current font. */
int  wuos_font_height(void);

#ifdef __cplusplus
}
#endif
#endif /* WUOS_FONT_H */
