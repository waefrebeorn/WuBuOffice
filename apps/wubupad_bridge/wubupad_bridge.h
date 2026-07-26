/* wubupad_bridge.h -- public API for the WuBuPad editor-core bridge.
 * See wubupad_bridge.c. Clean C11. */
#ifndef WUBUOFFICE_WUBUPAD_BRIDGE_H
#define WUBUOFFICE_WUBUPAD_BRIDGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Load a file's text via WuBuPad's Doc model. *out is malloc'd (caller frees).
 * Returns 0 on success, -1 on I/O error. */
int wubupad_load(const char *path, char **out, size_t *out_len);

/* Line / character counts via WuBuPad's Doc. Returns 0 on success, -1 on err. */
int wubupad_stats(const char *path, size_t *lines, size_t *chars);

/* Run a find/replace-all over a file using WuBuPad's DONE search engine +
 * find controller. *out is malloc'd (caller frees). Returns:
 *   0  = replaced at least one match
 *   1  = no matches (original text returned unchanged)
 *  -1  = I/O error */
int wubupad_find_replace(const char *path,
                         const char *find, int regex, int icase,
                         const char *repl,
                         char **out, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOFFICE_WUBUPAD_BRIDGE_H */
