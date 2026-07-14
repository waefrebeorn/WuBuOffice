/* cfb_write.c -- MS-CFB container writer. See cfb_write.h.
 * Clean-room C11. Fixed 512-byte sectors, 64-byte mini-sectors, 4096 cut-off.
 *
 * Sector allocation order (all main-FAT sectors, contiguous numbering):
 *   [0 .. mini_stream_sectors)         mini-stream (root entry's stream)
 *   [.. + minifat_sectors)             MiniFAT stream
 *   [.. + large-stream sectors)        each FAT stream, padded to a sector
 *   [.. + dir_sectors)                 directory
 *   [.. + fat_sector_count)            main FAT (sectors marked FATSECT)
 * The main FAT is written last, after its own sector range is reserved. */

#include "cfb_write.h"

#include <stdlib.h>
#include <string.h>

#define SEC_SHIFT   9
#define SEC_SIZE    (1u << SEC_SHIFT)
#define MINI_SHIFT  6
#define MINI_SIZE   (1u << MINI_SHIFT)
#define CUTOFF      4096

#define ENDOFC  0xFFFFFFFEu
#define FREE    0xFFFFFFFFu
#define FATSECT 0xFFFFFFFDu
#define DIFSECT 0xFFFFFFFCu

typedef struct {
    char   *name;
    uint8_t *data;
    size_t  len;
    int     in_mini;
} cfstream;

struct wubucfb_writer {
    cfstream *s;
    size_t n, cap;
};

wubucfb_writer *wubucfb_writer_create(void) { return calloc(1, sizeof(wubucfb_writer)); }

void wubucfb_writer_free(wubucfb_writer *w) {
    if (!w) return;
    for (size_t i = 0; i < w->n; i++) { free(w->s[i].name); free(w->s[i].data); }
    free(w->s);
    free(w);
}

int wubucfb_writer_add(wubucfb_writer *w, const char *name, const void *data, size_t len) {
    if (!w || !name) return -1;
    size_t idx = w->n;
    for (size_t i = 0; i < w->n; i++) if (strcmp(w->s[i].name, name) == 0) { idx = i; free(w->s[i].data); w->s[i].data = NULL; w->s[i].len = 0; break; }
    if (idx == w->n) {
        if (w->n + 1 > w->cap) { size_t nc = w->cap ? w->cap * 2 : 4; cfstream *ns = realloc(w->s, nc * sizeof *ns); if (!ns) return -1; w->s = ns; w->cap = nc; }
        w->s[idx].name = strdup(name); if (!w->s[idx].name) return -1;
        w->n++;
    }
    w->s[idx].data = malloc(len ? len : 1);
    if (!w->s[idx].data) return -1;
    if (len) memcpy(w->s[idx].data, data, len);
    w->s[idx].len = len;
    w->s[idx].in_mini = (len < CUTOFF) ? 1 : 0;
    return 0;
}

static void wr16(uint8_t *p, uint16_t v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; }
static void wr32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)((v >> (8*i)) & 0xff); }

static void dir_entry(uint8_t *e, const char *name, uint32_t start, uint64_t size, uint8_t type,
                  int is_mini, uint32_t child, uint32_t left, uint32_t right) {
    memset(e, 0, 128);
    size_t L = strlen(name); if (L > 31) L = 31;
    for (size_t i = 0; i < L; i++) { e[i*2] = (uint8_t)name[i]; e[i*2+1] = 0; }
    wr16(e + 64, (uint16_t)((L + 1) * 2));
    e[66] = type;
    e[67] = 1;                 /* node colour (unused) */
    wr32(e + 68, left);        /* left sibling */
    wr32(e + 72, right);       /* right sibling */
    wr32(e + 76, child);       /* child */
    wr32(e + 116, start);
    wr32(e + 120, (uint32_t)(size & 0xffffffffu));
    wr32(e + 124, (uint32_t)(size >> 32));
    if (is_mini) e[100] = 1;   /* 0x60 flag bit: stream in mini-stream */
}

int wubucfb_writer_finish(wubucfb_writer *w, uint8_t **out, size_t *out_len) {
    if (!w || !out || !out_len) return -1;

    /* mini-stream layout: append each mini stream, padding each to a whole
       mini-sector, and track the running padded length. */
    size_t mini_cur = 0;
    for (size_t i = 0; i < w->n; i++) {
        if (!w->s[i].in_mini) continue;
        mini_cur += w->s[i].len;
        size_t np = mini_cur; if (np % MINI_SIZE) np += MINI_SIZE - (np % MINI_SIZE);
        mini_cur = np;
    }
    size_t mini_pad = mini_cur;          /* running padded length == full mini-stream size */
    uint8_t *ministream = calloc(mini_pad ? mini_pad : 1, 1);
    if (!ministream) return -1;
    mini_cur = 0;
    uint32_t *mini_start = calloc(w->n ? w->n : 1, sizeof(uint32_t));
    if (!mini_start) { free(ministream); return -1; }
    for (size_t i = 0; i < w->n; i++) {
        if (!w->s[i].in_mini) { mini_start[i] = ENDOFC; continue; }
        mini_start[i] = (uint32_t)(mini_cur / MINI_SIZE);
        memcpy(ministream + mini_cur, w->s[i].data, w->s[i].len);
        mini_cur += w->s[i].len;
        size_t np = mini_cur; if (np % MINI_SIZE) np += MINI_SIZE - (np % MINI_SIZE); mini_cur = np;
    }
    size_t mini_stream_sectors = mini_cur / MINI_SIZE;   /* count of 64-byte mini-sectors */
    size_t mini_fat_sec = (mini_pad + SEC_SIZE - 1) / SEC_SIZE; /* 512-byte FAT footprint of the mini-stream body */
    uint8_t *minifat = NULL; size_t minifat_sectors = 0;
    if (mini_stream_sectors) {
        minifat_sectors = (mini_stream_sectors * 4 + SEC_SIZE - 1) / SEC_SIZE;
        minifat = calloc(minifat_sectors * SEC_SIZE, 1);
        if (!minifat) { free(ministream); free(mini_start); return -1; }
    }

    /* large streams: padded to whole sectors */
    uint32_t *fat_start = calloc(w->n ? w->n : 1, sizeof(uint32_t));
    uint32_t *fat_sects = calloc(w->n ? w->n : 1, sizeof(uint32_t));
    if (!fat_start || !fat_sects) { free(ministream); free(mini_start); free(minifat); free(fat_start); free(fat_sects); return -1; }
    size_t fat_bytes = 0;
    for (size_t i = 0; i < w->n; i++) {
        if (w->s[i].in_mini) { fat_start[i] = ENDOFC; fat_sects[i] = 0; continue; }
        size_t padded = w->s[i].len ? w->s[i].len : SEC_SIZE;
        if (padded % SEC_SIZE) padded += SEC_SIZE - (padded % SEC_SIZE);
        fat_sects[i] = (uint32_t)(padded / SEC_SIZE);
        fat_start[i] = (uint32_t)(fat_bytes / SEC_SIZE);
        fat_bytes += padded;
    }

    /* directory */
    size_t ndir = w->n + 1;
    size_t dir_bytes = ndir * 128;
    if (dir_bytes % SEC_SIZE) dir_bytes += SEC_SIZE - (dir_bytes % SEC_SIZE);
    uint8_t *dir = calloc(dir_bytes, 1);
    if (!dir) { free(ministream); free(mini_start); free(minifat); free(fat_start); free(fat_sects); return -1; }
    uint32_t mini_stream_start = mini_stream_sectors ? 0 : ENDOFC;
    dir_entry(dir, "Root Entry", mini_stream_start, mini_pad, 5, 0,
              (uint32_t)(w->n ? 1 : ENDOFC), ENDOFC, ENDOFC);
    for (size_t i = 0; i < w->n; i++) {
        uint32_t st = w->s[i].in_mini ? mini_start[i]
                                      : (uint32_t)(mini_fat_sec + minifat_sectors + fat_start[i]);
        uint32_t right = (i + 1 < w->n) ? (uint32_t)(i + 2) : ENDOFC;
        dir_entry(dir + (i + 1) * 128, w->s[i].name, st, w->s[i].len, 2, w->s[i].in_mini,
                  ENDOFC, ENDOFC, right);
    }

    /* total managed (data) sectors */
    size_t managed = mini_fat_sec + minifat_sectors + (fat_bytes / SEC_SIZE) + (dir_bytes / SEC_SIZE);
    size_t fat_sector_count = (managed + SEC_SIZE/4 - 1) / (SEC_SIZE/4);
    if (fat_sector_count > 109) { /* beyond simple header DIFAT */ free(dir); free(ministream); free(mini_start); free(minifat); free(fat_start); free(fat_sects); return -1; }

    size_t total_sectors = managed + fat_sector_count;
    size_t image_size = (size_t)SEC_SIZE + total_sectors * (size_t)SEC_SIZE;
    uint8_t *img = calloc(image_size, 1);
    if (!img) { free(dir); free(ministream); free(mini_start); free(minifat); free(fat_start); free(fat_sects); return -1; }

    #define SECT_OFF(s) ((size_t)SEC_SIZE + (size_t)(s) * SEC_SIZE)

    /* write bodies */
    if (mini_fat_sec) memcpy(img + SECT_OFF(0), ministream, mini_pad);
    for (size_t i = 0; i < w->n; i++) {
        if (w->s[i].in_mini) continue;
        uint32_t base = (uint32_t)(mini_fat_sec + minifat_sectors + fat_start[i]);
        memcpy(img + SECT_OFF(base), w->s[i].data, w->s[i].len);
    }
    uint32_t dir_start = (uint32_t)(mini_fat_sec + minifat_sectors + (fat_bytes / SEC_SIZE));
    memcpy(img + SECT_OFF(dir_start), dir, dir_bytes);

    /* main FAT */
    uint32_t *fat = calloc(fat_sector_count * (SEC_SIZE/4), sizeof(uint32_t));
    if (!fat) { free(img); free(dir); free(ministream); free(mini_start); free(minifat); free(fat_start); free(fat_sects); return -1; }
    /* the mini-stream body occupies FAT sectors [0 .. mini_fat_sec-1] */
    for (size_t i = 0; i + 1 < mini_fat_sec; i++) fat[i] = (uint32_t)(i + 1);
    if (mini_fat_sec) fat[mini_fat_sec - 1] = ENDOFC;
    uint32_t mf_base = (uint32_t)mini_fat_sec;
    for (size_t i = 0; i + 1 < minifat_sectors; i++) fat[mf_base + i] = mf_base + (uint32_t)(i + 1);
    if (minifat_sectors) fat[mf_base + minifat_sectors - 1] = ENDOFC;
    /* MiniFAT contents: for every mini stream, chain its mini-sectors in the
       mini-stream (minifat[k] -> next mini-sector of that stream, ENDOFC last). */
    if (minifat) {
        for (size_t i = 0; i < w->n; i++) {
            if (!w->s[i].in_mini || w->s[i].len == 0) continue;
            size_t nsec = (w->s[i].len + MINI_SIZE - 1) / MINI_SIZE;
            uint32_t base = mini_start[i];
            for (size_t k = 0; k + 1 < nsec; k++)
                wr32(minifat + (base + k) * 4, base + (uint32_t)(k + 1));
            wr32(minifat + (base + nsec - 1) * 4, ENDOFC);
        }
    }
    /* copy the now-populated MiniFAT into the image */
    if (minifat_sectors) memcpy(img + SECT_OFF(mini_fat_sec), minifat, minifat_sectors * SEC_SIZE);
    for (size_t i = 0; i < w->n; i++) {
        if (w->s[i].in_mini) continue;
        uint32_t base = (uint32_t)(mini_fat_sec + minifat_sectors + fat_start[i]);
        for (uint32_t k = 0; k + 1 < fat_sects[i]; k++) fat[base + k] = base + k + 1;
        if (fat_sects[i]) fat[base + fat_sects[i] - 1] = ENDOFC;
    }
    uint32_t dir_end = dir_start + (uint32_t)(dir_bytes / SEC_SIZE);
    for (uint32_t k = dir_start; k + 1 < dir_end; k++) fat[k] = k + 1;
    if (dir_end > dir_start) fat[dir_end - 1] = ENDOFC;
    uint32_t fat_base = (uint32_t)managed;
    for (uint32_t k = 0; k < fat_sector_count; k++) fat[fat_base + k] = FATSECT;

    /* write FAT sectors */
    for (uint32_t k = 0; k < fat_sector_count; k++) {
        uint32_t *src = fat + k * (SEC_SIZE/4);
        uint8_t *dst = img + SECT_OFF(fat_base + k);
        for (uint32_t j = 0; j < SEC_SIZE/4; j++) wr32(dst + j*4, src[j]);
    }

    /* header */
    static const uint8_t sig[8] = {0xD0,0xCF,0x11,0xE0,0xA1,0xB1,0x1A,0xE1};
    memcpy(img, sig, 8);
    memset(img + 8, 0, 16);
    wr16(img + 24, 0x3B);
    wr16(img + 26, 3);
    wr16(img + 28, 0xFFFE);
    wr16(img + 30, SEC_SHIFT);
    wr16(img + 32, MINI_SHIFT);
    wr16(img + 34, 0);
    wr32(img + 36, 0);
    wr32(img + 40, 0);
    wr32(img + 44, (uint32_t)fat_sector_count);
    wr32(img + 48, dir_start);
    wr32(img + 52, 0);
    wr32(img + 56, CUTOFF);
    wr32(img + 60, minifat_sectors ? (uint32_t)mf_base : ENDOFC);
    wr32(img + 64, (uint32_t)minifat_sectors);
    wr32(img + 68, ENDOFC);
    wr32(img + 72, 0);
    for (int i = 0; i < 109; i++) wr32(img + 76 + i*4, (i < (int)fat_sector_count) ? (fat_base + (uint32_t)i) : FREE);

    free(fat); free(dir); free(ministream); free(mini_start); free(minifat); free(fat_start); free(fat_sects);
    *out = img; *out_len = image_size;
    return 0;
}
