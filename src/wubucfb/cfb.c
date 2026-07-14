/* wubucfb -- MS-CFB / OLE2 compound-file container reader. See cfb.h.
 *
 * Layout recap (little-endian throughout):
 *   - 512-byte header: signature, sector/mini-sector shifts, FAT/DIFAT/dir/
 *     minifat chain starts, mini-stream cutoff, DIFAT[109].
 *   - FAT: sector-allocation chains (one entry per sector).
 *   - Directory: 128-byte entries in a red-black tree; entry 0 = Root Entry,
 *     whose start sector + size describe the mini-stream.
 *   - Streams >= cutoff (usually 4096) live in FAT sectors; smaller streams
 *     live in the mini-stream, addressed by the MiniFAT. */

#include "cfb.h"

#include <stdlib.h>
#include <string.h>

#define CFB_ENDOFCHAIN 0xFFFFFFFEu
#define CFB_FREESECT   0xFFFFFFFFu
#define CFB_FATSECT    0xFFFFFFFDu
#define CFB_DIFSECT    0xFFFFFFFCu

struct wubucfb {
    uint8_t *img;          /* full file image (owned copy) */
    size_t   img_len;
    uint32_t sector_size;  /* 1 << sector_shift */
    uint32_t mini_size;    /* 1 << mini_shift   */
    uint32_t mini_cutoff;  /* streams < this use the mini-stream */
    uint32_t *fat;         /* FAT entries (sector links) */
    size_t   fat_n;
    uint32_t *minifat;     /* MiniFAT entries */
    size_t   minifat_n;
    uint8_t *dir;          /* raw directory bytes */
    size_t   dir_len;
    uint8_t *mini_stream;  /* materialized mini-stream (root entry chain) */
    size_t   mini_stream_len;
};

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Byte offset of FAT sector `s` in the image (sector 0 starts right after the
 * 512-byte header regardless of sector size). */
static size_t sect_off(const wubucfb *c, uint32_t s) {
    return (size_t)512 + (size_t)s * c->sector_size;
}

/* Follow a FAT chain starting at `start`, concatenating each sector's bytes.
 * Caps total output at `limit` (0 = no cap beyond the image). Returns malloc'd
 * buffer (caller frees) and sets *out_len; NULL on error/empty. */
static uint8_t *read_fat_chain(const wubucfb *c, uint32_t start, size_t limit, size_t *out_len) {
    size_t cap = 0, cur = 0;
    uint8_t *buf = NULL;
    uint32_t s = start;
    size_t guard = 0;
    while (s != CFB_ENDOFCHAIN && s != CFB_FREESECT) {
        if (s >= c->fat_n) break;                 /* out-of-range link */
        if (++guard > c->fat_n + 1) break;        /* cycle guard */
        size_t off = sect_off(c, s);
        if (off + c->sector_size > c->img_len) break;
        if (cur + c->sector_size > cap) {
            size_t nc = cap ? cap * 2 : c->sector_size;
            while (cur + c->sector_size > nc) nc *= 2;
            uint8_t *nb = realloc(buf, nc);
            if (!nb) { free(buf); return NULL; }
            buf = nb; cap = nc;
        }
        memcpy(buf + cur, c->img + off, c->sector_size);
        cur += c->sector_size;
        s = c->fat[s];
    }
    if (limit && cur > limit) cur = limit;
    *out_len = cur;
    return buf;
}

/* Read `size` bytes of a mini-stream chain starting at mini-sector `start`. */
static uint8_t *read_mini_chain(const wubucfb *c, uint32_t start, size_t size, size_t *out_len) {
    uint8_t *buf = malloc(size ? size : 1);
    if (!buf) return NULL;
    size_t cur = 0;
    uint32_t s = start;
    size_t guard = 0;
    while (s != CFB_ENDOFCHAIN && s != CFB_FREESECT && cur < size) {
        if (s >= c->minifat_n) break;
        if (++guard > c->minifat_n + 1) break;
        size_t off = (size_t)s * c->mini_size;
        if (off + c->mini_size > c->mini_stream_len) break;
        size_t take = c->mini_size;
        if (cur + take > size) take = size - cur;
        memcpy(buf + cur, c->mini_stream + off, take);
        cur += take;
        s = c->minifat[s];
    }
    *out_len = cur;
    return buf;
}

/* Compare a UTF-16LE directory name (byte length incl. terminator) against an
 * ASCII query, case-insensitively. Returns 1 on match. */
static int name_eq(const uint8_t *name16, uint16_t name_bytes, const char *ascii) {
    if (name_bytes < 2) return 0;
    size_t chars = (size_t)(name_bytes / 2) - 1;   /* drop the NUL terminator */
    size_t qi = 0;
    for (size_t i = 0; i < chars; i++) {
        uint16_t ch = rd16(name16 + i * 2);
        char a = ascii[qi];
        if (a == '\0') return 0;
        char lc = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : (char)ch;
        char la = (a >= 'A' && a <= 'Z') ? (char)(a + 32) : a;
        if (ch > 0x7F || lc != la) return 0;
        qi++;
    }
    return ascii[qi] == '\0';
}

wubucfb *wubucfb_open(const uint8_t *data, size_t len) {
    static const uint8_t sig[8] = {0xD0,0xCF,0x11,0xE0,0xA1,0xB1,0x1A,0xE1};
    if (!data || len < 512 || memcmp(data, sig, 8) != 0) return NULL;

    wubucfb *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->img = malloc(len);
    if (!c->img) { free(c); return NULL; }
    memcpy(c->img, data, len);
    c->img_len = len;

    const uint8_t *h = c->img;
    uint16_t sector_shift = rd16(h + 30);
    uint16_t mini_shift   = rd16(h + 32);
    if (sector_shift < 7 || sector_shift > 20 || mini_shift < 2 || mini_shift > sector_shift) {
        wubucfb_close(c); return NULL;
    }
    c->sector_size = 1u << sector_shift;
    c->mini_size   = 1u << mini_shift;
    c->mini_cutoff = rd32(h + 56);
    if (c->mini_cutoff == 0) c->mini_cutoff = 4096;

    uint32_t num_fat_sectors   = rd32(h + 44);
    uint32_t first_dir_sector  = rd32(h + 48);
    uint32_t first_minifat     = rd32(h + 60);
    uint32_t num_minifat       = rd32(h + 64);
    uint32_t first_difat       = rd32(h + 68);
    uint32_t num_difat         = rd32(h + 72);

    /* --- assemble the DIFAT: list of FAT sector numbers --- */
    size_t difat_cap = num_fat_sectors + 16;
    uint32_t *difat = malloc(difat_cap * sizeof(uint32_t));
    if (!difat) { wubucfb_close(c); return NULL; }
    size_t difat_n = 0;
    for (int i = 0; i < 109; i++) {
        uint32_t v = rd32(h + 76 + i * 4);
        if (v == CFB_FREESECT || v == CFB_ENDOFCHAIN) continue;
        if (difat_n < difat_cap) difat[difat_n++] = v;
    }
    /* extra DIFAT sectors (rare, large files) */
    {
        uint32_t s = first_difat;
        uint32_t guard = 0;
        uint32_t per = c->sector_size / 4;
        while (s != CFB_ENDOFCHAIN && s != CFB_FREESECT && guard < num_difat + 4) {
            size_t off = sect_off(c, s);
            if (off + c->sector_size > c->img_len) break;
            for (uint32_t i = 0; i + 1 < per; i++) {
                uint32_t v = rd32(c->img + off + i * 4);
                if (v == CFB_FREESECT || v == CFB_ENDOFCHAIN) continue;
                if (difat_n >= difat_cap) {
                    difat_cap *= 2;
                    uint32_t *nd = realloc(difat, difat_cap * sizeof(uint32_t));
                    if (!nd) { free(difat); wubucfb_close(c); return NULL; }
                    difat = nd;
                }
                difat[difat_n++] = v;
            }
            s = rd32(c->img + off + (per - 1) * 4);  /* next DIFAT sector */
            guard++;
        }
    }

    /* --- build the FAT by reading each FAT sector --- */
    uint32_t per_fat = c->sector_size / 4;
    c->fat_n = difat_n * per_fat;
    c->fat = malloc((c->fat_n ? c->fat_n : 1) * sizeof(uint32_t));
    if (!c->fat) { free(difat); wubucfb_close(c); return NULL; }
    for (size_t i = 0; i < difat_n; i++) {
        size_t off = sect_off(c, difat[i]);
        for (uint32_t j = 0; j < per_fat; j++) {
            c->fat[i * per_fat + j] =
                (off + (size_t)j * 4 + 4 <= c->img_len) ? rd32(c->img + off + j * 4) : CFB_FREESECT;
        }
    }
    free(difat);

    /* --- directory chain --- */
    c->dir = read_fat_chain(c, first_dir_sector, 0, &c->dir_len);
    if (!c->dir || c->dir_len < 128) { wubucfb_close(c); return NULL; }

    /* --- MiniFAT --- */
    if (num_minifat && first_minifat != CFB_ENDOFCHAIN) {
        size_t mf_len = 0;
        uint8_t *mf = read_fat_chain(c, first_minifat, 0, &mf_len);
        if (mf) {
            c->minifat_n = mf_len / 4;
            c->minifat = malloc((c->minifat_n ? c->minifat_n : 1) * sizeof(uint32_t));
            if (c->minifat)
                for (size_t i = 0; i < c->minifat_n; i++) c->minifat[i] = rd32(mf + i * 4);
            free(mf);
        }
    }

    /* --- mini-stream: root entry (dir entry 0) start sector + size --- */
    {
        const uint8_t *root = c->dir;                 /* entry 0 */
        uint32_t start = rd32(root + 116);
        uint64_t size  = (uint64_t)rd32(root + 120) | ((uint64_t)rd32(root + 124) << 32);
        if (start != CFB_ENDOFCHAIN && size > 0) {
            size_t ml = 0;
            c->mini_stream = read_fat_chain(c, start, (size_t)size, &ml);
            c->mini_stream_len = ml;
        }
    }

    return c;
}

void wubucfb_close(wubucfb *c) {
    if (!c) return;
    free(c->img);
    free(c->fat);
    free(c->minifat);
    free(c->dir);
    free(c->mini_stream);
    free(c);
}

/* Locate directory entry index for `name`; returns -1 if absent. Walks the flat
 * entry array (the RB-tree ordering is irrelevant for a name lookup). */
static long find_dir_entry(wubucfb *c, const char *name) {
    size_t nentries = c->dir_len / 128;
    for (size_t i = 0; i < nentries; i++) {
        const uint8_t *e = c->dir + i * 128;
        uint16_t name_bytes = rd16(e + 64);
        uint8_t type = e[66];             /* 0=empty 1=storage 2=stream 5=root */
        if (type != 2) continue;          /* streams only */
        if (name_eq(e, name_bytes, name)) return (long)i;
    }
    return -1;
}

int wubucfb_has_stream(wubucfb *c, const char *name) {
    return c && find_dir_entry(c, name) >= 0;
}

int wubucfb_read_stream(wubucfb *c, const char *name, uint8_t **out, size_t *out_len) {
    if (!c || !out || !out_len) return -1;
    long idx = find_dir_entry(c, name);
    if (idx < 0) return -1;
    const uint8_t *e = c->dir + (size_t)idx * 128;
    uint32_t start = rd32(e + 116);
    uint64_t size  = (uint64_t)rd32(e + 120) | ((uint64_t)rd32(e + 124) << 32);
    size_t sz = (size_t)size;

    uint8_t *buf;
    size_t got = 0;
    if (size < c->mini_cutoff) {
        if (!c->mini_stream) return -1;
        buf = read_mini_chain(c, start, sz, &got);
    } else {
        buf = read_fat_chain(c, start, sz, &got);
    }
    if (!buf) return -1;
    /* pad/truncate to the declared size for predictable decoding */
    if (got < sz) {
        uint8_t *nb = realloc(buf, sz ? sz : 1);
        if (!nb) { free(buf); return -1; }
        memset(nb + got, 0, sz - got);
        buf = nb;
    }
    *out = buf;
    *out_len = sz;
    return 0;
}
