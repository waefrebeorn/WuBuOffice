/* shape.c -- see shape.h. Bounded Bidi visual reordering. */
#include "shape.h"

#include <string.h>
#include <stddef.h>

int shape_is_rtl_codepoint(unsigned long cp){
    /* Arabic and derived blocks (0600–06FF, 0750–077F, 08A0–08FF, FB50–FDFF,
     * FE70–FEFF), Hebrew (0590–05FF), Syriac (0700–074F), Thaana (0780–07BF),
     * NKo (07C0–07FF), Samaritan (0800–083F), etc. */
    if (cp >= 0x0590 && cp <= 0x05FF) return 1;   /* Hebrew */
    if (cp >= 0x0600 && cp <= 0x06FF) return 1;   /* Arabic */
    if (cp >= 0x0700 && cp <= 0x074F) return 1;   /* Syriac */
    if (cp >= 0x0750 && cp <= 0x077F) return 1;   /* Arabic Supp */
    if (cp >= 0x0780 && cp <= 0x07BF) return 1;   /* Thaana */
    if (cp >= 0x07C0 && cp <= 0x07FF) return 1;   /* NKo */
    if (cp >= 0x0800 && cp <= 0x083F) return 1;   /* Samaritan */
    if (cp >= 0x0840 && cp <= 0x085F) return 1;   /* Mandaic */
    if (cp >= 0x08A0 && cp <= 0x08FF) return 1;   /* Arabic Ext-A */
    if (cp >= 0xFB1D && cp <= 0xFB4F) return 1;   /* Hebrew preset */
    if (cp >= 0xFB50 && cp <= 0xFDFF) return 1;   /* Arabic pres forms-A */
    if (cp >= 0xFE70 && cp <= 0xFEFF) return 1;   /* Arabic pres forms-B */
    if (cp >= 0x1E900 && cp <= 0x1E95F) return 1; /* Adlam */
    if (cp >= 0x10E60 && cp <= 0x10E7F) return 1; /* Rumi */
    return 0;
}

/* decode one UTF-8 codepoint from s; advance *pp; return cp (0 on end). */
static unsigned long utf8_next(const char **pp){
    const unsigned char *p = (const unsigned char*)*pp;
    unsigned long cp; int n;
    if (*p < 0x80){ cp = *p; n = 1; }
    else if ((*p & 0xE0) == 0xC0){ cp = *p & 0x1F; n = 2; }
    else if ((*p & 0xF0) == 0xE0){ cp = *p & 0x0F; n = 3; }
    else if ((*p & 0xF8) == 0xF0){ cp = *p & 0x07; n = 4; }
    else { cp = 0; n = 1; } /* invalid lead -> skip */
    for (int i=1;i<n;i++) cp = (cp<<6) | (p[i] & 0x3F);
    *pp = (const char*)(p + n);
    return cp;
}

/* direction of a codepoint: 1=RTL, 0=LTR. Numbers are treated LTR. */
static int cp_dir(unsigned long cp){
    if (cp >= '0' && cp <= '9') return 0;
    if (shape_is_rtl_codepoint(cp)) return 1;
    return 0;
}

size_t shape_reorder(const char *text, ShapeDir base, char *out, size_t outcap){
    if (!text || !out || outcap == 0){ if (out) out[0]=0; return 0; }
    /* Pass 1: split into directional runs. A run is a maximal span whose
     * characters share direction (RTL/LTR), with neutrals (spaces/punct) that
     * are NOT at run boundaries absorbed into the surrounding run direction.
     * For each run whose direction differs from base, reverse its bytes. */
    size_t n = strlen(text);
    /* copy logical into a working buffer we can reverse in place */
    char *work = (char*)text;  /* read-only source; we copy to out in passes */
    (void)work;

    /* Simpler correct approach: walk codepoints, classify each as its run
     * direction, then emit. We collect runs as [start,end) byte ranges. */
    #define MAXRUNS 1024
    size_t rstart[MAXRUNS]; size_t rend[MAXRUNS]; int rdir[MAXRUNS];
    int nruns = 0;

    const char *p = text;
    size_t pos = 0;
    while (*p){
        unsigned long cp = utf8_next(&p);
        int d = cp_dir(cp);
        size_t cpend = (size_t)(p - text);
        if (nruns == 0 || rdir[nruns-1] != d){
            if (nruns < MAXRUNS){ rstart[nruns]=pos; rend[nruns]=cpend; rdir[nruns]=d; nruns++; }
            else { rend[nruns-1] = cpend; }
        } else {
            rend[nruns-1] = cpend;
        }
        pos = cpend;
    }

    /* emit runs: for an RTL base paragraph the visual order is the run
     * sequence reversed (each run keeps its own direction). For LTR base we
     * emit in logical order. A run is internally reversed ONLY when the run
     * itself is RTL (right-to-left reading); LTR runs stay left-to-right even
     * inside an RTL paragraph. Reversal is done at the CODEPOINT level (not
     * bytes) so multi-byte UTF-8 glyphs are not corrupted. */
    int step = 1, start = 0, end = nruns;
    if (base == SHAPE_RTL){ start = nruns-1; end = -1; step = -1; }
    size_t outlen = 0;
    for (int r=start; r!=end; r+=step){
        int reverse = (rdir[r] == (int)SHAPE_RTL);   /* reverse only RTL runs */
        size_t a = rstart[r], b = rend[r];
        if (!reverse){
            for (size_t i=a; i<b && outlen+1<outcap; i++) out[outlen++] = text[i];
        } else {
            /* decode codepoints of the run, then emit them reversed */
            const char *rp = text + a;
            size_t runlen = b - a;
            /* count + collect codepoint byte-spans */
            #define MAXCP 512
            size_t cps[MAXCP], cpe[MAXCP]; int ncps = 0;
            const char *q = rp;
            while ((size_t)(q - rp) < runlen && ncps < MAXCP){
                const char *seg = q;
                unsigned long cp = utf8_next(&q); (void)cp;
                cps[ncps] = (size_t)(seg - text); cpe[ncps] = (size_t)(q - text);
                ncps++;
            }
            for (int i=ncps-1; i>=0 && outlen+1<outcap; i--)
                for (size_t j=cps[i]; j<cpe[i] && outlen+1<outcap; j++)
                    out[outlen++] = text[j];
            #undef MAXCP
        }
    }
    if (outlen < outcap) out[outlen] = '\0'; else out[outcap-1]='\0';
    return outlen;
    #undef MAXRUNS
}
