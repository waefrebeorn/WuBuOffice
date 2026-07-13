#ifndef WUBUZIP_IO_LE_H
#define WUBUZIP_IO_LE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Little-endian readers used by the ZIP/OPC readers. Static inline so there
 * is no link dependency and no duplicated helper in every TU. */
static inline uint16_t wubuzip_rd16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t wubuzip_rd32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_IO_LE_H */
