/* exp_png.c -- selection/region -> PNG export. See exp_png.h. */
#include "exp_png.h"
#include "wubupng.h"   /* wubupng_write_file (RGBA fmt = 32) */

int exp_png_write(const char *path, int fmt, const void *pixels, int W, int H){
    if (!path || !pixels || W<=0 || H<=0) return -1;
    return wubupng_write_file(path, fmt, pixels, (uint32_t)W, (uint32_t)H);
}
