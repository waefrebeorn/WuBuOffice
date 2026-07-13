#ifndef WUBUZIP_ZIP_H
#define WUBUZIP_ZIP_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque ZIP writer (store / no-compression mode). Produces ZIP containers
 * that conform to APPNOTE.TXT and are accepted by Word / LibreOffice / Excel. */
typedef struct wubuzip_writer wubuzip_writer;

/* Create a writer that emits a ZIP archive to `out` (opened for binary write
 * at offset 0). The writer buffers central-directory metadata until
 * wubuzip_finalize(). */
wubuzip_writer *wubuzip_create(FILE *out);

/* Add a stored (uncompressed) file. `name` is the in-archive path using '/'
 * separators (e.g. "word/document.xml"); a leading '/' is ignored. `data` may
 * be NULL only when size == 0. Returns 0 on success, -1 on I/O error. */
int wubuzip_add(wubuzip_writer *z, const char *name, const void *data, uint32_t size);

/* Write the central directory + end-of-central-directory record, flush, and
 * free the writer. Returns 0 on success, -1 on I/O error. */
int wubuzip_finalize(wubuzip_writer *z);

/* IEEE 802.3 CRC-32 (zlib-compatible). */
uint32_t wubuzip_crc32(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_ZIP_H */
