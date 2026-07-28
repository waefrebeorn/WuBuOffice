/* test_sig.c */
#include "sig.h"
#include "hash.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    const char *doc = "Hello, signed world.";
    const char *key = "secret-key";
    uint8_t sig[32];
    sig_sign(doc, strlen(doc), key, strlen(key), sig);
    CK(sig_verify(doc, strlen(doc), key, strlen(key), sig)==1, "verify good");
    /* tamper */
    char tampered[64]; strcpy(tampered, doc); tampered[0]='h';
    CK(sig_verify(tampered, strlen(tampered), key, strlen(key), sig)==0, "verify tampered fails");
    /* wrong key */
    CK(sig_verify(doc, strlen(doc), "other", 5, sig)==0, "verify wrong key fails");
    char hex[65]; sig_hex(sig, hex); CK(strlen(hex)==64,"hex len");
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: sig (HMAC-SHA256 sign/verify/tamper)\n"); return 0;
}
