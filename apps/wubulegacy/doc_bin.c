/* doc_bin.c -- legacy Word (.doc) reader -> dm_doc.
 *
 * A .doc is a CFB container with a "WordDocument" stream holding the FIB
 * (File Information Block) and text, plus a "0Table"/"1Table" stream holding
 * the piece table (CLX). Modern .doc always uses the complex piece table, so
 * we parse the CLX -> PlcPcd -> piece descriptors, each of which points at a
 * run of characters in WordDocument that is either 16-bit Unicode or
 * 8-bit CP1252 (compressed). Paragraphs split on the CR (0x0D) mark.
 *
 * Clean-room C11. Read-only, text + paragraph structure. */

#include "legacy.h"
#include "legacy_internal.h"
#include "../../src/wubucfb/cfb.h"

#include <string.h>

/* CP1252 high range (0x80-0x9F) maps to specific code points; the rest of the
 * upper range equals Latin-1. Only the printable specials are worth mapping. */
static uint32_t cp1252(uint8_t b) {
    switch (b) {
        case 0x80: return 0x20AC; case 0x82: return 0x201A; case 0x83: return 0x0192;
        case 0x84: return 0x201E; case 0x85: return 0x2026; case 0x86: return 0x2020;
        case 0x87: return 0x2021; case 0x88: return 0x02C6; case 0x89: return 0x2030;
        case 0x8A: return 0x0160; case 0x8B: return 0x2039; case 0x8C: return 0x0152;
        case 0x8E: return 0x017D; case 0x91: return 0x2018; case 0x92: return 0x2019;
        case 0x93: return 0x201C; case 0x94: return 0x201D; case 0x95: return 0x2022;
        case 0x96: return 0x2013; case 0x97: return 0x2014; case 0x98: return 0x02DC;
        case 0x99: return 0x2122; case 0x9A: return 0x0161; case 0x9B: return 0x203A;
        case 0x9C: return 0x0153; case 0x9E: return 0x017E; case 0x9F: return 0x0178;
        default:   return b;   /* 0x00-0x7F and 0xA0-0xFF: identity/Latin-1 */
    }
}

/* Growable UTF-8 sink. */
typedef struct { char *b; size_t cap, len; } sink;
static void sink_cp(sink *s, uint32_t cp) {
    if (s->len + 4 > s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 256;
        char *nb = realloc(s->b, nc);
        if (!nb) return;
        s->b = nb; s->cap = nc;
    }
    char *p = s->b + s->len;
    lg_put_utf8(cp, &p);
    s->len = (size_t)(p - s->b);
}

/* Flush the current paragraph text (up to but excluding a CR) into dm_doc. */
static void push_para(dm_doc *doc, const char *text, size_t n) {
    /* trim a trailing NUL/whitespace-only paragraph but keep real content */
    if (doc->n + 1 > doc->cap) {
        doc->cap = doc->cap ? doc->cap * 2 : 8;
        doc->blocks = realloc(doc->blocks, doc->cap * sizeof(*doc->blocks));
    }
    dm_block *blk = &doc->blocks[doc->n++];
    memset(blk, 0, sizeof *blk);
    blk->kind = DM_BLOCK_PARA;
    char *t = malloc(n + 1);
    if (t) { memcpy(t, text, n); t[n] = '\0'; }
    blk->para.text = t;
    blk->para.style = NULL;
    blk->para.bold = 0;
}

int wubulegacy_read_doc(const char *path, dm_doc *out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);

    size_t flen = 0;
    uint8_t *file = lg_slurp(path, &flen);
    if (!file) return -1;
    wubucfb *c = wubucfb_open(file, flen);
    free(file);
    if (!c) return -1;

    uint8_t *wd = NULL; size_t wdlen = 0;
    if (wubucfb_read_stream(c, "WordDocument", &wd, &wdlen) != 0 || wdlen < 0x200) {
        wubucfb_close(c); free(wd); return -1;
    }

    uint16_t flags = lg_rd16(wd + 0x000A);
    int which_table = (flags & 0x0200) ? 1 : 0;
    uint32_t fcClx = lg_rd32(wd + 0x01A2);
    uint32_t lcbClx = lg_rd32(wd + 0x01A6);

    uint8_t *tbl = NULL; size_t tbllen = 0;
    const char *tname = which_table ? "1Table" : "0Table";
    if (wubucfb_read_stream(c, tname, &tbl, &tbllen) != 0) {
        /* fall back to the other table stream name */
        wubucfb_read_stream(c, which_table ? "0Table" : "1Table", &tbl, &tbllen);
    }
    wubucfb_close(c);

    sink s = {0};
    int used_piece_table = 0;

    if (tbl && lcbClx > 0 && (size_t)fcClx + lcbClx <= tbllen) {
        /* Walk the Clx: skip Prc blocks (0x01), find the Pcdt (0x02). */
        size_t p = fcClx, end = (size_t)fcClx + lcbClx;
        while (p < end) {
            uint8_t clxt = tbl[p];
            if (clxt == 0x01) {
                if (p + 3 > end) break;
                uint16_t cb = lg_rd16(tbl + p + 1);
                p += 3 + cb;
            } else if (clxt == 0x02) {
                if (p + 5 > end) break;
                uint32_t lcb = lg_rd32(tbl + p + 1);
                size_t pcdt = p + 5;
                if (pcdt + lcb > tbllen) break;
                /* PlcPcd: (n+1) CPs (u32) then n PCDs (8 bytes). Solve n. */
                if (lcb < 4) break;
                size_t n = (lcb - 4) / (4 + 8);
                const uint8_t *cps = tbl + pcdt;
                const uint8_t *pcds = tbl + pcdt + (n + 1) * 4;
                for (size_t i = 0; i < n; i++) {
                    uint32_t cpStart = lg_rd32(cps + i * 4);
                    uint32_t cpEnd   = lg_rd32(cps + (i + 1) * 4);
                    uint32_t nchars = cpEnd - cpStart;
                    const uint8_t *pcd = pcds + i * 8;
                    uint32_t fc = lg_rd32(pcd + 2);
                    int compressed = (fc & 0x40000000u) != 0;
                    uint32_t off = compressed ? (fc & 0x3FFFFFFFu) / 2 : (fc & 0x3FFFFFFFu);
                    for (uint32_t k = 0; k < nchars; k++) {
                        uint32_t cp;
                        if (compressed) {
                            size_t bo = off + k;
                            if (bo >= wdlen) break;
                            cp = cp1252(wd[bo]);
                        } else {
                            size_t bo = off + (size_t)k * 2;
                            if (bo + 1 >= wdlen) break;
                            cp = lg_rd16(wd + bo);
                        }
                        sink_cp(&s, cp);
                    }
                }
                used_piece_table = 1;
                break;
            } else {
                break;
            }
        }
    }

    if (!used_piece_table) {
        /* Fallback: contiguous text between fcMin and fcMac (old simple docs).
         * fcMin at 0x0018, fcMac at 0x001C (byte offsets into WordDocument). */
        uint32_t fcMin = lg_rd32(wd + 0x0018);
        uint32_t fcMac = lg_rd32(wd + 0x001C);
        if (fcMac > fcMin && fcMac <= wdlen) {
            for (uint32_t i = fcMin; i < fcMac; i++) sink_cp(&s, cp1252(wd[i]));
        }
    }

    free(wd); free(tbl);

    /* Split the decoded text into paragraphs on CR (0x0D). Word uses CR (\r)
     * as the paragraph mark; drop other control chars except tab/newline. */
    if (s.b) {
        /* re-scan the UTF-8 buffer, splitting on the CR byte 0x0D and skipping
         * cell/row marks (0x07) and other low control bytes. */
        size_t start = 0;
        char *clean = malloc(s.len + 1);
        size_t cl = 0;
        for (size_t i = 0; i <= s.len; i++) {
            unsigned char ch = (i < s.len) ? (unsigned char)s.b[i] : 0x0D; /* flush tail */
            if (ch == 0x0D || ch == 0x07 || ch == 0x0C || i == s.len) {
                if (cl > 0) push_para(out, clean, cl);
                cl = 0; start = i + 1; (void)start;
            } else if (ch == 0x0B) {
                clean[cl++] = '\n';   /* vertical tab = line break within para */
            } else if (ch >= 0x20 || ch == 0x09 || ch == 0x0A) {
                clean[cl++] = (char)ch;
            }
            /* other control bytes (field marks 0x13/0x14/0x15 etc.) dropped */
        }
        free(clean);
    }
    free(s.b);

    return 0;
}
