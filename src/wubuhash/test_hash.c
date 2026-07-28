/* test_hash.c */
#include "hash.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    /* FIPS 180-4 known answers */
    uint8_t d[32]; char hex[65];
    sha256("abc", 3, d); hash_hex(d, hex);
    CK(strcmp(hex,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0,"abc");
    sha256("", 0, d); hash_hex(d, hex);
    CK(strcmp(hex,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")==0,"empty");
    sha256("The quick brown fox jumps over the lazy dog", 43, d); hash_hex(d, hex);
    CK(strcmp(hex,"d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592")==0,"fox");
    /* streaming == one-shot */
    Sha256 *s = sha256_create();
    sha256_update(s, "The quick brown fox ", 20);
    sha256_update(s, "jumps over the lazy dog", 23);
    sha256_final(s, d); sha256_destroy(s); hash_hex(d, hex);
    CK(strcmp(hex,"d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592")==0,"streaming");
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: hash (SHA-256 FIPS vectors)\n"); return 0;
}
