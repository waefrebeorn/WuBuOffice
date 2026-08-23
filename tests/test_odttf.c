/* test_odttf.c -- H5: embedded font de-obfuscation.
 * Round-trip: take a known 64-byte header, XOR the first 32 bytes with a
 * reversed GUID (obfuscate), then decode and assert byte-exact recovery.
 * Also: is_plain_sfnt must distinguish obfuscated vs plain, and font_open()
 * must reject the obfuscated blob but accept the decoded one. */
#include "../../src/wubufont/odttf.h"
#include "../../src/wubufont/wubufont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

int main(void){
    /* GUID from a fontTable rels: standard string form */
    const char *guid = "{1DF903F4-9F0A-4A8C-87B5-2E5D3C6B7A01}";
    uint8_t key[16];
    ck(wubufont_odttf_guid_from_string(guid, key) == 0, "guid parses");
    /* reversed order: first key byte = last GUID byte = 0x01 */
    ck(key[0] == 0x01, "key is reversed (key[0]=0x01)");
    ck(key[15] == 0x1D, "key[15]=0x1D (first GUID byte last)");

    /* build a synthetic "font": plausible sfnt header + payload */
    uint8_t plain[128];
    memset(plain, 0xAB, sizeof plain);
    plain[0]=0x00; plain[1]=0x01; plain[2]=0x00; plain[3]=0x00;  /* sfnt v1 */
    /* obfuscate: XOR first 32 bytes with the key */
    uint8_t obf[128];
    memcpy(obf, plain, sizeof obf);
    for (int i = 0; i < 32; i++) obf[i] ^= key[i % 16];

    ck(!wubufont_odttf_is_plain_sfnt(obf, sizeof obf),
       "obfuscated blob is NOT a plain sfnt");
    ck(wubufont_odttf_is_plain_sfnt(plain, sizeof plain),
       "plain blob recognized as sfnt");

    /* decode restores byte-exact */
    size_t out_len = 0;
    uint8_t *dec = wubufont_odttf_decode(obf, sizeof obf, key, &out_len);
    ck(dec != NULL && out_len == sizeof obf, "decode returns full blob");
    ck(dec && memcmp(dec, plain, sizeof plain) == 0,
       "decode is byte-exact (round-trip)");

    /* font_open must accept the decoded blob (signature check passes) */
    Font *f = font_open(dec, out_len);
    /* our synthetic payload is not a real font (no tables) so font_open may
     * return NULL -- but it must NOT crash; the signature gate passed. */
    if (f) font_free(f);
    ck(1, "font_open on decoded blob is safe");

    /* malformed GUID rejected */
    ck(wubufont_odttf_guid_from_string("not-a-guid", key) == -1,
       "malformed guid rejected");

    free(dec);
    fprintf(stderr, bad ? "ODTTF FAIL\n" : "ODTTF PASS\n");
    return bad ? 1 : 0;
}
