/* sig.c -- HMAC-SHA256 document signature. See sig.h. */
#include "sig.h"
#include "hash.h"

#include <string.h>

#define BLK 64

void sig_sign(const void *data, size_t len, const void *key, size_t keylen,
              uint8_t out[32]){
    uint8_t k[BLK]; memset(k, 0, BLK);
    if (keylen > BLK) sha256(key, keylen, k);
    else memcpy(k, key, keylen);
    uint8_t kipad[BLK], kopad[BLK];
    for (int i=0;i<BLK;i++){ kipad[i]=k[i]^0x36; kopad[i]=k[i]^0x5c; }
    uint8_t inner[32];
    Sha256 *s = sha256_create();
    sha256_update(s, kipad, BLK); sha256_update(s, data, len); sha256_final(s, inner);
    sha256_destroy(s);
    Sha256 *o = sha256_create();
    sha256_update(o, kopad, BLK); sha256_update(o, inner, 32); sha256_final(o, out);
    sha256_destroy(o);
    memset(k,0,BLK); memset(kipad,0,BLK); memset(kopad,0,BLK);
}

int sig_verify(const void *data, size_t len, const void *key, size_t keylen,
               const uint8_t sig[32]){
    uint8_t c[32]; sig_sign(data, len, key, keylen, c);
    return memcmp(c, sig, 32)==0;
}

void sig_hex(const uint8_t sig[32], char *out){ hash_hex(sig, out); }
