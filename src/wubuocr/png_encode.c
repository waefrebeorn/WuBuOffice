/* png_encode.c -- thin wrapper over the shared wubupng encoder.
 *
 * Kept only to preserve the png_encode_gray() API used by the debug tools
 * (gen_realpage, crnn_photo_demo). The real encoder now lives in
 * src/wubupng (single, correct, zlib-deflated RGBA+GRAY8). */
#include "png_encode.h"
#include "wubupng.h"
#include <stdlib.h>

int png_encode_gray(const uint8_t *gray, uint32_t W, uint32_t H,
                    uint8_t **out, size_t *out_len){
    return wubupng_encode(WUBUPNG_GRAY8, gray, W, H, out, out_len);
}
