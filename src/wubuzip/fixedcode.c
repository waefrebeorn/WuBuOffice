#include "fixedcode.h"
#include "fixed.h"

#include <string.h>

static void build_codes(const uint8_t *lengths, int n,
                        uint16_t *code, uint8_t *len) {
    int cnt[16], offs[16];
    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < n; i++) cnt[lengths[i]]++;
    cnt[0] = 0;
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + cnt[i];

    /* canonical codes: within each length, codes are consecutive; the first
     * code for length L is the previous first code shifted left one bit. */
    uint16_t c[16];
    c[0] = 0;
    for (int i = 1; i <= 15; i++) c[i] = (uint16_t)((c[i - 1] + cnt[i - 1]) << 1);

    for (int i = 0; i < n; i++) {
        int L = lengths[i];
        len[i] = (uint8_t)L;
        if (L == 0) { code[i] = 0; continue; }
        code[i] = c[L]++;
    }
}

void wubuzip_fixed_code_init(uint16_t lit_code[288], uint8_t lit_len[288],
                             uint16_t dist_code[32], uint8_t dist_len[32]) {
    uint8_t ll[288], d[32];
    wubuzip_fixed_litlen(ll);
    wubuzip_fixed_dist(d);
    build_codes(ll, 288, lit_code, lit_len);
    build_codes(d, 32, dist_code, dist_len);
}
