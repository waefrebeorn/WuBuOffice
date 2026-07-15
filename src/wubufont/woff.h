/* woff.h -- clean-room WOFF 1.0 (Web Open Font Format) reader + writer.
 *
 * WOFF 1.0 wraps an sfnt by zlib-compressing each table (W3C WOFF1; the
 * compression is RFC 1950 zlib = 2-byte header + raw DEFLATE + Adler-32).
 * We reuse wubuzip's DEFLATE codec and the Font sfnt parser so a WOFF file
 * is transparently a compressed font.
 *
 * Reference: W3C WOFF 1.0 File Format (https://www.w3.org/TR/WOFF/),
 *           which itself references ISO/IEC 14496-22 (the sfnt inside).
 */
#ifndef WUBUFONT_WOFF_H
#define WUBUFONT_WOFF_H

#include <stddef.h>
#include <stdint.h>

typedef struct Font Font;

/* Open a WOFF 1.0 blob. Decompresses every table, reconstructs the original
 * sfnt in memory, and returns a Font* (same object font_open would return for
 * the uncompressed font). Returns NULL if not a valid WOFF or decompression
 * fails. The WOFF blob must outlive the reconstructed sfnt? No — we copy the
 * reconstructed sfnt, so only the returned Font* (and its own buffer) lives. */
Font *woff_open(const uint8_t *data, size_t size);

/* Compress an sfnt (as opened by font_open / read from disk) into a WOFF 1.0
 * blob. Returns a malloc'd WOFF buffer (caller frees) and sets *out_len.
 * Used both as a producer and as the round-trip oracle for woff_open. */
uint8_t *sfnt_to_woff(const uint8_t *sfnt, size_t sfnt_len, size_t *out_len);

/* Adler-32 (zlib). Exposed for the trailer check / round-trip assertion. */
uint32_t woff_adler32(const uint8_t *data, size_t len);

#endif /* WUBUFONT_WOFF_H */
