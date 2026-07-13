#ifndef WUBUZIP_READER_H
#define WUBUZIP_READER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* From-scratch ZIP reader. Parses the central directory and lets you extract
 * parts (store + deflate via wubuzip_inflate). Enough to open real OOXML docs. */

typedef struct wubuzip_entry wubuzip_entry;

typedef struct wubuzip_archive {
    const uint8_t *data;   /* full file bytes */
    size_t len;
    wubuzip_entry *entries;
    size_t n;
} wubuzip_archive;

/* Open an in-memory ZIP. Returns 0 on success, -1 on malformed input. */
int wubuzip_open(const uint8_t *data, size_t len, wubuzip_archive *z);

/* Number of entries. */
size_t wubuzip_count(const wubuzip_archive *z);

/* Entry name (NUL-terminated, valid until archive freed). */
const char *wubuzip_name(const wubuzip_archive *z, size_t i);

/* Find entry index by name (case-sensitive); returns (size_t)-1 if absent. */
size_t wubuzip_find(const wubuzip_archive *z, const char *name);

/* Extract entry `i` into a heap buffer. *out is owned by caller (free it).
 * Returns 0 on success, -1 on error. */
int wubuzip_extract(const wubuzip_archive *z, size_t i, uint8_t **out, size_t *out_len);

/* Free internal structures (does NOT free the data buffer you passed in). */
void wubuzip_close(wubuzip_archive *z);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_READER_H */
