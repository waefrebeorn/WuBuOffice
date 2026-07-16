/* b64.h -- base64 (RFC 4648) codec for binary parts in the document model.
 *
 * Binary parts (zip/docx/cfb streams, font tables, media) are carried as
 * base64 strings in the normalized JSON model so the wubuOS AGI edits
 * documents purely as JSON with no raw binary in the protocol. This is the
 * single shared implementation: reuse it, do not add another.
 *
 * Opaque-free utility: pure functions, no state. */
#ifndef WUBUDOC_B64_H
#define WUBUDOC_B64_H

#include <stddef.h>
#include <stdint.h>

/* Encode `in` (n bytes) into `out`, which the caller must size to
 * ((n + 2) / 3 * 4 + 1) bytes. NUL-terminated on return. */
void b64_encode(const uint8_t *in, size_t n, char *out);

/* Encode and return a malloc'd NUL-terminated string (caller frees), or NULL. */
char *b64_of(const uint8_t *data, size_t len);

/* Decode a base64 string into malloc'd bytes (caller frees). *out_len is set
 * to the decoded byte count. Returns NULL only on OOM. */
uint8_t *b64_dec(const char *s, size_t *out_len);

#endif /* WUBUDOC_B64_H */
