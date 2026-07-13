#include "reader.h"
#include "io_le.h"
#include "inflate.h"

#include <stdlib.h>
#include <string.h>

struct wubuzip_entry {
    char *name;
    uint32_t method;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint32_t crc;
    uint32_t local_off;
};

int wubuzip_open(const uint8_t *data, size_t len, wubuzip_archive *z) {
    memset(z, 0, sizeof *z);
    z->data = data;
    z->len = len;

    size_t eocd = (size_t)-1;
    size_t start = len > 22 ? len - 22 : 0;
    for (size_t i = start; ; i--) {
        if (i + 22 <= len &&
            data[i] == 'P' && data[i+1] == 'K' && data[i+2] == 5 && data[i+3] == 6) {
            eocd = i;
            break;
        }
        if (i == 0) break;
    }
    if (eocd == (size_t)-1) return -1;

    uint32_t cd_off = wubuzip_rd32(data + eocd + 16);
    uint16_t n = wubuzip_rd16(data + eocd + 10);
    z->entries = calloc(n ? n : 1, sizeof *z->entries);
    if (!z->entries) return -1;
    z->n = n;

    size_t p = cd_off;
    for (uint16_t i = 0; i < n; i++) {
        if (p + 46 > len) { z->n = i; break; }
        if (!(data[p] == 'P' && data[p+1] == 'K' && data[p+2] == 1 && data[p+3] == 2))
            return -1;
        uint16_t nl = wubuzip_rd16(data + p + 28);
        uint16_t el = wubuzip_rd16(data + p + 30);
        uint16_t cl = wubuzip_rd16(data + p + 32);
        wubuzip_entry *e = &z->entries[i];
        e->method = wubuzip_rd16(data + p + 10);
        e->crc = wubuzip_rd32(data + p + 16);
        e->comp_size = wubuzip_rd32(data + p + 20);
        e->uncomp_size = wubuzip_rd32(data + p + 24);
        e->local_off = wubuzip_rd32(data + p + 42);
        e->name = malloc(nl + 1);
        if (!e->name) return -1;
        memcpy(e->name, data + p + 46, nl);
        e->name[nl] = '\0';
        p += 46u + nl + el + cl;
    }
    return 0;
}

size_t wubuzip_count(const wubuzip_archive *z) { return z->n; }
const char *wubuzip_name(const wubuzip_archive *z, size_t i) { return z->entries[i].name; }

size_t wubuzip_find(const wubuzip_archive *z, const char *name) {
    for (size_t i = 0; i < z->n; i++)
        if (strcmp(z->entries[i].name, name) == 0) return i;
    return (size_t)-1;
}

int wubuzip_extract(const wubuzip_archive *z, size_t i, uint8_t **out, size_t *out_len) {
    const wubuzip_entry *e = &z->entries[i];
    const uint8_t *lh = z->data + e->local_off;
    if (e->local_off + 30 > z->len) return -1;
    uint16_t nl = wubuzip_rd16(lh + 26), el = wubuzip_rd16(lh + 28);
    const uint8_t *comp = lh + 30 + nl + el;
    if ((size_t)(comp - z->data) + e->comp_size > z->len) return -1;

    if (e->method == 0) { /* store */
        uint8_t *buf = malloc(e->uncomp_size ? e->uncomp_size : 1);
        if (!buf) return -1;
        memcpy(buf, comp, e->uncomp_size);
        *out = buf;
        *out_len = e->uncomp_size;
        return 0;
    } else if (e->method == 8) { /* deflate */
        uint8_t *buf = NULL;
        size_t buflen = 0;
        if (wubuzip_inflate(comp, e->comp_size, &buf, &buflen, 0) != 0) return -1;
        *out = buf;
        *out_len = buflen;
        return 0;
    }
    return -1;
}

void wubuzip_close(wubuzip_archive *z) {
    for (size_t i = 0; i < z->n; i++) free(z->entries[i].name);
    free(z->entries);
    z->entries = NULL;
    z->n = 0;
}
