#include "fixed.h"

void wubuzip_fixed_litlen(uint8_t out[288]) {
    int i = 0;
    for (; i < 144; i++) out[i] = 8;   /* 0..143   -> 8 bits */
    for (; i < 256; i++) out[i] = 9;   /* 144..255 -> 9 bits */
    for (; i < 280; i++) out[i] = 7;   /* 256..279 -> 7 bits */
    for (; i < 288; i++) out[i] = 8;   /* 280..287 -> 8 bits */
}

void wubuzip_fixed_dist(uint8_t out[32]) {
    for (int i = 0; i < 32; i++) out[i] = 5;
}
