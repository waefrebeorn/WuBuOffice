/* wubuencrypt.h — password-based document encryption (AES-128-CBC + SHA-256
 * key derivation). Clean C11, no external deps. */
#ifndef WUBUENCRYPT_H
#define WUBUENCRYPT_H
#include <stddef.h>

/* Derive a 16-byte AES key + 16-byte IV from a password. Uses SHA-256 of
 * password||salt. salt may be NULL (then zero salt). Returns 0. */
int wubuencrypt_derive(const char *password, const unsigned char salt[16],
                       unsigned char key[16], unsigned char iv[16]);

/* Encrypt plaintext (len bytes) with AES-128-CBC. `len` must be a multiple of
 * 16 (caller pads, typically PKCS7). Writes len bytes to out (may alias in).
 * Returns 0 on success. */
int wubuencrypt_cbc(const unsigned char key[16], const unsigned char iv[16],
                    const unsigned char *in, unsigned char *out, size_t len);

/* Decrypt: inverse of wubuencrypt_cbc. Returns 0 on success. */
int wubuencrypt_decrypt_cbc(const unsigned char key[16], const unsigned char iv[16],
                            const unsigned char *in, unsigned char *out, size_t len);

/* PKCS7 pad / unpad a buffer. pad writes n+padlen bytes to out (caller
 * allocates n+16). Returns padded length. unpad reads padded (padded_len
 * bytes) into out, returns unpadded length or -1 on bad padding. */
size_t wubuencrypt_pkcs7_pad(const unsigned char *in, size_t n, unsigned char *out);
int wubuencrypt_pkcs7_unpad(const unsigned char *in, size_t padded_len, unsigned char *out);

/* Round-trip convenience: returns malloc'd encrypted blob (len+16) or NULL.
 * Caller frees. */
unsigned char *wubuencrypt_document(const char *password, const unsigned char *data, size_t n, size_t *outlen);
/* Decrypt a wubuencrypt_document blob. Returns malloc'd plaintext or NULL on
 * wrong password / bad data. Caller frees. */
unsigned char *wubuencrypt_open(const char *password, const unsigned char *blob, size_t bloblen, size_t *outlen);

#endif
