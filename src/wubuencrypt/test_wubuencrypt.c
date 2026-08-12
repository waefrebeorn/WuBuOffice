#include "wubuencrypt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

/* NIST SP 800-38A F.2.1 CBC-AES128 known-answer test. */
static void test_nist_vector(void){
    const unsigned char key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    const unsigned char iv[16]  = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    const unsigned char pt[16]  = {0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
    const unsigned char ct[16]  = {0x76,0x49,0xab,0xac,0x81,0x19,0xb2,0x46,0xce,0xe9,0x8e,0x9b,0x12,0xe9,0x19,0x7d};
    unsigned char out[16], dec[16];
    CK(wubuencrypt_cbc(key, iv, pt, out, 16) == 0, "nist encrypt rc");
    CK(memcmp(out, ct, 16) == 0, "nist AES-128-CBC known vector");
    wubuencrypt_decrypt_cbc(key, iv, ct, dec, 16);
    CK(memcmp(dec, pt, 16) == 0, "nist decrypt roundtrip");
}

static void test_roundtrip(void){
    const char *pw = "hunter2";
    const unsigned char data[] = "Hello, WuBuOffice document. This is a secret body with padding.";
    size_t n = sizeof data - 1;
    size_t enc_len, dec_len;
    unsigned char *enc = wubuencrypt_document(pw, data, n, &enc_len);
    CK(enc != NULL, "encrypt alloc");
    unsigned char *dec = wubuencrypt_open(pw, enc, enc_len, &dec_len);
    CK(dec != NULL, "decrypt ok");
    if (dec) {
        CK(dec_len == n && memcmp(dec, data, n) == 0, "roundtrip content");
    }
    /* wrong password fails */
    unsigned char *bad = wubuencrypt_open("wrongpw", enc, enc_len, &dec_len);
    CK(bad == NULL, "wrong password rejected");
    free(enc); free(dec); free(bad);
}

static void test_pad(void){
    unsigned char src[3] = {1,2,3};
    unsigned char padded[32], unp[32];
    size_t plen = wubuencrypt_pkcs7_pad(src, 3, padded);
    CK(plen == 16, "pad 3->16");
    CK(padded[3]==13 && padded[15]==13, "pad bytes=13");
    int u = wubuencrypt_pkcs7_unpad(padded, plen, unp);
    CK(u == 3 && unp[0]==1 && unp[2]==3, "unpad 3");
    CK(wubuencrypt_pkcs7_unpad(padded, 10, unp) == -1, "unpad bad len");
}

int main(void){
    test_nist_vector();
    test_pad();
    test_roundtrip();
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuencrypt (AES-128-CBC NIST vector + password roundtrip + PKCS7)\n");
    return 0;
}
