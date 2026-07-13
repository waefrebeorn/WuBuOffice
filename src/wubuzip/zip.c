#include "zip.h"

#include <stdlib.h>
#include <string.h>

struct wubuzip_writer {
    FILE *out;
    uint16_t count;
    struct cd {
        char *name;
        uint32_t crc;
        uint32_t size;
        uint32_t off;
    } *cds;
    size_t cap;
};

/* ---- CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320) ---- */
static uint32_t crc_table[256];
static int crc_ready = 0;

static void build_crc(void) {
    for (uint32_t i = 0; i < 256u; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
    crc_ready = 1;
}

uint32_t wubuzip_crc32(const void *data, size_t len) {
    if (!crc_ready) build_crc();
    const unsigned char *p = (const unsigned char *)data;
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        c = crc_table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

wubuzip_writer *wubuzip_create(FILE *out) {
    wubuzip_writer *z = calloc(1, sizeof *z);
    if (!z) return NULL;
    z->out = out;
    return z;
}

static void putle16(uint8_t *b, uint16_t v) { b[0] = (uint8_t)(v & 0xFF); b[1] = (uint8_t)((v >> 8) & 0xFF); }
static void putle32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)((v >> 8) & 0xFF);
    b[2] = (uint8_t)((v >> 16) & 0xFF);
    b[3] = (uint8_t)((v >> 24) & 0xFF);
}

int wubuzip_add(wubuzip_writer *z, const char *name, const void *data, uint32_t size) {
    const char *n = (name[0] == '/') ? name + 1 : name;
    uint32_t crc = wubuzip_crc32(data, size);
    long off = ftell(z->out);
    if (off < 0) return -1;

    uint8_t h[30];
    putle32(h + 0, 0x04034b50u); /* local file header signature */
    putle16(h + 4, 20);          /* version needed to extract */
    putle16(h + 6, 0);           /* general purpose flags */
    putle16(h + 8, 0);           /* method 0 = store */
    putle16(h + 10, 0);          /* last mod time */
    putle16(h + 12, 0);          /* last mod date */
    putle32(h + 14, crc);
    putle32(h + 18, size);       /* compressed size */
    putle32(h + 22, size);       /* uncompressed size */
    uint16_t nl = (uint16_t)strlen(n);
    putle16(h + 26, nl);         /* file name length */
    putle16(h + 28, 0);          /* extra field length */
    if (fwrite(h, 1, 30, z->out) != 30) return -1;
    if (fwrite(n, 1, nl, z->out) != nl) return -1;
    if (size && fwrite(data, 1, size, z->out) != size) return -1;

    if (z->count == z->cap) {
        z->cap = z->cap ? z->cap * 2 : 16;
        z->cds = realloc(z->cds, z->cap * sizeof(*z->cds));
        if (!z->cds) return -1;
    }
    z->cds[z->count].name = strdup(n);
    z->cds[z->count].crc = crc;
    z->cds[z->count].size = size;
    z->cds[z->count].off = (uint32_t)off;
    z->count++;
    return 0;
}

int wubuzip_finalize(wubuzip_writer *z) {
    long cd_off = ftell(z->out);
    if (cd_off < 0) return -1;

    uint32_t cd_size = 0;
    for (uint16_t i = 0; i < z->count; i++) {
        struct cd *e = &z->cds[i];
        uint16_t nl = (uint16_t)strlen(e->name);
        uint8_t h[46];
        putle32(h + 0, 0x02014b50u); /* central directory header signature */
        putle16(h + 4, 20);          /* version made by */
        putle16(h + 6, 20);          /* version needed */
        putle16(h + 8, 0);           /* flags */
        putle16(h + 10, 0);          /* method */
        putle16(h + 12, 0);          /* time */
        putle16(h + 14, 0);          /* date */
        putle32(h + 16, e->crc);
        putle32(h + 20, e->size);    /* compressed size */
        putle32(h + 24, e->size);    /* uncompressed size */
        putle16(h + 28, nl);         /* name length */
        putle16(h + 30, 0);          /* extra length */
        putle16(h + 32, 0);          /* comment length */
        putle16(h + 34, 0);          /* disk number start */
        putle16(h + 36, 0);          /* internal attrs */
        putle32(h + 38, 0);          /* external attrs */
        putle32(h + 42, e->off);     /* local header offset */
        if (fwrite(h, 1, 46, z->out) != 46) return -1;
        if (fwrite(e->name, 1, nl, z->out) != nl) return -1;
        cd_size += 46u + nl;
    }

    uint8_t e[22];
    putle32(e + 0, 0x06054b50u); /* end of central directory signature */
    putle16(e + 4, 0);           /* number of this disk */
    putle16(e + 6, 0);           /* disk with start of cd */
    putle16(e + 8, z->count);    /* entries on this disk */
    putle16(e + 10, z->count);   /* total entries */
    putle32(e + 12, cd_size);
    putle32(e + 16, (uint32_t)cd_off);
    putle16(e + 20, 0);          /* comment length */
    if (fwrite(e, 1, 22, z->out) != 22) return -1;

    if (fflush(z->out) != 0) return -1;
    for (uint16_t i = 0; i < z->count; i++) free(z->cds[i].name);
    free(z->cds);
    free(z);
    return 0;
}
