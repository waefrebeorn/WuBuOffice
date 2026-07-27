/* wuos_file.h -- minimal file read/write helpers for the GUI shell. */
#ifndef WUOS_FILE_H
#define WUOS_FILE_H
#include <stddef.h>

/* Read entire file into a malloc'd NUL-terminated buffer (caller frees).
 * Returns NULL on failure (and *len is 0). */
char *wuos_read_file(const char *path, size_t *len);

/* Write buffer (len bytes) to path. Returns 0 ok, -1 on failure. */
int   wuos_write_file(const char *path, const char *buf, size_t len);

#endif /* WUOS_FILE_H */
