#include "wubufont_internal.h"

Font *font_open(const uint8_t *data, size_t size) {
    Font *f = font_open_owned(data, size, 0);
    return f;
}

/* Open from a blob. If take_ownership, we copy the blob so the returned Font
 * is fully self-contained (used by woff_open which reconstructs an sfnt). */
Font *font_open_owned(const uint8_t *data, size_t size, int take_ownership) {
    if (!data || size < 12) return NULL;
    uint32_t sig = rd32(data);
    if (sig != TAG('t','r','u','e') && sig != TAG('O','T','T','O') && sig != 0x00010000u)
        return NULL;
    Font *f = xrealloc(NULL, sizeof *f);
    memset(f, 0, sizeof *f);
    if (take_ownership) {
        uint8_t *copy = xrealloc(NULL, size);
        memcpy(copy, data, size);
        f->owned = copy;
        f->data = copy;
    } else {
        f->data = data;
    }
    /* Parse from the buffer we actually retain (the copy when we own it), so
     * cached pointers (loca/glyf) never dangle after the caller frees `data`. */
    const uint8_t *base = f->data;
    f->size = size;
    f->n_tables = rd16(base + 4);
    /* robustness (H5): a decoded-but-corrupt embedded font must be rejected,
     * never read past the blob. Directory must fit: 12 + n*16 bytes. */
    if ((size_t)f->n_tables > (size - 12) / 16){
        free(f);
        return NULL;
    }
    f->tags = xrealloc(NULL, f->n_tables * sizeof *f->tags);
    f->off  = xrealloc(NULL, f->n_tables * sizeof *f->off);
    f->len  = xrealloc(NULL, f->n_tables * sizeof *f->len);
    /* directory entries start at offset 12, 16 bytes each */
    for (uint16_t i = 0; i < f->n_tables; i++) {
        const uint8_t *e = base + 12 + (size_t)i * 16;
        f->tags[i] = rd32(e);
        f->off[i]  = rd32(e + 8);
        f->len[i]  = rd32(e + 12);
    }
    /* head */
    size_t o, l;
    if (font_find_table(f, TAG('h','e','a','d'), &o, &l) && l >= 54) {
        f->have_head = 1;
        f->units_per_em = rd16(base + o + 18);
    }
    /* maxp */
    if (font_find_table(f, TAG('m','a','x','p'), &o, &l) && l >= 6) {
        f->have_maxp = 1;
        f->glyph_count = rd16(base + o + 4);
    }
    /* hhea */
    if (font_find_table(f, TAG('h','h','e','a'), &o, &l) && l >= 36) {
        f->have_hhea = 1;
        f->ascent  = rd16s(base + o + 4);
        f->descent = rd16s(base + o + 6);
    }
    /* loca + glyf */
    size_t lo, ll, go, gl;
    if (font_find_table(f, TAG('l','o','c','a'), &lo, &ll) &&
        font_find_table(f, TAG('g','l','y','f'), &go, &gl)) {
        f->loca = base + lo;
        f->glyf = base + go;
    }
    /* loca format (short/long) lives in head[50]; read it now that head is known */
    if (f->have_head) {
        size_t ho; size_t hl;
        if (font_find_table(f, TAG('h','e','a','d'), &ho, &hl) && hl >= 52)
            f->loca_is_long = (rd16s(base + ho + 50) != 0);
    }
    return f;
}

void font_free(Font *f) {
    if (!f) return;
    free(f->tags); free(f->off); free(f->len);
    free(f->owned);
    free(f);
}

size_t font_table_count(const Font *f) { return f ? f->n_tables : 0; }
uint32_t font_table_tag(const Font *f, size_t idx) { return f && idx < f->n_tables ? f->tags[idx] : 0; }
int font_table_range(const Font *f, size_t idx, size_t *offset, size_t *length) {
    if (!f || idx >= f->n_tables) return 0;
    if (offset) *offset = f->off[idx];
    if (length) *length = f->len[idx];
    return 1;
}
int font_find_table(const Font *f, uint32_t tag, size_t *offset, size_t *length) {
    if (!f) return 0;
    for (uint16_t i = 0; i < f->n_tables; i++)
        if (f->tags[i] == tag) {
            if (offset) *offset = f->off[i];
            if (length) *length = f->len[i];
            /* sanity: table must fit in blob */
            if (f->off[i] + f->len[i] > f->size) return 0;
            return 1;
        }
    return 0;
}
uint16_t font_units_per_em(const Font *f) { return f && f->have_head ? f->units_per_em : 0; }
uint16_t font_glyph_count(const Font *f) { return f && f->have_maxp ? f->glyph_count : 0; }
int16_t  font_ascent(const Font *f) { return f && f->have_hhea ? f->ascent : 0; }
int16_t  font_descent(const Font *f) { return f && f->have_hhea ? f->descent : 0; }
/* ---- glyf outline -> SVG path 'd' ---- */
static size_t glyph_offset(const Font *f, uint16_t gi) {
    if (!f->loca) return 0;
    if (f->loca_is_long) return rd32(f->loca + (size_t)gi * 4);
    return (size_t)rd16(f->loca + (size_t)gi * 2) * 2;
}
static size_t glyph_next(const Font *f, uint16_t gi) {
    if (f->loca_is_long) return rd32(f->loca + (size_t)(gi + 1) * 4);
    return (size_t)rd16(f->loca + (size_t)(gi + 1) * 2) * 2;
}
