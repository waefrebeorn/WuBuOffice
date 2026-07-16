/* b64.c -- base64 (RFC 4648) codec (see b64.h). Self-contained C11. */
#include "b64.h"

#include <stdlib.h>
#include <string.h>

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void b64_encode(const uint8_t *in, size_t n, char *out) {
    size_t i = 0, o = 0;
    while (i + 3 <= n) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
        out[o++] = B64[(v >> 18) & 63];
        out[o++] = B64[(v >> 12) & 63];
        out[o++] = B64[(v >> 6) & 63];
        out[o++] = B64[v & 63];
        i += 3;
    }
    size_t rem = n - i;
    if (rem == 1) {
        uint32_t v = (uint32_t)in[i] << 16;
        out[o++] = B64[(v >> 18) & 63];
        out[o++] = B64[(v >> 12) & 63];
        out[o++] = '=';
        out[o++] = '=';
    } else if (rem == 2) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8);
        out[o++] = B64[(v >> 18) & 63];
        out[o++] = B64[(v >> 12) & 63];
        out[o++] = B64[(v >> 6) & 63];
        out[o++] = '=';
    }
    out[o] = '\0';
}

char *b64_of(const uint8_t *data, size_t len) {
    char *o = malloc((len + 2) / 3 * 4 + 1);
    if (!o) return NULL;
    b64_encode(data, len, o);
    return o;
}

static int b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

uint8_t *b64_dec(const char *s, size_t *out_len) {
    size_t n = strlen(s);
    size_t o = 0;
    uint8_t *buf = malloc(n / 4 * 3 + 3);
    if (!buf) return NULL;
    size_t i = 0;
    while (i + 4 <= n) {
        int a = b64_val(s[i]), b = b64_val(s[i + 1]),
            c = b64_val(s[i + 2]), d = b64_val(s[i + 3]);
        if (a < 0 || b < 0) break;
        uint32_t v = ((uint32_t)a << 18) | ((uint32_t)b << 12) |
                     ((c < 0 ? 0 : (uint32_t)c) << 6) | (d < 0 ? 0 : (uint32_t)d);
        buf[o++] = (v >> 16) & 255;
        if (c >= 0) buf[o++] = (v >> 8) & 255;
        if (d >= 0) buf[o++] = v & 255;
        i += 4;
    }
    *out_len = o;
    return buf;
}
