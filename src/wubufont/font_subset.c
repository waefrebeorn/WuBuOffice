/* font_subset.c -- N1: sfnt glyph subsetting for PDF FontFile2 embedding.
 *
 * Produces a valid subset sfnt containing only the glyphs reachable from
 * a set of used codepoints (cmap lookup + composite component closure).
 * Tables: head/hhea/maxp/name/OS2/post copied verbatim (offset-independent
 * parts), cmap rebuilt as format 4, hmtx rebuilt per kept gid, glyf copied
 * raw per-glyph with loca rebuilt (long format).
 *
 * Composite closure: component glyph refs are parsed from the raw glyf
 * entries (bit 0x08 of the flags word = MORE_COMPONENTS walk).
 *
 * C11; reuses wubufont's table reader. */
#include "font_subset.h"
#include "wubufont.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG(a,b,c,d) ((uint32_t)((a)<<24)|((b)<<16)|((c)<<8)|(d))

static uint16_t rd16(const uint8_t *p){ return (uint16_t)((p[0]<<8)|p[1]); }
static uint32_t rd32(const uint8_t *p){
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}
static void wr32(uint8_t *p, uint32_t v){ p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static void wr16(uint8_t *p, uint16_t v){ p[0]=v>>8; p[1]=v&0xFF; }

typedef struct { uint32_t tag; size_t off, len; } TEnt;
typedef struct { uint32_t tag; const uint8_t *d; size_t len; } SPart;

static int find_tbl(const Font *f, uint32_t tag, TEnt *e){
    size_t n = font_table_count(f);
    for (size_t i = 0; i < n; i++)
        if (font_table_tag(f, i) == tag){
            font_table_range(f, i, &e->off, &e->len);
            return 1;
        }
    return 0;
}

int wubufont_subset(Font *f, const uint32_t *codepoints, size_t ncodes,
                    uint8_t **out, size_t *out_len){
    if (!f || !out || !out_len) return -1;
    *out = NULL; *out_len = 0;
    const uint8_t *data = font_data(f);
    size_t dsize = font_data_size(f);
    if (!data || !dsize) return -1;

    /* --- 1. seed glyph set from codepoints via cmap, close composites --- */
    size_t nglyphs = font_glyph_count(f);
    char *keep = calloc(nglyphs ? nglyphs : 1, 1);   /* keep[gid] = 1 */
    int nkeep = 0;
    /* composite closure worklist */
    uint16_t *stack = malloc(sizeof(uint16_t) * (nglyphs ? nglyphs : 1));
    int sp = 0;
    for (size_t i = 0; i < ncodes; i++){
        uint16_t gi = font_gid(f, codepoints[i]);
        if (gi && gi < nglyphs && !keep[gi]){ keep[gi] = 1; nkeep++; stack[sp++] = gi; }
    }
    keep[0] = 1;   /* .notdef always kept */
    /* locate glyf/loca/raw head for composite walking */
    TEnt g = {0}, lo = {0}, he = {0};
    int have_glyf = find_tbl(f, TAG('g','l','y','f'), &g);
    find_tbl(f, TAG('l','o','c','a'), &lo);
    find_tbl(f, TAG('h','e','a','d'), &he);
    if (!have_glyf || !have_glyf){ free(keep); free(stack); return -1; }
    int indexToLocLong = he.len >= 52 && rd16(data + he.off + 50) == 1;
    while (sp > 0){
        uint16_t gid = stack[--sp];
        /* raw glyph span from loca */
        size_t s0, s1;
        if (indexToLocLong){
            const uint8_t *lp = data + lo.off + (size_t)gid * 4;
            s0 = rd32(lp); s1 = rd32(lp + 4);
        } else {
            const uint8_t *lp = data + lo.off + (size_t)gid * 2;
            s0 = (size_t)rd16(lp) * 2; s1 = (size_t)rd16(lp + 2) * 2;
        }
        if (s1 <= s0) continue;
        if (g.off + s1 > dsize) continue;
        /* parse the glyph's own header to find its flags/components region:
         * we need composite detection. numberOfContours is int16 at start;
         * negative => composite. Walk components reading flags words. */
        const uint8_t *gp = data + g.off + s0;
        size_t glen = s1 - s0;
        if (glen < 10) continue;
        int16_t ncont = (int16_t)rd16(gp);
        if (ncont < 0){
            /* composite: walk component records */
            size_t p = 10;
            for (;;){
                if (p + 4 > glen) break;
                uint16_t flags = rd16(gp + p);
                uint16_t cgid = rd16(gp + p + 2);
                p += 4;
                if (cgid < nglyphs && !keep[cgid]){
                    keep[cgid] = 1; nkeep++;
                    if (sp < (int)nglyphs) stack[sp++] = cgid;
                }
                /* args: 2 or 4 bytes */
                p += (flags & 0x0001) ? 4 : 2;
                /* more components? */
                if (!(flags & 0x0020)) break;
            }
        }
    }
    free(stack);
    if (nkeep == 0){ free(keep); return -1; }

    /* --- 2. build old->new gid map (keep original order, compact) --- */
    uint16_t *map = calloc(nglyphs, sizeof(uint16_t));
    uint16_t next = 0;
    for (size_t g2 = 0; g2 < nglyphs; g2++)
        if (keep[g2]) map[g2] = next++;
    uint16_t new_nglyphs = next;
    if (new_nglyphs < 2) new_nglyphs = 2;   /* spec minimum */

    /* --- 3. rebuild tables into one output buffer --- */
    /* collect needed source tables verbatim */
    static const uint32_t copy_tags[] = {
        TAG('h','e','a','d'), TAG('h','h','e','a'), TAG('m','a','x','p'),
        TAG('n','a','m','e'), TAG('O','S','/','2'), TAG('p','o','s','t'),
        TAG('c','v','t',' '), TAG('f','p','g','m'), TAG('p','r','e','p'),
    };
    enum { NCOPY = sizeof copy_tags / sizeof copy_tags[0] };
    SPart parts[NCOPY + 8];
    int nparts = 0;
    for (int i = 0; i < NCOPY; i++){
        TEnt e; 
        if (find_tbl(f, copy_tags[i], &e)){
            parts[nparts].tag = copy_tags[i];
            parts[nparts].d = data + e.off;
            parts[nparts].len = e.len;
            nparts++;
        }
    }
    /* new loca (long format): new_nglyphs+1 entries */
    size_t loca_len = ((size_t)new_nglyphs + 1) * 4;
    uint8_t *loca_new = calloc(loca_len, 1);
    /* new glyf: concatenate kept glyphs in new-id order */
    size_t glyf_cap = 1024; uint8_t *glyf_new = malloc(glyf_cap); size_t glyf_len = 0;
    uint32_t *loca_vals = calloc((size_t)new_nglyphs + 1, sizeof(uint32_t));
    for (uint16_t old = 0; old < nglyphs; old++){
        if (!keep[old]) continue;
        uint16_t nw = map[old];
        size_t s0, s1;
        if (indexToLocLong){
            const uint8_t *lp = data + lo.off + (size_t)old * 4;
            s0 = rd32(lp); s1 = rd32(lp + 4);
        } else {
            const uint8_t *lp = data + lo.off + (size_t)old * 2;
            s0 = (size_t)rd16(lp) * 2; s1 = (size_t)rd16(lp + 2) * 2;
        }
        size_t glen = (s1 > s0 && g.off + s1 <= dsize) ? (s1 - s0) : 0;
        loca_vals[nw] = glyf_len;
        if (glen){
            if (glyf_len + glen > glyf_cap){ glyf_cap = (glyf_len + glen) * 2; glyf_new = realloc(glyf_new, glyf_cap); }
            memcpy(glyf_new + glyf_len, data + g.off + s0, glen);
            glyf_len += glen;
            if (glyf_len & 3){ size_t pad = 4 - (glyf_len & 3);
                memset(glyf_new + glyf_len, 0, pad); glyf_len += pad; }
        }
    }
    loca_vals[new_nglyphs] = glyf_len;
    for (uint16_t i2 = 0; i2 <= new_nglyphs; i2++)
        wr32(loca_new + (size_t)i2 * 4, loca_vals[i2]);
    free(loca_vals);

    parts[nparts].tag = TAG('g','l','y','f'); parts[nparts].d = glyf_new; parts[nparts].len = glyf_len; nparts++;
    parts[nparts].tag = TAG('l','o','c','a'); parts[nparts].d = loca_new; parts[nparts].len = loca_len; nparts++;
    /* new maxp: patch numGlyphs */
    for (int i = nparts - nparts; i < nparts; i++){
        if (parts[i].tag == TAG('m','a','x','p') && parts[i].len >= 6){
            uint8_t *mp = malloc(parts[i].len);
            memcpy(mp, parts[i].d, parts[i].len);
            wr16(mp + 4, new_nglyphs);
            parts[i].d = mp;   /* keep original table length */
        }
    }
    /* new hmtx: subset advances in new-gid order */
    TEnt hm; int have_hmtx = find_tbl(f, TAG('h','m','t','x'), &hm);
    if (have_hmtx){
        size_t hl = (size_t)new_nglyphs * 4;
        uint8_t *hm_new = calloc(hl, 1);
        for (uint16_t old_g = 0; old_g < nglyphs; old_g++){
            if (!keep[old_g]) continue;
            uint16_t nw = map[old_g];
            if (hm.off + (size_t)old_g * 4 + 4 <= dsize)
                memcpy(hm_new + (size_t)nw * 4, data + hm.off + (size_t)old_g * 4, 4);
        }
        parts[nparts].tag = TAG('h','m','t','x'); parts[nparts].d = hm_new; parts[nparts].len = hl; nparts++;
    }
    /* new cmap: format 4, one segment per kept-codepoint run */
    {
        uint32_t *cps = malloc(sizeof(uint32_t) * (ncodes ? ncodes : 1));
        size_t nc = 0;
        for (size_t i = 0; i < ncodes; i++)
            if (font_gid(f, codepoints[i])) cps[nc++] = codepoints[i];
        for (size_t i = 0; i + 1 < nc; i++)
            for (size_t j2 = i + 1; j2 < nc; j2++)
                if (cps[j2] < cps[i]){ uint32_t t = cps[i]; cps[i] = cps[j2]; cps[j2] = t; }

        /* build segments: contiguous (cp and gid both +1) runs merge */
        uint16_t *seg_start = malloc(sizeof(uint16_t) * (nc + 1));
        uint16_t *seg_end   = malloc(sizeof(uint16_t) * (nc + 1));
        size_t nsegs = 0;
        for (size_t i = 0; i < nc; i++){
            if (i == 0 || cps[i] != cps[i-1] + 1 ||
                font_gid(f,cps[i]) != font_gid(f,cps[i-1]) + 1){
                nsegs++;
                seg_start[nsegs-1] = (uint16_t)cps[i];
            }
            seg_end[nsegs-1] = (uint16_t)cps[i];
        }
        nsegs++;                       /* final 0xFFFF segment */
        size_t cm_len = 12 + 8 + 14 + (size_t)nsegs * 8 + 16;

        uint8_t *cm = calloc(cm_len, 1);
        wr16(cm, 0);                              /* version */
        wr16(cm+2, 1);                            /* numTables = 1 */
        /* encoding record at offset 4: platform 3 (Windows), encoding 1 (UCS-2) */
        wr16(cm+4, 3); wr16(cm+6, 1);
        wr32(cm+8, 20);                           /* subtable offset */
        uint8_t *f4 = cm + 20;
        size_t ec_off = f4 - cm + 14;
        size_t sc_off = f4 - cm + 14 + nsegs*2 + 2;
        size_t idd_off = sc_off + nsegs*2;
        size_t idr_off = idd_off + nsegs*2;
        wr16(f4, 4);
        wr16(f4+2, (uint16_t)(16 + nsegs*8));
        wr16(f4+4, 0);
        wr16(f4+6, (uint16_t)(nsegs*2));
        int sr2 = 0; while ((size_t)(1 << (sr2+1)) <= nsegs) sr2++;
        wr16(f4+8, (uint16_t)((1<<sr2)*2));
        wr16(f4+10, (uint16_t)(nsegs*2 - (1<<sr2)*2));
        for (size_t i = 0; i < nsegs - 1; i++){
            wr16(cm + ec_off + i*2, seg_end[i]);
            wr16(cm + sc_off + i*2, seg_start[i]);
            /* delta maps cp -> NEW gid: new_gid(seg_start) - seg_start */
            int delta = (int)map[font_gid(f, seg_start[i])] - (int)seg_start[i];
            wr16(cm + idd_off + i*2, (uint16_t)delta);
            wr16(cm + idr_off + i*2, 0);
        }
        /* terminator segment */
        wr16(cm + ec_off + (nsegs-1)*2, 0xFFFF);
        wr16(cm + sc_off + (nsegs-1)*2, 0xFFFF);
        wr16(cm + idd_off + (nsegs-1)*2, 1);
        wr16(cm + idr_off + (nsegs-1)*2, 0);

        parts[nparts].tag = TAG('c','m','a','p');
        parts[nparts].d = cm; parts[nparts].len = cm_len; nparts++;
        free(cps); free(seg_start); free(seg_end);
        /* cm ownership transferred into parts[] body copy below */
    }

    /* --- 4. assemble sfnt --- */
    /* directory: 12 + nparts*16, tables 4-aligned */
    size_t off_cur = 12 + (size_t)nparts * 16;
    struct { uint32_t tag, chk, off, len; } dirent[NCOPY + 8];
    uint8_t *body = malloc(1024); size_t body_cap = 1024, body_len = 0;
    /* sort parts by tag (sfnt requirement) */
    for (int i = 0; i < nparts; i++)
        for (int k = i+1; k < nparts; k++)
            if (parts[k].tag < parts[i].tag){
                SPart tmp = parts[i]; parts[i] = parts[k]; parts[k] = tmp;
            }
    for (int i = 0; i < nparts; i++){
        uint32_t chk = 0;
        for (size_t k = 0; k + 3 < parts[i].len + ((4 - parts[i].len % 4) % 4); k += 4)
            chk += rd32(parts[i].d + k);
        dirent[i].tag = parts[i].tag; dirent[i].chk = chk;
        dirent[i].off = off_cur; dirent[i].len = parts[i].len;
        if (body_len + parts[i].len + 4 > body_cap){
            body_cap = (body_len + parts[i].len + 64) * 2;
            body = realloc(body, body_cap);
        }
        memcpy(body + body_len, parts[i].d, parts[i].len);
        body_len += parts[i].len;
        while (body_len & 3){ body[body_len++] = 0; }
        off_cur = 12 + (size_t)nparts * 16 + body_len;
    }
    size_t total = off_cur;
    uint8_t *result = calloc(total + 64, 1);
    wr32(result, 0x00010000);
    wr16(result+4, (uint16_t)nparts);
    int sr2 = 0; while ((size_t)(1 << (sr2+1)) <= (size_t)nparts) sr2++;
    wr16(result+6, (uint16_t)((1<<sr2)*16));
    wr16(result+8, (uint16_t)(sr2));
    wr16(result+10, (uint16_t)(nparts*16 - (1<<sr2)*16));
    for (int i = 0; i < nparts; i++){
        uint8_t *de = result + 12 + (size_t)i * 16;
        wr32(de, dirent[i].tag); wr32(de+4, dirent[i].chk);
        wr32(de+8, (uint32_t)dirent[i].off); wr32(de+12, (uint32_t)dirent[i].len);
    }
    memcpy(result + 12 + (size_t)nparts*16, body, body_len);
    free(body);
    free(loca_new); free(glyf_new); free(map); free(keep);
    *out = result; *out_len = total;
    return 0;
}
