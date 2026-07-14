/* cfb_write.h -- Microsoft Compound File Binary (MS-CFB / OLE2) container
 * WRITER. Counterpart to cfb.c (reader). Builds a small container holding one
 * or more named streams (e.g. "WordDocument", "Workbook", "PowerPoint
 * Document") and serializes it to a byte buffer that, when written to disk, is
 * a valid .doc/.xls/.ppt container.
 *
 * Design notes:
 *   - Fixed 512-byte sector size, 64-byte mini-sector size, 4096 cut-off.
 *   - Directory is a flat array (no red-black colour bits needed); entry 0 is
 *     the Root Entry, followed by one stream entry per added stream.
 *   - Streams >= cutoff use the FAT; smaller streams use the MiniFAT + the
 *     root entry's mini-stream.
 *   - DIFAT fits in the header's 109 slots (covers up to 109 FAT sectors ~
 *     55 MB), which is far beyond any legacy document; large-file DIFAT
 *     chaining is not needed here.
 *
 * Clean-room C11, no dependencies. Opaque handle. */

#ifndef WUBUCFB_CFB_WRITE_H
#define WUBUCFB_CFB_WRITE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wubucfb_writer wubucfb_writer;

/* Create an empty container writer. */
wubucfb_writer *wubucfb_writer_create(void);

/* Free a writer (safe on NULL). */
void wubucfb_writer_free(wubucfb_writer *w);

/* Add (or replace) a stream with UTF-16LE name `name` (ASCII input) and bytes
 * `data`/`len`. Returns 0 on success, -1 on error. The data is copied. */
int wubucfb_writer_add(wubucfb_writer *w, const char *name, const void *data, size_t len);

/* Serialize the container to a fresh malloc'd buffer (`*out`, caller frees)
 * and set *out_len. Returns 0 on success, -1 on error. */
int wubucfb_writer_finish(wubucfb_writer *w, uint8_t **out, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCFB_CFB_WRITE_H */
