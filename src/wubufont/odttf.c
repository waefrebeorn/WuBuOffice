/* odttf.c -- H5: embedded-font de-obfuscation for DOCX (word/fonts/*.odttf).
 *
 * ODTTF = a TrueType font whose FIRST 32 bytes are XORed with the 16-byte
 * GUID of the font's relationship, in reversed byte order. Everything after
 * byte 32 is plain sfnt. De-obfuscation restores the standard font header so
 * font_open() can parse it. Clean-room per the ECMA-376 embedded-font part
 * structure; the XOR scheme is documented publicly in multiple places.
 *
 * C11, no deps. */
#include "odttf.h"
#include <stdlib.h>
#include <string.h>

static int hexval(char c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int wubufont_odttf_guid_from_string(const char *guid, uint8_t out[16]){
    if (!guid || !out) return -1;
    /* GUID string form: {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
     * ODTTF key = the GUID bytes in REVERSED order. */
    uint8_t raw[16];
    int n = 0;
    for (const char *p = guid; *p && n < 32; p++){
        int hi = hexval(*p);
        if (hi < 0) continue;
        int lo = -1;
        for (const char *q = p + 1; *q; q++){
            lo = hexval(*q);
            if (lo >= 0){ p = q; break; }
        }
        if (lo < 0) break;
        raw[n++] = (uint8_t)(hi * 16 + lo);
    }
    if (n != 16) return -1;
    for (int i = 0; i < 16; i++) out[i] = raw[15 - i];
    return 0;
}

uint8_t *wubufont_odttf_decode(const uint8_t *data, size_t len,
                               const uint8_t guid[16], size_t *out_len){
    if (!data || len < 32 || !guid || !out_len) return NULL;
    uint8_t *out = malloc(len);
    if (!out) return NULL;
    memcpy(out, data, len);
    /* first 32 bytes: XOR with the reversed GUID, twice through */
    for (int i = 0; i < 32; i++)
        out[i] ^= guid[i % 16];
    *out_len = len;
    return out;
}

/* Convenience: is this blob already a plain sfnt? */
int wubufont_odttf_is_plain_sfnt(const uint8_t *data, size_t len){
    if (!data || len < 4) return 0;
    uint32_t sig = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
                 | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
    return sig == 0x00010000u || sig == 0x74727565u /* true */
        || sig == 0x4F54544Fu /* OTTO */ || sig == 0x74746366u /* ttcf */;
}
