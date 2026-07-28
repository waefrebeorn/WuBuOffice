/* hash.h -- SHA-256 (FIPS 180-4), standalone. Used by wubusig for document
 * signatures. Opaque over the raw state. */
#ifndef WUBUHASH_H
#define WUBUHASH_H

#include <stddef.h>
#include <stdint.h>

#define WUBUHASH_SHA256_SIZE 32

typedef struct Sha256 Sha256;

Sha256 *sha256_create(void);
void    sha256_destroy(Sha256 *s);
void    sha256_update(Sha256 *s, const void *data, size_t len);
/* Finalize; writes 32 bytes to `out`. */
void    sha256_final(Sha256 *s, uint8_t out[WUBUHASH_SHA256_SIZE]);

/* One-shot helper: hash `len` bytes -> 32-byte digest. */
void    sha256(const void *data, size_t len, uint8_t out[WUBUHASH_SHA256_SIZE]);

/* Hex string (64 chars + NUL) of a digest. `out` must be >= 65 bytes. */
void    hash_hex(const uint8_t *dig, char *out);

#endif /* WUBUHASH_H */
