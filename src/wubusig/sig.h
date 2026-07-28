/* sig.h -- document digital signature (EXP-90). HMAC-SHA256 over a document
 * blob with a key, producing a detached signature. Verify recomputes the HMAC
 * and compares. Opaque. */
#ifndef WUBUSIG_H
#define WUBUSIG_H
#include <stddef.h>
#include <stdint.h>

/* Sign `data`/`len` with `key`/`keylen`; writes a 32-byte signature to `out`. */
void sig_sign(const void *data, size_t len, const void *key, size_t keylen,
              uint8_t out[32]);
/* Verify: 1 if the signature matches, 0 otherwise. */
int  sig_verify(const void *data, size_t len, const void *key, size_t keylen,
                const uint8_t sig[32]);
/* Hex signature (65 bytes) for display/storage. */
void sig_hex(const uint8_t sig[32], char *out);

#endif /* WUBUSIG_H */
