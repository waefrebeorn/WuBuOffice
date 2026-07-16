#include "wubufont_internal.h"

/* ---- name table -> UTF-8 ---- */
char *font_name(const Font *f, uint16_t name_id) {
    size_t o, l;
    if (!font_find_table(f, TAG('n','a','m','e'), &o, &l) || l < 6) return NULL;
    const uint8_t *p = f->data + o;
    uint16_t count = rd16(p + 2);
    uint16_t stor  = rd16(p + 4);
    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *r = p + 6 + (size_t)i * 12;
        uint16_t pid  = rd16(r);
        uint16_t eid  = rd16(r + 2);
        uint16_t nid  = rd16(r + 6);
        uint16_t slen = rd16(r + 8);
        uint16_t soff = rd16(r + 10);
        if (nid != name_id) continue;
        /* prefer Windows (pid 3) UTF-16BE (eid 1) */
        if (!(pid == 3 && eid == 1) && i + 1 < count) continue; /* try to find a better one first pass */
        const uint8_t *s = p + stor + soff;
        if ((size_t)stor + soff + slen > f->size) continue;
        /* decode UTF-16BE -> UTF-8 */
        char *out = xrealloc(NULL, (size_t)slen * 3 + 1);
        size_t k = 0;
        for (uint16_t j = 0; j + 1 < slen; j += 2) {
            uint32_t cp = (uint32_t)((s[j] << 8) | s[j + 1]);
            if (cp < 0x80) out[k++] = (char)cp;
            else if (cp < 0x800) { out[k++] = (char)(0xC0 | (cp >> 6)); out[k++] = (char)(0x80 | (cp & 0x3F)); }
            else { out[k++] = (char)(0xE0 | (cp >> 12)); out[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[k++] = (char)(0x80 | (cp & 0x3F)); }
        }
        out[k] = '\0';
        return out;
    }
    return NULL;
}
