#ifndef WUBUBASE_H
#define WUBUBASE_H
/* wububase -- shared, dependency-free internal utilities for WuBuOffice.
 *
 * Collects the helpers that were copy-pasted across modules (utf8 decode,
 * dynamic string buffer, xml escaping) so there is ONE tested copy. Modules
 * that previously rolled their own (wubuchart, wubudraw, wubumath, wubuepub,
 * wubuwordview, ...) should #include this instead of re-declaring them -- that
 * is how we stop re-fixing the same silent bug (e.g. byte-wise UTF-8) in six
 * places.
 *
 * C11, no third-party deps. 0 warnings under -Wall -Wextra -Wpedantic.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- UTF-8 ---------------- */
/* Decode one codepoint from *s. Returns bytes consumed (>0) and writes *cp.
 * Returns 0 on end-of-string, <0 on an invalid lead byte (cp set to the raw
 * byte so callers can render a replacement). NEVER consumes a partial
 * sequence -- that is what prevents feeding FreeType raw bytes (mojibake). */
int  wububase_utf8_decode(const char *s, uint32_t *cp);
/* Encode codepoint cp into out (must be >= 4 bytes). Returns bytes written. */
int  wububase_utf8_encode(uint32_t cp, char *out);
/* Number of codepoints in a valid UTF-8 string (not byte length). */
int  wububase_utf8_len(const char *s);

/* ---------------- dynamic string buffer ---------------- */
typedef struct {
    char  *p;     /* NUL-terminated on demand via wububase_buf_str() */
    size_t len;   /* bytes used, excluding NUL */
    size_t cap;   /* allocated capacity */
} Buf;

void wububase_buf_init(Buf *b);
void wububase_buf_free(Buf *b);
/* Append raw string t. Returns 0 ok, -1 on alloc failure (buffer unchanged). */
int  wububase_buf_add(Buf *b, const char *t);
/* Append printf-formatted text (va_list / stdarg). */
int  wububase_buf_vprintf(Buf *b, const char *fmt, va_list ap);
int  wububase_buf_printf(Buf *b, const char *fmt, ...);
/* Pointer to the NUL-terminated content (always valid; lazily ensures NUL). */
const char *wububase_buf_str(Buf *b);
/* Current length in bytes. */
size_t wububase_buf_len(const Buf *b);

/* ---------------- XML / HTML escaping ---------------- */
/* Append t to b with & < > " escaped (XML/HTML text + attribute safe). */
int  wububase_xml_escape(Buf *b, const char *t);

#ifdef __cplusplus
}
#endif
#endif /* WUBUBASE_H */
