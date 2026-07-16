#include "wubufont_internal.h"

/* ---- cmap -> glyph index (format 4, with format 0/6 fallback) ---- */
uint16_t font_cmap(const Font *f, uint32_t cp) {
    size_t o, l;
    if (!font_find_table(f, TAG('c','m','a','p'), &o, &l) || l < 4) return 0;
    const uint8_t *p = f->data + o;
    uint16_t nv = rd16(p + 2);
    /* find a subtable: prefer format 4, platform 3 (Windows) */
    size_t best_fmt4 = 0; int have_fmt4 = 0;
    size_t best_any = 0;  int have_any = 0;
    for (uint16_t i = 0; i < nv; i++) {
        const uint8_t *r = p + 4 + (size_t)i * 8;
        uint16_t pid = rd16(r);
        uint16_t eid = rd16(r + 2);
        uint32_t sub = rd32(r + 4);
        if (sub + 4 > l) continue;
        uint16_t f2 = rd16(p + o + sub);
        if (f2 == 4) { best_fmt4 = (size_t)sub; have_fmt4 = 1; break; }
        if (!have_any) { best_any = (size_t)sub; have_any = 1; }
        (void)pid; (void)eid;
    }
    size_t sub_off = have_fmt4 ? best_fmt4 : (have_any ? best_any : 0);
    if (!sub_off) return 0;
    const uint8_t *s = p + sub_off;
    uint16_t fmt = rd16(s);
    if (fmt == 4) {
        uint16_t segX2 = rd16(s + 6);
        const uint8_t *ends   = s + 14;
        const uint8_t *starts = ends + segX2 + 2;
        const uint8_t *deltas = starts + segX2;
        const uint8_t *rangeo = deltas + segX2;
        uint16_t segs = segX2 / 2;
        for (uint16_t i = 0; i < segs; i++) {
            uint16_t end   = rd16(ends + (size_t)i * 2);
            uint16_t start = rd16(starts + (size_t)i * 2);
            int16_t  delta = rd16s(deltas + (size_t)i * 2);
            if (cp < start || cp > end) continue;
            uint16_t ro = rd16(rangeo + (size_t)i * 2);
            if (ro == 0) return (uint16_t)(cp + delta) & 0xFFFF;
            /* glyphIdArray is at rangeo + segX2 */
            const uint8_t *gid = rangeo + segX2 + (size_t)i * 2;
            /* index into array by (cp - start); ro is byte offset from rangeo[i] */
            uint16_t idx = (cp - start) + (uint16_t)(ro / 2);
            return rd16(gid + (size_t)idx * 2);
        }
        return 0;
    }
    if (fmt == 6) {
        uint16_t first = rd16(s + 6);
        uint16_t cnt   = rd16(s + 8);
        if (cp >= first && cp < (uint32_t)first + cnt)
            return rd16(s + 10 + (cp - first) * 2);
        return 0;
    }
    if (fmt == 0) {
        if (cp < 256) return s[6 + cp];
        return 0;
    }
    return 0;
}
