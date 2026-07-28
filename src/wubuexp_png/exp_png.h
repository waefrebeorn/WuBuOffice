/* exp_png.h -- selection/region -> PNG export (EXP-84). Wraps wubupng to write
 * an RGBA pixel buffer to a PNG file. The view supplies the pixels. */
#ifndef WUBUEXP_PNG_H
#define WUBUEXP_PNG_H

/* Write `pixels` (RGBA, W*H) to `path` as PNG. Returns 0 on success, -1 on
 * error. `fmt` is the wubupng pixel format id (e.g. 4 = RGBA8888). */
int exp_png_write(const char *path, int fmt, const void *pixels, int W, int H);

#endif /* WUBUEXP_PNG_H */
