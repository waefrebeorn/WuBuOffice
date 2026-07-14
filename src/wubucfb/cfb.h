/* wubucfb -- Microsoft Compound File Binary (MS-CFB / OLE2) container reader.
 *
 * The shared substrate under the legacy binary Office formats: .doc (Word
 * binary), .xls (BIFF8) and .ppt (PowerPoint binary) are all CFB containers
 * holding named streams. This module parses the container and hands back the
 * raw bytes of a named stream; the per-format decoders live on top of it.
 *
 * Clean-room C11, no dependencies. Opaque handle; read-only. */

#ifndef WUBUCFB_CFB_H
#define WUBUCFB_CFB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wubucfb wubucfb;

/* Open a CFB container from an in-memory image. The bytes are copied; the
 * caller may free `data` immediately after. Returns NULL if the image is not
 * a valid CFB container (bad signature / truncated / inconsistent FAT). */
wubucfb *wubucfb_open(const uint8_t *data, size_t len);

/* Release a handle (safe on NULL). */
void wubucfb_close(wubucfb *c);

/* Extract the stream whose directory name equals `name` (case-insensitive,
 * ASCII). On success allocates `*out` (caller frees) with the stream bytes,
 * sets `*out_len`, and returns 0. Returns non-zero if no such stream exists
 * or on allocation failure. */
int wubucfb_read_stream(wubucfb *c, const char *name,
                        uint8_t **out, size_t *out_len);

/* True (1) if a stream with `name` exists. */
int wubucfb_has_stream(wubucfb *c, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCFB_CFB_H */
