/* wuos_file.c -- minimal file read/write helpers for the GUI shell. */
#include "wuos_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *wuos_read_file(const char *path, size_t *len){
    if (len) *len = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0){ fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0){ fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0){ fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf){ fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    if (len) *len = got;
    return buf;
}

int wuos_write_file(const char *path, const char *buf, size_t len){
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t wrote = fwrite(buf, 1, len, f);
    fclose(f);
    return (wrote == len) ? 0 : -1;
}
