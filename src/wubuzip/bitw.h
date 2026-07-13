#ifndef WUBUZIP_BITW_H
#define WUBUZIP_BITW_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LSB-first bit writer (mirror of wubuzip_bitreader). Bits are packed into the
 * current byte low-to-high so the matching reader reconstructs them in order.
 * Self-contained: C standard library only. */
typedef struct wubuzip_bitwriter {
    uint8_t *p;
    size_t len;
    size_t cap;
    uint64_t bitbuf;
    int bitcnt;
} wubuzip_bitwriter;

void wubuzip_bitw_init(wubuzip_bitwriter *w);
void wubuzip_bitw_free(wubuzip_bitwriter *w);

/* Emit the low `n` bits of `v` (1..25), LSB-first. Returns 0 / -1. */
int wubuzip_bitw_put(wubuzip_bitwriter *w, uint32_t v, int n);

/* Flush remaining bits (padding the open byte with zeros) and ensure the
 * output ends on a byte boundary. Returns 0 / -1. */
int wubuzip_bitw_align(wubuzip_bitwriter *w);

/* Append a whole byte unaligned (used for stored-block length fields). */
int wubuzip_bitw_byte(wubuzip_bitwriter *w, uint8_t b);

/* Finalize: flush + null-terminate the growable buffer. On success *out and
 * *out_len own the buffer (caller frees with free()). Returns 0 / -1. */
int wubuzip_bitw_finish(wubuzip_bitwriter *w, uint8_t **out, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_BITW_H */
