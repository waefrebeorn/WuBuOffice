#ifndef WUBUZIP_CRC_H
#define WUBUZIP_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IEEE 802.3 CRC-32 (reflected, polynomial 0xEDB88320). Identical to zlib's
 * crc32, so it validates against any standard ZIP tool. */
uint32_t wubuzip_crc32(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_CRC_H */
