/* crnn_transcribe.c -- page -> line segmentation -> CRNN -> docmodel JSON.
 * See crnn_transcribe.h. C11, no deps beyond wubuocr core. */
#include "crnn_transcribe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "binarize.h"
#include "lexicon.h"

/* A pixel is "ink" when it deviates strongly from the page background. This
 * is adaptive: synthetic test pages use a dark background (low value) with
 * light text, while real scans/photos use a light background with dark text.
 * Either way, ink = |g - bg| > MARGIN. */
#define INK_MARGIN 40
static int is_ink(uint8_t g, int bg) { return g > bg ? (g - bg) > INK_MARGIN : (bg - g) > INK_MARGIN; }

/* precompose(base, mark): return the Unicode codepoint of base LETTER followed
 * by combining mark `mark` where a precomposed form exists, else 0. base must
 * be an ASCII letter; the returned codepoint matches base's case. Covers the
 * common Latin letters/marks. */
static unsigned int precompose(unsigned int base, unsigned int m) {
    int up = (base >= 'A' && base <= 'Z');
    unsigned int lo = up ? base + 0x20 : base;
    unsigned int pl = 0;
    switch (lo) {
    case 'a': switch (m) {
        case 0x300: pl=0x00E0; break; case 0x301: pl=0x00E1; break; case 0x302: pl=0x00E2; break;
        case 0x303: pl=0x00E3; break; case 0x304: pl=0x0101; break; case 0x306: pl=0x0103; break;
        case 0x307: pl=0x0227; break; case 0x308: pl=0x00E4; break; case 0x30A: pl=0x00E5; break;
        case 0x327: pl=0x00E7; break; case 0x328: pl=0x0105; break; } break;
    case 'e': switch (m) {
        case 0x300: pl=0x00E8; break; case 0x301: pl=0x00E9; break; case 0x302: pl=0x00EA; break;
        case 0x303: pl=0x1EB9; break; case 0x304: pl=0x0113; break; case 0x306: pl=0x0115; break;
        case 0x307: pl=0x0117; break; case 0x308: pl=0x00EB; break; case 0x30A: pl=0x1EBD; break; } break;
    case 'i': switch (m) {
        case 0x300: pl=0x00EC; break; case 0x301: pl=0x00ED; break; case 0x302: pl=0x00EE; break;
        case 0x303: pl=0x1ECB; break; case 0x304: pl=0x012B; break; case 0x306: pl=0x012D; break;
        case 0x307: pl=0x1E2F; break; case 0x308: pl=0x00EF; break; } break;
    case 'o': switch (m) {
        case 0x300: pl=0x00F2; break; case 0x301: pl=0x00F3; break; case 0x302: pl=0x00F4; break;
        case 0x303: pl=0x00F5; break; case 0x304: pl=0x014D; break; case 0x306: pl=0x014F; break;
        case 0x308: pl=0x00F6; break; case 0x30A: pl=0x00F8; break; case 0x328: pl=0x01EB; break; } break;
    case 'u': switch (m) {
        case 0x300: pl=0x00F9; break; case 0x301: pl=0x00FA; break; case 0x302: pl=0x00FB; break;
        case 0x303: pl=0x1EE5; break; case 0x304: pl=0x016B; break; case 0x306: pl=0x016D; break;
        case 0x308: pl=0x00FC; break; case 0x30A: pl=0x016F; break; case 0x328: pl=0x0173; break; } break;
    case 'y': switch (m) {
        case 0x300: pl=0x1EF2; break; case 0x301: pl=0x00FD; break; case 0x302: pl=0x1EF4; break;
        case 0x303: pl=0x1EF8; break; case 0x304: pl=0x0233; break; case 0x308: pl=0x00FF; break; } break;
    case 'c': switch (m) { case 0x307: pl=0x010B; break; case 0x327: pl=0x00E7; break; } break;
    case 'g': switch (m) { case 0x307: pl=0x0121; break; } break;
    case 'n': switch (m) { case 0x303: pl=0x00F1; break; case 0x328: pl=0x0149; break; } break;
    case 's': switch (m) { case 0x327: pl=0x015F; break; } break;
    case 'z': switch (m) { case 0x307: pl=0x017C; break; } break;
    }
    if (!pl) return 0;
    if (!up) return pl;
    /* uppercase the precomposed codepoint */
    if (pl >= 0x00E0 && pl <= 0x00FE && pl != 0x00F7) return pl - 0x20;  /* À..Þ, Ü, etc. */
    if (pl >= 0x0101 && pl <= 0x01FF && (pl & 1) == 1) return pl - 1;    /* Ā,Ē,Ī,Ō,Ū,Ă,Ą,... */
    return pl;  /* no simple uppercase form known */
}

size_t wubuocr_nfc_latin(const char *in, char *out, size_t outsz) {
    const unsigned char *p = (const unsigned char *)in;
    size_t o = 0;
    while (*p) {
        /* decode one codepoint */
        unsigned int c; int n;
        if (*p < 0x80)      { c = *p; n = 1; }
        else if ((*p&0xE0)==0xC0){ c=((*p&0x1F)<<6)|(p[1]&0x3F); n=2; }
        else if ((*p&0xF0)==0xE0){ c=((*p&0x0F)<<12)|((p[1]&0x3F)<<6)|(p[2]&0x3F); n=3; }
        else if ((*p&0xF8)==0xF0){ c=((*p&0x07)<<18)|((p[1]&0x3F)<<12)|((p[2]&0x3F)<<6)|(p[3]&0x3F); n=4; }
        else { /* invalid lead: copy raw, advance 1 */ if(o<outsz-1) out[o++]=*p; p++; continue; }
        /* if base is a Latin letter, look for following combining marks */
        if ((c>='A'&&c<='Z') || (c>='a'&&c<='z')) {
            unsigned int base = c;
            while (p[n] >= 0xCC && p[n] <= 0xCD) {  /* lead of U+0300..U+036F */
                unsigned int m = ((p[n]&0x0F)<<6) | (p[n+1]&0x3F);  /* 0x300..0x36F */
                unsigned int comp = precompose(base, m);
                if (comp) { base = comp; p += 2; }   /* consume the mark, keep composing */
                else break;                            /* unknown mark: stop */
            }
            c = base;
        }
        /* encode c into out */
        if (c < 0x80) { if(o<outsz-1) out[o++]= (char)c; }
        else if (c < 0x800) { if(o<outsz-2){ out[o++]=(char)(0xC0|(c>>6)); out[o++]=(char)(0x80|(c&0x3F)); } }
        else if (c < 0x10000) { if(o<outsz-3){ out[o++]=(char)(0xE0|(c>>12)); out[o++]=(char)(0x80|((c>>6)&0x3F)); out[o++]=(char)(0x80|(c&0x3F)); } }
        else { if(o<outsz-4){ out[o++]=(char)(0xF0|(c>>18)); out[o++]=(char)(0x80|((c>>12)&0x3F)); out[o++]=(char)(0x80|((c>>6)&0x3F)); out[o++]=(char)(0x80|(c&0x3F)); } }
        p += n;
    }
    if(o<outsz) out[o]='\0';
    return o;
}

/* Script / language auto-detection (#46 / #94): classify a text string by the
 * Unicode ranges of its non-ASCII codepoints. Single-byte ASCII is ambiguous
 * (could be English/French/etc.) so it falls back to "en". This is a lightweight
 * heuristand not a full language model, but it gives a usable per-block `lang`
 * tag for downstream (PDF/UA /Lang, accessibility, charset routing). */
static const char *detect_script(const char *s) {
    if (!s || !*s) return "en";
    int cp = 0, n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        unsigned int c;
        if (*p < 0x80)      { c = *p; p += 1; }
        else if ((*p & 0xE0) == 0xC0) { c = ((*p & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0) { c = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
        else if ((*p & 0xF8) == 0xF0) { c = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; }
        else { p++; continue; }
        /* ignore whitespace/punctuation/currency for script voting */
        if (c < 0x80) continue;
        n++;
        if (c >= 0x0400 && c <= 0x04FF) return "ru";        /* Cyrillic */
        if (c >= 0x0370 && c <= 0x03FF) return "el";        /* Greek */
        if (c >= 0x0590 && c <= 0x05FF) return "he";        /* Hebrew */
        if (c >= 0x0600 && c <= 0x06FF) return "ar";        /* Arabic */
        if (c >= 0x0900 && c <= 0x097F) return "hi";        /* Devanagari */
        if (c >= 0x3040 && c <= 0x30FF) return "ja";        /* Hiragana/Katakana */
        if (c >= 0x3100 && c <= 0x312F) return "zh";        /* Bopomofo (zh hint) */
        if (c >= 0x4E00 && c <= 0x9FFF) return "zh";        /* CJK ideographs */
        if (c >= 0xAC00 && c <= 0xD7A3) return "ko";        /* Hangul */
        if (c >= 0x0E00 && c <= 0x0E7F) return "th";        /* Thai */
    }
    (void)cp;
    return n ? "und" : "en";   /* und = script detected but unmapped; en = ASCII-only */
}

/* Math/equation recognition (#48): flag a text line as a math/equation region.
 * Signals: Greek/math Unicode codepoints (α β γ Δ ∫ ∑ √ ± ≠ ≤ ≥ ≈ ∞ ∂ × ÷),
 * relational/operator chars (= + - * / ^ _ < >), fraction notation (a/b with
 * alnum around the slash), and nested parentheses (depth >= 2 with an operator).
 * Heuristic but effective for isolating displayed equations from prose. A
 * downstream LaTeX model can fill the `latex` placeholder in the emitted block. */
static int detect_math_line(const char *s) {
    if (!s || !*s) return 0;
    int ops = 0, digits = 0, greek = 0, frac = 0, depth = 0, maxdepth = 0;
    int prev_alnum = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        unsigned int c;
        if (*p < 0x80)      { c = *p; p += 1; }
        else if ((*p & 0xE0) == 0xC0) { c = ((*p & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0) { c = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
        else if ((*p & 0xF8) == 0xF0) { c = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; }
        else { p++; continue; }
        int alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (c >= '0' && c <= '9') digits++;
        if (strchr("=+-*/^_<>", (int)c)) ops++;
        if (c == '(') { depth++; if (depth > maxdepth) maxdepth = depth; }
        else if (c == ')') { if (depth > 0) depth--; }
        if (c == '/' && prev_alnum && alnum) frac++;
        /* Greek + common math symbols */
        if ((c >= 0x03B1 && c <= 0x03C9) || (c >= 0x0391 && c <= 0x03A9)) greek++;
        switch (c) {
            case 0x222B: case 0x222E: case 0x2211: case 0x220F: case 0x221A:
            case 0x00B1: case 0x2260: case 0x2264: case 0x2265: case 0x2248:
            case 0x221E: case 0x00F7: case 0x00D7: case 0x2202:
                ops++; greek++; break;
        }
        prev_alnum = alnum;
    }
    if (greek > 0 && (ops > 0 || digits > 0)) return 1;
    if (ops >= 2) return 1;
    if (frac > 0 && digits > 0) return 1;
    if (maxdepth >= 2 && ops > 0) return 1;
    return 0;
}

int wubuocr_detect_math_line(const char *s) { return detect_math_line(s); }

/* Growable byte-append: realloc (doubling) as needed, then copy `n` bytes
 * from `s`. Never uses strcat on a possibly-reallocated pointer. */
static int ba_append(char **buf, size_t *len, size_t *cap, const char *s, size_t n) {
    if (n == 0) return 0;
    if (*len + n + 1 > *cap) {
        size_t ncap = *cap ? *cap : 64;
        while (*len + n + 1 > ncap) ncap *= 2;
        char *nb = realloc(*buf, ncap);
        if (!nb) return -1;
        *buf = nb; *cap = ncap;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = '\0';
    return 0;
}

static int ba_append_str(char **buf, size_t *len, size_t *cap, const char *s) {
    return ba_append(buf, len, cap, s, strlen(s));
}

/* Append a JSON-escaped copy of `s`. */
static int ba_append_json_escaped(char **buf, size_t *len, size_t *cap, const char *s) {
    for (const char *p = s; *p; p++) {
        char lit[8];
        const char *esc = NULL;
        size_t elen = 1;
        switch (*p) {
            case '"':  esc = "\\\""; elen = 2; break;
            case '\\': esc = "\\\\"; elen = 2; break;
            case '\n': esc = "\\n";  elen = 2; break;
            case '\r': esc = "\\r";  elen = 2; break;
            case '\t': esc = "\\t";  elen = 2; break;
            case '\b': esc = "\\b";  elen = 2; break;
            case '\f': esc = "\\f";  elen = 2; break;
            default: lit[0] = *p; esc = lit; elen = 1; break;
        }
        if (ba_append(buf, len, cap, esc, elen) != 0) return -1;
    }
    return 0;
}

/* Count ink pixels in row `y` that are 8-connected to another ink pixel
 * (any of the 8 neighbours is ink). Real glyph strokes -- even thin vertical
 * stems -- are connected, so their pixels count. Salt-and-pepper noise is
 * single isolated pixels with no ink neighbour, so it counts ~0. This cleanly
 * separates text rows from noisy blank rows regardless of ink density. */
static int row_paired_ink(const OcrImage *page, int y, int bg, int W) {
    int H = (int)ocr_image_height(page);
    int cnt = 0;
    for (int x = 0; x < W; x++) {
        if (!is_ink(ocr_image_get(page, (size_t)x, (size_t)y), bg)) continue;
        int connected = 0;
        for (int dy = -1; dy <= 1 && !connected; dy++) {
            int ny = y + dy;
            if (ny < 0 || ny >= H) continue;
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx;
                if (nx < 0 || nx >= W) continue;
                if (is_ink(ocr_image_get(page, (size_t)nx, (size_t)ny), bg)) { connected = 1; break; }
            }
        }
        if (connected) cnt++;
    }
    return cnt;
}

/* Build a strip-tall (height == `strip`) line image centered on the text
 * line at vertical position `cy`. The window is a FIXED strip-tall region
 * around the line center -- NO vertical scaling. The CRNN was trained on
 * glyphs centered in a fixed strip-tall cell; stretching the variable-height
 * ink band to `strip` pixels distorts glyph proportions and tanks accuracy.
 * (Lines are cropped per-column inline in the recognition pass below.) */

/* Rotate `src` about its center by `deg` degrees (nearest-neighbour). The
 * canvas is kept the same size; rotated-out corners are filled with `fill`
 * (use the page background so they don't read as ink). Returns a new image. */
static OcrImage *rotate_img(const OcrImage *src, double deg, uint8_t fill) {
    int W = (int)ocr_image_width(src), H = (int)ocr_image_height(src);
    int cx = W / 2, cy = H / 2;
    double a = deg * 3.141592653589793 / 180.0, ca = cos(a), sa = sin(a);
    OcrImage *dst = ocr_image_create((size_t)W, (size_t)H);
    if (!dst) return NULL;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        ocr_image_set(dst, (size_t)x, (size_t)y, fill);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int sx = (int)(cx + (x - cx) * ca + (y - cy) * sa);
        int sy = (int)(cy - (x - cx) * sa + (y - cy) * ca);
        if (sx >= 0 && sx < W && sy >= 0 && sy < H)
            ocr_image_set(dst, (size_t)x, (size_t)y, ocr_image_get(src, (size_t)sx, (size_t)sy));
    }
    return dst;
}

/* Projection-profile deskew: scan small rotation angles and pick the one that
 * maximizes the variance of the per-row ink count. Text lines are most
 * separated (and background bands emptiest) at the correct skew, so this
 * straightens a slightly-rotated page before line segmentation. */
static OcrImage *deskew_page(const OcrImage *src, int bg) {
    if (getenv("DESKEW") && getenv("DESKEW")[0] == '0') return NULL;
    double best = -1; int bestk = 0;
    uint8_t fill = (uint8_t)(bg > 127 ? 235 : 15);
    for (int k = -16; k <= 16; k++) {
        double deg = k * 0.5;
        OcrImage *r = rotate_img(src, deg, fill);
        int H = (int)ocr_image_height(r), W = (int)ocr_image_width(r);
        double mean = 0;
        int *proj = calloc((size_t)H, sizeof(int));
        for (int y = 0; y < H; y++) {
            int cnt = row_paired_ink(r, y, bg, W);
            proj[y] = cnt > 0 ? cnt : 0;   /* paired>0 => real ink row */
            mean += proj[y];
        }
        mean /= H;
        double var = 0;
        for (int y = 0; y < H; y++) { double d = proj[y] - mean; var += d * d; }
        free(proj); ocr_image_free(r);
        if (var > best) { best = var; bestk = k; }
    }
    if (bestk == 0) return NULL;
    return rotate_img(src, bestk * 0.5, fill);
}

/* Detect a ruled (bordered) table grid (#32). Operates on the normalized page
 * `pg` (which after polarity normalization is DARK bg + LIGHT ink), so a drawn
 * border is a long run of LIGHT pixels. A row y is a ruling line if it contains
 * a horizontal run of light pixels >= 0.5*W (a border spans the table); likewise
 * for columns. Returns 1 and fills rows[]/cols[] (caller frees) with the line
 * positions when a grid of at least 2x2 cells is found, else 0. This catches
 * explicitly bordered tables, which the flowing text-column splitter does not. */
static int detect_ruled_grid(const OcrImage *pg, int bg, int W, int H,
                             int **rows_out, int *nrows_out,
                             int **cols_out, int *ncols_out) {
    const double SPAN = 0.5;
    int *hr = malloc((size_t)H * sizeof(int));
    int *vc = malloc((size_t)W * sizeof(int));
    if (!hr || !vc) { free(hr); free(vc); return 0; }
    int nh = 0, nv = 0;
    /* horizontal: any row with a stroke run (pixels deviating from bg) spanning >= SPAN*W */
    for (int y = 0; y < H; y++) {
        int run = 0, best = 0;
        for (int x = 0; x < W; x++) {
            int v = ocr_image_get(pg,(size_t)x,(size_t)y);
            int stroke = (v - bg > 40) || (bg - v > 40);  /* deviates from background */
            run = stroke ? run+1 : 0;
            if (run > best) best = run;
        }
        if (best >= (int)(SPAN * W)) hr[nh++] = y;
    }
    /* vertical: any column with a stroke run spanning >= SPAN*H */
    for (int x = 0; x < W; x++) {
        int run = 0, best = 0;
        for (int y = 0; y < H; y++) {
            int v = ocr_image_get(pg,(size_t)x,(size_t)y);
            int stroke = (v - bg > 40) || (bg - v > 40);
            run = stroke ? run+1 : 0;
            if (run > best) best = run;
        }
        if (best >= (int)(SPAN * H)) vc[nv++] = x;
    }
    /* cluster nearby detections (a thick line spans several scanlines) into a
     * single line position, so each border counts once. */
    int *hr2 = malloc((size_t)(nh>0?nh:1) * sizeof(int));
    int *vc2 = malloc((size_t)(nv>0?nv:1) * sizeof(int));
    int nh2 = 0, nv2 = 0;
    for (int i = 0; i < nh; i++) {
        if (nh2 == 0 || hr[i] - hr2[nh2-1] > 8) hr2[nh2++] = hr[i];
        else hr2[nh2-1] = (hr2[nh2-1] + hr[i]) / 2;
    }
    for (int i = 0; i < nv; i++) {
        if (nv2 == 0 || vc[i] - vc2[nv2-1] > 8) vc2[nv2++] = vc[i];
        else vc2[nv2-1] = (vc2[nv2-1] + vc[i]) / 2;
    }
    free(hr); free(vc);
    if (nh2 >= 2 && nv2 >= 2) {
        *rows_out = hr2; *nrows_out = nh2;
        *cols_out = vc2; *ncols_out = nv2;
        return 1;
    }
    free(hr2); free(vc2);
    return 0;
}

/* Detect figure / image regions (#94 / #33). A figure is a large connected
 * component of pixels that deviate from the page background, whose interior is
 * SPARSE (low ink density) — i.e. a photo or diagram, not a block of text.
 * Runs a 4-connected flood fill over the deviation mask and keeps components
 * whose bounding box covers >= 8% of the page and whose filled-pixel density
 * is below DENSITY_MAX. Returns up to `maxb` boxes (x0,y0,x1,y1) via out args;
 * returns the count found (0 if none). */
static int detect_figure_regions(const OcrImage *pg, int bg, int W, int H,
                                 int maxb, int *bx0, int *by0, int *bx1, int *by1) {
    if (W < 1 || H < 1 || maxb < 1) return 0;
    const int DENSITY_MAX = 18;            /* % filled pixels inside the region */
    const long area_min = (long)W * H * 8 / 100;  /* >= 8% of page */
    uint8_t *vis = calloc((size_t)W * H, 1);
    if (!vis) return 0;
    /* deviation test (polarity-invariant): pixel far from background */
    #define DEV(x,y) ({\
        int _g = ocr_image_get(pg,(size_t)(x),(size_t)(y)); \
        int _d = _g - bg; if (_d < 0) _d = -_d; _d > 16; })
    /* seed/connectivity test: a pixel is part of a figure if IT deviates or any
     * of its 8 neighbours does. This dilates sparse speckle photos so that
     * isolated pixels (a real photograph is never a solid block) still form one
     * connected component. Density is counted with the strict DEV above. */
    #define SEED(x,y) ({\
        int _s = DEV(x,y); \
        if (!_s) { \
            for (int _dy=-1; _dy<=1 && !_s; _dy++) \
                for (int _dx=-1; _dx<=1; _dx++) { \
                    int _nx=(x)+(_dx), _ny=(y)+(_dy); \
                    if (_nx<0||_ny<0||_nx>=W||_ny>=H) continue; \
                    if (DEV(_nx,_ny)) { _s=1; break; } \
                } \
        } \
        _s; })
    int n = 0;
    int *stack = malloc((size_t)W * H * sizeof(int));
    if (!stack) { free(vis); return 0; }
    for (int sy = 0; sy < H && n < maxb; sy++) {
        for (int sx = 0; sx < W && n < maxb; sx++) {
            if (vis[(size_t)sy * W + sx]) continue;
            if (!SEED(sx, sy)) { vis[(size_t)sy * W + sx] = 1; continue; }
            /* flood fill this component */
            int s0 = sy, s1 = sy, l0 = sx, l1 = sx, cnt = 0, area = 0;
            int sp = 0; stack[sp++] = sy * W + sx; vis[sy*W+sx] = 1;
            while (sp > 0) {
                int idx = stack[--sp];
                int y = idx / W, x = idx % W;
                area++;
                if (x < l0) l0 = x; if (x > l1) l1 = x;
                if (y < s0) s0 = y; if (y > s1) s1 = y;
                static const int ddx[4] = {1,-1,0,0}, ddy[4] = {0,0,1,-1};
                for (int k = 0; k < 4; k++) {
                    int nx = x + ddx[k], ny = y + ddy[k];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int ni = ny * W + nx;
                    if (vis[ni]) continue;
                    if (SEED(nx, ny)) { vis[ni] = 1; stack[sp++] = ni; }
                }
            }
            /* count filled (deviating) pixels for density */
            for (int y = s0; y <= s1; y++)
                for (int x = l0; x <= l1; x++)
                    if (DEV(x, y)) cnt++;
            long reg_area = (long)(l1 - l0 + 1) * (s1 - s0 + 1);
            int density = (int)(100LL * cnt / reg_area);
            if (area >= area_min && density <= DENSITY_MAX) {
                bx0[n] = l0; by0[n] = s0; bx1[n] = l1; by1[n] = s1; n++;
            }
        }
    }
    #undef DEV
    free(vis); free(stack);
    return n;
}

int crnn_transcribe_page_json(CRNN *m, const OcrImage *page,
                              int strip, const char *charset,
                              const Lexicon *lex,
                              char **out_json) {
    if (out_json) *out_json = NULL;
    if (!m || !page || strip < 1 || !charset) return -1;

    int W = (int)ocr_image_width(page);
    int H = (int)ocr_image_height(page);
    if (W < 1 || H < 1) return -1;

    /* ---- adaptive background: median intensity of the page ----
     * Synthetic test pages paint a dark background; real scans/photos paint a
     * light one. Take the median pixel value as the background estimate. */
    int bg = 128;
    {
        size_t hist[256]; memset(hist, 0, sizeof hist);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                hist[ocr_image_get(page, (size_t)x, (size_t)y)]++;
        size_t half = (size_t)W * H / 2, acc = 0;
        for (int v = 0; v < 256; v++) { acc += hist[v]; if (acc >= half) { bg = v; break; } }
    }
    /* capture pre-normalization geometry for ruled-table detection (#32): the
     * grid lines must be detected on the ORIGINAL page (dark strokes on light
     * bg), not the polarity-inverted CRNN page. */
    int orig_bg = bg;
    int page_W = W, page_H = H;

    /* ---- blank-page guard (before polarity flip) ----
     * A truly empty scan is near-uniform: almost no pixel deviates from the
     * median background. Count deviations > 16 and bail out early with an
     * empty document. Done on the ORIGINAL page so it is polarity-invariant
     * (a uniform page inverts to another uniform page after the flip below). */
    {
        size_t dev = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int d = (int)ocr_image_get(page, (size_t)x, (size_t)y) - bg;
                if (d < 0) d = -d;
                if (d > 16) dev++;
            }
        if (dev * 200 < (size_t)W * H) {  /* < 0.5% deviating pixels -> blank */
            char *eb = malloc(64); size_t el = 0, ec = 64;
            if (eb) { if (ba_append_str(&eb, &el, &ec, "{\"blocks\":[]}") == 0) *out_json = eb;
                      else { free(eb); return -1; } }
            return 0;
        }
    }

    /* ---- deskew: straighten a slightly-rotated page before segmentation ----
     * Run on the ORIGINAL page (before polarity normalization) so the deskew
     * fill color and ink detection use the true background. A perfect
     * (unrotated) page deskews to itself (no-op), so this is safe to always
     * run on photo/scan input. */
    OcrImage *desk = deskew_page(page, bg);
    const OcrImage *pg = desk ? desk : page;
    W = (int)ocr_image_width(pg);
    H = (int)ocr_image_height(pg);

    /* ---- polarity normalization for the CRNN ----
     * The model is trained on dark background + light text. Real scans/photos
     * are the opposite (light background + dark text). Detect that from the
     * median and invert the deskewed page so the CRNN sees the polarity it
     * was trained on. Line images are extracted from this normalized page. */
    OcrImage *norm = NULL;
    if (bg > 127) {
        norm = ocr_image_create((size_t)W, (size_t)H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                ocr_image_set(norm, (size_t)x, (size_t)y,
                              (uint8_t)(255 - ocr_image_get(pg, (size_t)x, (size_t)y)));
        pg = norm;   /* CRNN sees the inverted (dark-bg) page */
        bg = 15;     /* background is now dark after inversion */
    }

    /* ---- noise-robust paired-ink mask ----
     * A pixel is "real ink" iff it deviates from the background AND has an
     * 8-connected ink neighbour. This rejects salt-and-pepper noise (isolated
     * pixels) exactly like the line detector, so both the row and column
     * projections below are noise-immune. Built FIRST so the optional
     * pre-processing below can operate on the true ink map. */
    uint8_t *pim = malloc((size_t)W * H);
    if (!pim) { if (norm) ocr_image_free(norm); if (desk) ocr_image_free(desk); return -1; }
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint8_t g = ocr_image_get(pg, (size_t)x, (size_t)y);
            int ink = 0;
            if (is_ink(g, bg)) {
                ink = 1;
                for (int dy = -1; dy <= 1 && ink; dy++) {
                    int ny = y + dy; if (ny < 0 || ny >= H) continue;
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx; if (nx < 0 || nx >= W) continue;
                        if (is_ink(ocr_image_get(pg, (size_t)nx, (size_t)ny), bg)) { ink = 2; break; }
                    }
                }
            }
            pim[(size_t)y * W + x] = (uint8_t)(ink == 2 ? 1 : 0);
        }
    }

    /* ---- optional adaptive pre-processing (Office-Lens grade) ----
     * Gated by env flags so default behaviour is unchanged:
     *   SAUVOLA=1  Sauvola adaptive re-binarization (uneven illumination)
     *   DESPECKLE=1 remove isolated ink specks from the ink mask
     *   CROP=1      auto-crop to the ink bounding box (drop page margins)
     * All operate on the true ink map `pim` / page `pg`, so polarity (which
     * can be inverted for the CRNN) is handled consistently. */
    {
        int do_sauvola = getenv("SAUVOLA") != NULL;
        int do_despeck = getenv("DESPECKLE") != NULL;
        int do_crop    = getenv("CROP") != NULL;

        /* (blank-page guard already handled before the polarity flip) */
        if (do_sauvola) {
            /* Sauvola assumes dark-text on light bg; our CRNN page is inverted
             * (light text on dark bg) when bg<127, so flip the ink test. */
            int light_text = (bg < 127);  /* 1 if ink is the BRIGHT pixels */
            uint8_t *th = ocr_sauvola_thresh(pg, 31, 0.30, 128.0);
            if (th) {
                for (int y = 0; y < H; y++)
                    for (int x = 0; x < W; x++) {
                        int ink = light_text ? (ocr_image_get(pg,(size_t)x,(size_t)y) > th[y*W+x])
                                            : (ocr_image_get(pg,(size_t)x,(size_t)y) < th[y*W+x]);
                        /* 8-connected requirement to stay noise-robust */
                        int conn = 0;
                        if (ink) for (int dy=-1; dy<=1 && !conn; dy++)
                            for (int dx=-1; dx<=1; dx++) {
                                if (!dx && !dy) continue;
                                int nx=x+dx, ny=y+dy;
                                if (nx<0||ny<0||nx>=W||ny>=H) continue;
                                int o = light_text ? (ocr_image_get(pg,(size_t)nx,(size_t)ny) > th[ny*W+nx])
                                                   : (ocr_image_get(pg,(size_t)nx,(size_t)ny) < th[ny*W+nx]);
                                if (o) { conn=1; break; }
                            }
                        pim[(size_t)y*W+x] = (uint8_t)(ink && conn ? 1 : 0);
                    }
                free(th);
            }
        }
        if (do_despeck) {
            /* open by 1: drop foreground specks with no 8-connected neighbour */
            uint8_t *np = malloc((size_t)W * H);
            if (np) {
                for (int y = 0; y < H; y++)
                    for (int x = 0; x < W; x++) {
                        if (!pim[(size_t)y*W+x]) { np[(size_t)y*W+x]=0; continue; }
                        int n=0;
                        for (int dy=-1; dy<=1 && !n; dy++)
                            for (int dx=-1; dx<=1; dx++) {
                                if (!dx && !dy) continue;
                                int nx=x+dx, ny=y+dy;
                                if (nx<0||ny<0||nx>=W||ny>=H) continue;
                                n += pim[(size_t)ny*W+nx];
                            }
                        np[(size_t)y*W+x] = (uint8_t)(n?1:0);
                    }
                memcpy(pim, np, (size_t)W * H);
                free(np);
            }
        }
        if (do_crop) {
            long x0=(long)W, y0=(long)H, x1=-1, y1=-1;
            for (int y=0; y<H; y++)
                for (int x=0; x<W; x++)
                    if (pim[(size_t)y*W+x]) {
                        if (x<x0)x0=x; if (x>x1)x1=x; if (y<y0)y0=y; if (y>y1)y1=y;
                    }
            if (x1>=x0 && y1>=y0) {
                int pad=8; x0-=pad; if(x0<0)x0=0; y0-=pad; if(y0<0)y0=0;
                x1+=pad; if(x1>=W)x1=W-1; y1+=pad; if(y1>=H)y1=H-1;
                size_t cw=(size_t)(x1-x0+1), ch=(size_t)(y1-y0+1);
                OcrImage *cr = ocr_image_create(cw, ch);
                if (cr) {
                    const uint8_t *px = ocr_image_pixels(pg);
                    for (size_t y=0; y<ch; y++)
                        for (size_t x=0; x<cw; x++)
                            ocr_image_set(cr, x, y, px[((size_t)y0+y)*W+((size_t)x0+x)]);
                    if (desk) ocr_image_free(desk); desk = cr; pg = cr;
                    W = (int)cw; H = (int)ch;
                    /* rebuild pim for the cropped page via the same ink rule */
                    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
                        uint8_t g = ocr_image_get(pg, (size_t)x, (size_t)y);
                        int ink = 0;
                        if (is_ink(g, bg)) {
                            ink = 1;
                            for (int dy = -1; dy <= 1 && ink; dy++) { int ny = y + dy; if (ny < 0 || ny >= H) continue;
                                for (int dx = -1; dx <= 1; dx++) { if (!dx && !dy) continue; int nx = x + dx; if (nx < 0 || nx >= W) continue;
                                    if (is_ink(ocr_image_get(pg, (size_t)nx, (size_t)ny), bg)) { ink = 2; break; } } }
                        }
                        pim[(size_t)y * W + x] = (uint8_t)(ink == 2 ? 1 : 0);
                    }
                }
            }
        }
    }

    /* ---- row projection: detect text lines ----
     * A row is ink if it contains at least one real-ink pixel. Consecutive
     * ink rows form a line band; its center is the line position. */
    char *row_ink = malloc((size_t)H);
    if (!row_ink) { free(pim); if (norm) ocr_image_free(norm); if (desk) ocr_image_free(desk); return -1; }
    for (int y = 0; y < H; y++) {
        int cnt = 0;
        for (int x = 0; x < W; x++) cnt += pim[(size_t)y * W + x];
        row_ink[y] = (char)(cnt >= 1);
    }

    /* ---- Pass 1: collect line bands (top/bottom/center) ---- */
    int nlines = 0;
    int *ly0 = malloc((size_t)H * sizeof(int));
    int *ly1 = malloc((size_t)H * sizeof(int));
    int *lcy = malloc((size_t)H * sizeof(int));
    if (!ly0 || !ly1 || !lcy) {
        free(pim); free(row_ink); free(ly0); free(ly1); free(lcy);
        if (norm) ocr_image_free(norm); if (desk) ocr_image_free(desk); return -1;
    }
    int y = 0;
    while (y < H) {
        while (y < H && !row_ink[y]) y++;
        if (y >= H) break;
        int y0 = y;
        while (y < H && row_ink[y]) y++;
        int y1 = y;
        ly0[nlines] = y0; ly1[nlines] = y1;
        lcy[nlines] = (y0 + y1) / 2;
        nlines++;
    }
    /* merge bands separated by tiny gaps: small glyphs (apostrophes, quotes,
     * dots of i/j, accents) can leave a 1-2 row blank inside a text line and
     * split one line into two bands. A real inter-line gap is a significant
     * fraction of the line height; a 1-2 px gap is intra-line noise. */
    if (nlines > 1) {
        int hsum = 0;
        for (int li = 0; li < nlines; li++) hsum += ly1[li] - ly0[li];
        int avgh = hsum / nlines;
        int maxgap = avgh / 4; if (maxgap < 2) maxgap = 2;
        int w2 = 0;
        for (int li = 0; li < nlines; li++) {
            if (w2 > 0 && ly0[li] - ly1[w2 - 1] <= maxgap) {
                ly1[w2 - 1] = ly1[li];               /* merge into previous */
                lcy[w2 - 1] = (ly0[w2 - 1] + ly1[w2 - 1]) / 2;
            } else {
                ly0[w2] = ly0[li]; ly1[w2] = ly1[li]; lcy[w2] = lcy[li]; w2++;
            }
        }
        nlines = w2;
    }

    /* ---- column detection via row-counting vertical projection ----
     * A 2-column page has two lines per y-row (one per column) at the SAME y,
     * so the horizontal line scan merges them into one band. To recover
     * columns we project DOWN the columns but count, per x, how many LINE
     * BANDS have ink there (not pixels). A true inter-column gutter has ZERO
     * lines with ink at that x; a within-column letter/word gap has several
     * lines (different rows have letters at different x). This rejects letter
     * gaps automatically. A wide run where line-ink is below a small threshold
     * (also robust to slight slant) is a column boundary. Reading order:
     * columns left-to-right, lines top-to-bottom within each. */
    int *xrow = calloc((size_t)W, sizeof(int));  /* # lines with ink at col x */
    if (!xrow) {
        free(pim); free(row_ink); free(ly0); free(ly1); free(lcy);
        if (norm) ocr_image_free(norm); if (desk) ocr_image_free(desk); return -1;
    }
    for (int li = 0; li < nlines; li++)
        for (int xx = 0; xx < W; xx++) {
            int has = 0;
            for (int yy = ly0[li]; yy < ly1[li] && !has; yy++)
                if (pim[(size_t)yy * W + xx]) has = 1;
            xrow[xx] += has;
        }
    int min_gutter = W / 30 < 25 ? 25 : W / 30;       /* a real column gap */
    /* restrict to ink bounding box so empty margins don't invent columns */
    int xmin = 0, xmax = W - 1;
    for (int xx = 0; xx < W; xx++) if (xrow[xx] > 0) { xmin = xx; break; }
    for (int xx = W - 1; xx >= 0; xx--) if (xrow[xx] > 0) { xmax = xx; break; }
    int *col_edge = malloc(((size_t)W + 2) * sizeof(int));
    int ncols = 0;
    col_edge[ncols++] = xmin;

    /* ---- recursive widest-gutter column split ----
     * True N-column pages: repeatedly split any region [lo,hi) at its WIDEST
     * empty gutter (a run where no line has ink, >= min_gutter wide, fully
     * interior). Each split pushes a new edge and recurses into both halves.
     * This yields 2 columns, 3 columns, etc., WITHOUT the over-segmentation
     * the old iterative "mark every gap" approach caused: we only ever split
     * at the single strongest gutter within a region, and never on gaps that
     * aren't clearly interior. A region with no qualifying gutter stays whole
     * (1 column). */
    {
        int *stack_lo = malloc(((size_t)W + 2) * sizeof(int));
        int *stack_hi = malloc(((size_t)W + 2) * sizeof(int));
        if (!stack_lo || !stack_hi) { free(stack_lo); free(stack_hi);
            free(xrow); free(pim); free(col_edge);
            free(row_ink); free(ly0); free(ly1); free(lcy);
            if (norm) ocr_image_free(norm); if (desk) ocr_image_free(desk); return -1; }
        int sp = 0;
        stack_lo[sp] = xmin; stack_hi[sp] = xmax; sp++;
        while (sp > 0) {
            sp--;
            int lo = stack_lo[sp], hi = stack_hi[sp];
            if (hi - lo < 2 * min_gutter) continue;  /* too narrow to hold 2 cols */
            /* find widest empty gutter inside [lo,hi) */
            int best_w = 0, best_g0 = -1;
            int xx = lo;
            while (xx <= hi) {
                if (xrow[xx] != 0) { xx++; continue; }
                int g0 = xx;
                while (xx <= hi && xrow[xx] == 0) xx++;
                int gw = xx - g0;
                if (gw > best_w) { best_w = gw; best_g0 = g0; }
            }
            if (best_w >= min_gutter && best_g0 > lo + min_gutter
                && best_g0 < hi - min_gutter) {
                /* A genuine inter-column gutter is empty (xrow==0) for its full
                 * width AND is BRACKETED by ink on both sides, AND yields two
                 * real columns (each at least ~W/8 wide). The width guard rejects
                 * narrow intra-column word/block gaps, which otherwise read as
                 * spurious column separators and over-split 2-col -> 4-col. */
                int min_col_width = W / 8 < 100 ? 100 : W / 8;
                if (best_g0 - 1 >= lo && best_g0 + best_w <= hi
                    && xrow[best_g0 - 1] != 0 && xrow[best_g0 + best_w] != 0
                    && (best_g0 - lo) >= min_col_width
                    && (hi - best_g0) >= min_col_width) {
                    /* record the edge, recurse into left [lo,best_g0) and right [best_g0,hi) */
                    col_edge[ncols++] = best_g0;
                    stack_lo[sp] = lo;          stack_hi[sp] = best_g0 - 1; sp++;
                    stack_lo[sp] = best_g0 + 1; stack_hi[sp] = hi;          sp++;
                }
            }
        }
        free(stack_lo); free(stack_hi);
    }
    col_edge[ncols++] = xmax + 1;
    /* sort edges ascending (pushes happened in DFS order, not sorted) */
    for (int a = 0; a < ncols - 1; a++)
        for (int b = a + 1; b < ncols; b++)
            if (col_edge[b] < col_edge[a]) { int t = col_edge[a]; col_edge[a] = col_edge[b]; col_edge[b] = t; }
    int ncol = ncols - 1;
    if (ncol < 1) ncol = 1;
    free(xrow);
    free(pim);

    /* ---- Pass 2: recognize each (row, column) cell ----
     * Each row band is cropped into per-column segments and recognized. The
     * result is a row-major grid cells[r*ncol + c] preserving spatial layout. */
    char **line_text = malloc((size_t)nlines * (size_t)ncol * sizeof(char *));
    int  *line_conf = malloc((size_t)nlines * (size_t)ncol * sizeof(int));
    int  *line_x0 = malloc((size_t)nlines * (size_t)ncol * sizeof(int));
    int  *line_y0 = malloc((size_t)nlines * (size_t)ncol * sizeof(int));
    int  *line_x1 = malloc((size_t)nlines * (size_t)ncol * sizeof(int));
    int  *line_y1 = malloc((size_t)nlines * (size_t)ncol * sizeof(int));
    char **line_ccstr = malloc((size_t)nlines * (size_t)ncol * sizeof(char *));
    if (!line_text || !line_conf || !line_x0 || !line_y0 || !line_x1 || !line_y1 || !line_ccstr) {
        free(line_text); free(line_conf); free(line_x0); free(line_y0); free(line_x1); free(line_y1); free(line_ccstr);
        free(row_ink); free(ly0); free(ly1); free(lcy); free(col_edge);
        if (norm) ocr_image_free(norm); if (desk) ocr_image_free(desk); return -1;
    }
    for (int li = 0; li < nlines; li++) {
        for (int c = 0; c < ncol; c++) {
            line_ccstr[li * ncol + c] = NULL;
        }
    }
    for (int li = 0; li < nlines; li++) {
        for (int c = 0; c < ncol; c++) {
            int cx0 = col_edge[c], cx1 = col_edge[c + 1];
            line_conf[li * ncol + c] = 100;
            line_x0[li * ncol + c] = cx0; line_x1[li * ncol + c] = cx1;
            line_y0[li * ncol + c] = (ly0 && li < nlines) ? ly0[li] : 0;
            line_y1[li * ncol + c] = (ly1 && li < nlines) ? ly1[li] : H - 1;
            if (cx1 <= cx0) { line_text[li * ncol + c] = malloc(1); line_text[li * ncol + c][0] = '\0'; continue; }
            /* crop the column segment of this row band into a line image */
            OcrImage *seg = ocr_image_create((size_t)(cx1 - cx0), (size_t)strip);
            if (!seg) { line_text[li * ncol + c] = malloc(1); line_text[li * ncol + c][0] = '\0'; continue; }
            int top = lcy[li] - strip / 2;
            for (int y = 0; y < strip; y++) {
                int sy = top + y; if (sy < 0) sy = 0; if (sy >= H) sy = H - 1;
                for (int x = 0; x < cx1 - cx0; x++) {
                    int sx = cx0 + x; if (sx < 0) sx = 0; if (sx >= W) sx = W - 1;
                    ocr_image_set(seg, (size_t)x, (size_t)y, ocr_image_get(pg, (size_t)sx, (size_t)sy));
                }
            }
            char *pred = malloc(512);
            if (!pred) pred = (char *)"";
            int conf = 100;
            int cconf[512];
            int nch = crnn_recognize_scored_chars(m, seg, charset, pred, 512, &conf, cconf, 512);
            /* build a compact cconf array string "[a,b,c,...]" (nch entries) */
            char *cs = malloc((size_t)nch * 4 + 8);
            if (cs) { char *p = cs; *p++='[';
                for (int i=0;i<nch;i++){ if(i) *p++=','; p+=snprintf(p,4,"%d",cconf[i]); }
                *p++=']'; *p='\0'; line_ccstr[li*ncol+c]=cs; }
            /* optional lexicon beam post-correction (#45): correct whole words
             * against a loaded wordlist, preserving spacing/punctuation. */
            if (lex && pred[0]) {
                char corr[512]; int ci=0; int slen=(int)strlen(pred);
                int i=0;
                while (i<slen && ci<511) {
                    if (pred[i]==' '||pred[i]=='\t'){ corr[ci++]=pred[i]; i++; continue; }
                    int j=i; while (j<slen && pred[j]!=' ' && pred[j]!='\t') j++;
                    int wlen=j-i;
                    if (wlen>0) {
                        char w[128]; int k=0;
                        for (;i<j && k<127;) w[k++]=pred[i++]; w[k]='\0';
                        int dist=0;
                        int widx = lex_correct((Lexicon*)lex, w, 2, &dist);
                        if (widx >= 0) {
                            const char *fixed = lex_word((Lexicon*)lex, widx);
                            if (fixed && dist>0 && dist<=2) {
                                for (int q=0; fixed[q] && ci<511; q++) corr[ci++]=fixed[q];
                            } else {
                                for (int q=0; w[q] && ci<511; q++) corr[ci++]=w[q];
                            }
                        } else {
                            for (int q=0; w[q] && ci<511; q++) corr[ci++]=w[q];
                        }
                    }
                }
                corr[ci]='\0';
                if (ci>0) { char *np=strdup(corr); if (np) { free(pred); pred=np; } }
            }
            /* Unicode NFC composition (#96): precompose combining marks so the
             * OCR text is canonical (e.g. e + U+0301 -> é). */
            if (pred[0]) {
                char nfc[512];
                if (wubuocr_nfc_latin(pred, nfc, sizeof nfc) > 0 && nfc[0]) {
                    char *np = strdup(nfc);
                    if (np) { free(pred); pred = np; }
                }
            }
            ocr_image_free(seg);
            line_text[li * ncol + c] = pred;
            line_conf[li * ncol + c] = conf;
        }
    }

    /* ---- build docmodel JSON ----
     * Single column: emit plain paragraphs (clean output) in row order.
     * Multi-column: emit a table with rows = line bands, cols = columns, so
     * the spatial layout (columns read left-to-right, lines top-to-bottom)
     * survives the docx/odt round-trip. cells are row-major: cells[r*ncol+c]. */
    char *buf = malloc(64); size_t len = 0, cap = 64;
    if (!buf) {
        free(row_ink); free(ly0); free(ly1); free(lcy); free(col_edge);
        free(line_text);
        if (norm) ocr_image_free(norm); if (desk) ocr_image_free(desk); return -1;
    }
    /* script tally for doc-level language auto-detect (#46): majority vote */
    struct { const char *code; int n; } ltab[10] = {
        {"en",0},{"ru",0},{"el",0},{"he",0},{"ar",0},
        {"hi",0},{"ja",0},{"zh",0},{"ko",0},{"th",0} };
    if (ba_append_str(&buf, &len, &cap, "{\"blocks\":[") != 0) goto oom;
    int emitted = (nlines > 0 || ncol > 1);   /* tracks whether any block has been written */

    if (ncol <= 1) {
        int first = 1;
        for (int li = 0; li < nlines; li++) {
            if (!first) { if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom; }
            first = 0;
            int is_math = detect_math_line(line_text[li]);
            if (is_math) {
                /* math/equation block (#48): raw text + latex placeholder. The
                 * `latex` field is filled by a downstream LaTeX model; here we
                 * emit the recognized text verbatim so it round-trips as a string. */
                if (ba_append_str(&buf, &len, &cap, "{\"kind\":\"math\",\"text\":\"") != 0) goto oom;
                if (ba_append_json_escaped(&buf, &len, &cap, line_text[li]) != 0) goto oom;
                { char tmp[160]; snprintf(tmp, sizeof tmp,
                    "\",\"conf\":%d,\"bbox\":[%d,%d,%d,%d],\"latex\":\"%s\"}",
                    line_conf[li], line_x0[li], line_y0[li], line_x1[li], line_y1[li],
                    line_text[li]);   /* placeholder: raw text as latex stub */
                  if (ba_append_str(&buf,&len,&cap,tmp)!=0) goto oom; }
                continue;
            }
            if (ba_append_str(&buf, &len, &cap, "{\"kind\":\"paragraph\",\"text\":\"") != 0) goto oom;
            if (ba_append_json_escaped(&buf, &len, &cap, line_text[li]) != 0) goto oom;
            { char tmp[64]; snprintf(tmp, sizeof tmp, "\",\"conf\":%d,\"bbox\":[%d,%d,%d,%d]",
                line_conf[li], line_x0[li], line_y0[li], line_x1[li], line_y1[li]);
              if (ba_append_str(&buf,&len,&cap,tmp)!=0) goto oom; }
            if (line_ccstr[li]) {
                if (ba_append_str(&buf,&len,&cap,",\"cconf\":") != 0) goto oom;
                if (ba_append_str(&buf,&len,&cap,line_ccstr[li]) != 0) goto oom;
            }
            /* reading-order (#31): explicit order index + header detection.
             * A header is a short (<=6 word) high-confidence line near the top
             * of the page (top third). Order is the emission index (row-major). */
            {
                int order = li;
                int is_head = (line_y0[li] < H/3) ? 1 : 0;
                int words = 1; for (const char *p=line_text[li]; *p; p++) if (*p==' ') words++;
                if (words > 6 || line_conf[li] < 80) is_head = 0;
                char tmp[64];
                snprintf(tmp, sizeof tmp, ",\"order\":%d,\"head\":%d", order, is_head);
                if (ba_append_str(&buf,&len,&cap,tmp) != 0) goto oom;
            }
            /* language/script auto-detect (#46/#94): tag each block's script */
            {
                const char *lg = detect_script(line_text[li]);
                char tmp[32]; snprintf(tmp, sizeof tmp, ",\"lang\":\"%s\"", lg);
                if (ba_append_str(&buf,&len,&cap,tmp) != 0) goto oom;
                for (int t=0;t<10;t++) if (strcmp(ltab[t].code,lg)==0){ ltab[t].n++; break; }
            }
            if (ba_append_str(&buf,&len,&cap,"}") != 0) goto oom;
        }
    } else {
        if (ba_append_str(&buf, &len, &cap, "{\"kind\":\"table\",\"rows\":") != 0) goto oom;
        { char tmp[32]; snprintf(tmp, sizeof tmp, "%d", nlines); if (ba_append_str(&buf,&len,&cap,tmp)!=0) goto oom; }
        if (ba_append_str(&buf, &len, &cap, ",\"cols\":") != 0) goto oom;
        { char tmp[32]; snprintf(tmp, sizeof tmp, "%d", ncol); if (ba_append_str(&buf,&len,&cap,tmp)!=0) goto oom; }
        if (ba_append_str(&buf, &len, &cap, ",\"cells\":[") != 0) goto oom;
        for (int r = 0; r < nlines; r++) {
            if (r > 0) { if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom; }
            if (ba_append_str(&buf, &len, &cap, "[") != 0) goto oom;
            for (int c = 0; c < ncol; c++) {
                if (c > 0) { if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom; }
                if (ba_append_str(&buf, &len, &cap, "\"") != 0) goto oom;
                if (ba_append_json_escaped(&buf, &len, &cap, line_text[r * ncol + c]) != 0) goto oom;
                if (ba_append_str(&buf, &len, &cap, "\"") != 0) goto oom;
            }
            if (ba_append_str(&buf, &len, &cap, "]") != 0) goto oom;
        }
        if (ba_append_str(&buf, &len, &cap, "],\"conf\":[") != 0) goto oom;
        for (int r = 0; r < nlines; r++) {
            if (r > 0) { if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom; }
            if (ba_append_str(&buf, &len, &cap, "[") != 0) goto oom;
            for (int c = 0; c < ncol; c++) {
                if (c > 0) { if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom; }
                { char tmp[32]; snprintf(tmp, sizeof tmp, "%d", line_conf[r * ncol + c]); if (ba_append_str(&buf,&len,&cap,tmp)!=0) goto oom; }
            }
            if (ba_append_str(&buf, &len, &cap, "]") != 0) goto oom;
        }
        if (ba_append_str(&buf, &len, &cap, "\"]") != 0) goto oom;  /* close conf array only */
        /* per-cell char-confidence arrays, parallel to cells (ignored by wubuconv) */
        if (ba_append_str(&buf, &len, &cap, ",\"cconf\":[") != 0) goto oom;
        for (int r = 0; r < nlines; r++) {
            if (r > 0) { if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom; }
            if (ba_append_str(&buf, &len, &cap, "[") != 0) goto oom;
            for (int c = 0; c < ncol; c++) {
                if (c > 0) { if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom; }
                if (line_ccstr[r * ncol + c]) { if (ba_append_str(&buf,&len,&cap,line_ccstr[r*ncol+c])!=0) goto oom; }
                else { if (ba_append_str(&buf,&len,&cap,"[]")!=0) goto oom; }
            }
            if (ba_append_str(&buf, &len, &cap, "]") != 0) goto oom;
        }
        if (ba_append_str(&buf, &len, &cap, "]") != 0) goto oom;  /* close cconf array */
        /* per-cell bboxes, parallel to cells (ignored by wubuconv) */
        if (ba_append_str(&buf, &len, &cap, ",\"cellbox\":[") != 0) goto oom;
        for (int r = 0; r < nlines; r++) {
            if (r > 0) { if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom; }
            if (ba_append_str(&buf, &len, &cap, "[") != 0) goto oom;
            for (int c = 0; c < ncol; c++) {
                if (c > 0) { if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom; }
                char tmp[64];
                snprintf(tmp, sizeof tmp, "[%d,%d,%d,%d]",
                         line_x0[r*ncol+c], line_y0[r*ncol+c],
                         line_x1[r*ncol+c], line_y1[r*ncol+c]);
                if (ba_append_str(&buf,&len,&cap,tmp)!=0) goto oom;
            }
            if (ba_append_str(&buf, &len, &cap, "]") != 0) goto oom;
        }
        if (ba_append_str(&buf, &len, &cap, "]}") != 0) goto oom;  /* close cellbox + table obj */
    }

    /* ---- ruled-table detection (#32): if explicit borders form a grid, emit a
     * tagged table block with cell coordinates + recognized cell text. This
     * complements the flowing text-column splitter above. */
    {
        int *grows=NULL, *gcols=NULL, gnrows=0, gncols=0;
        if (detect_ruled_grid(page, orig_bg, page_W, page_H, &grows, &gnrows, &gcols, &gncols)) {
            /* scale grid lines from original-page coords into normalized pg coords */
            double sx = (page_W>0) ? (double)W / page_W : 1.0;
            double sy = (page_H>0) ? (double)H / page_H : 1.0;
            for (int i=0;i<gnrows;i++) grows[i] = (int)(grows[i]*sy);
            for (int i=0;i<gncols;i++) gcols[i] = (int)(gcols[i]*sx);
            if (emitted) { if (ba_append_str(&buf,&len,&cap,",")!=0) goto oom; }
            emitted = 1;
            if (ba_append_str(&buf,&len,&cap,
                  "{\"kind\":\"table\",\"rule\":\"ruled\",\"rows\":") != 0) goto oom;
            { char tmp[32]; snprintf(tmp,sizeof tmp,"%d",gnrows-1); if(ba_append_str(&buf,&len,&cap,tmp)!=0) goto oom; }
            if (ba_append_str(&buf,&len,&cap,",\"cols\":") != 0) goto oom;
            { char tmp[32]; snprintf(tmp,sizeof tmp,"%d",gncols-1); if(ba_append_str(&buf,&len,&cap,tmp)!=0) goto oom; }
            if (ba_append_str(&buf,&len,&cap,",\"cells\":[") != 0) goto oom;
            for (int r=0; r<gnrows-1; r++) {
                if (r>0){ if(ba_append_str(&buf,&len,&cap,",")!=0) goto oom; }
                if (ba_append_str(&buf,&len,&cap,"[")!=0) goto oom;
                for (int c=0; c<gncols-1; c++) {
                    if (c>0){ if(ba_append_str(&buf,&len,&cap,",")!=0) goto oom; }
                    int cx0=gcols[c], cx1=gcols[c+1], cy0=grows[r], cy1=grows[r+1];
                    if (ba_append_str(&buf,&len,&cap,"\"")!=0) goto oom;
                    int pad=2;
                    int ix0=cx0+pad, ix1=cx1-pad, iy0=cy0+pad, iy1=cy1-pad;
                    char *ctext="";
                    if (ix1>ix0 && iy1>iy0 && m) {
                        int cw=ix1-ix0, ch=iy1-iy0;
                        OcrImage *cell=ocr_image_create((size_t)cw,(size_t)ch);
                        if (cell) {
                            for (int yy=0; yy<ch; yy++) for (int xx=0; xx<cw; xx++) {
                                int sx=ix0+xx, sy=iy0+yy;
                                uint8_t v = (sx>=0&&sx<W&&sy>=0&&sy<H) ? ocr_image_get(pg,(size_t)sx,(size_t)sy) : (uint8_t)bg;
                                ocr_image_set(cell,(size_t)xx,(size_t)yy,v);
                            }
                            char *pred=malloc(256); int conf=100; int cc[256];
                            if (pred) {
                                crnn_recognize_scored_chars(m, cell, charset, pred, 256, &conf, cc, 256);
                                char nf[256]; if (wubuocr_nfc_latin(pred,nf,sizeof nf)>0 && nf[0]) { free(pred); pred=strdup(nf); }
                                ctext=pred;
                            }
                            ocr_image_free(cell);
                        }
                    }
                    if (ba_append_json_escaped(&buf,&len,&cap,ctext)!=0) goto oom;
                    if (ba_append_str(&buf,&len,&cap,"\"")!=0) goto oom;
                    if (ctext && ctext[0]) free((char*)ctext);
                }
                if (ba_append_str(&buf,&len,&cap,"]")!=0) goto oom;
            }
            if (ba_append_str(&buf,&len,&cap,"],\"cellbox\":[") != 0) goto oom;
            for (int r=0; r<gnrows-1; r++) {
                if (r>0){ if(ba_append_str(&buf,&len,&cap,",")!=0) goto oom; }
                if (ba_append_str(&buf,&len,&cap,"[")!=0) goto oom;
                for (int c=0; c<gncols-1; c++) {
                    if (c>0){ if(ba_append_str(&buf,&len,&cap,",")!=0) goto oom; }
                    char tmp[64]; snprintf(tmp,sizeof tmp,"[%d,%d,%d,%d]",
                        gcols[c],grows[r],gcols[c+1],grows[r+1]);
                    if(ba_append_str(&buf,&len,&cap,tmp)!=0) goto oom;
                }
                if (ba_append_str(&buf,&len,&cap,"]")!=0) goto oom;
            }
            if (ba_append_str(&buf,&len,&cap,"]}")!=0) goto oom;
            free(grows); free(gcols);
        }
    }

    /* ---- figure / image region detection (#94 / #33): emit a tagged `figure`
     * block with the bounding box and an alt-text placeholder. Coordinates are
     * in the normalized pg space (consistent with paragraph/table bboxes). The
     * alt text is a structured description; a downstream vision model can fill
     * it with a real caption. */
    {
        int fx0[16], fy0[16], fx1[16], fy1[16];
        int nfig = detect_figure_regions(pg, bg, W, H, 16, fx0, fy0, fx1, fy1);
        for (int fi = 0; fi < nfig; fi++) {
            if (emitted || fi > 0) {
                if (ba_append_str(&buf, &len, &cap, ",") != 0) goto oom;
            }
            emitted = 1;
            char tmp[128];
            snprintf(tmp, sizeof tmp,
                "{\"kind\":\"figure\",\"box\":[%d,%d,%d,%d],\"alt\":\"figure region %dx%d at (%d,%d)\"}",
                fx0[fi], fy0[fi], fx1[fi], fy1[fi],
                fx1[fi]-fx0[fi]+1, fy1[fi]-fy0[fi]+1, fx0[fi], fy0[fi]);
            if (ba_append_str(&buf, &len, &cap, tmp) != 0) goto oom;
        }
    }

    if (ba_append_str(&buf, &len, &cap, "]") != 0) goto oom;

    /* doc-level language auto-detect (#46): majority script vote */
    {
        const char *best = "en"; int bestn = -1;
        for (int t=0;t<10;t++) if (ltab[t].n > bestn){ bestn = ltab[t].n; best = ltab[t].code; }
        char tmp[32]; snprintf(tmp, sizeof tmp, ",\"lang\":\"%s\"", best);
        if (ba_append_str(&buf, &len, &cap, tmp) != 0) goto oom;
    }

    if (ba_append_str(&buf, &len, &cap, "}") != 0) goto oom;

    /* success */
    for (int i = 0; i < nlines * ncol; i++) { free(line_text[i]); free(line_ccstr[i]); }
    free(line_text); free(line_conf); free(line_ccstr);
    free(line_x0); free(line_y0); free(line_x1); free(line_y1);
    free(ly0); free(ly1); free(lcy); free(col_edge); free(row_ink);
    if (desk) ocr_image_free(desk);
    if (norm) ocr_image_free(norm);
    if (out_json) *out_json = buf;
    return 0;

oom:
    free(buf);
    for (int i = 0; i < nlines * ncol; i++) { free(line_text[i]); free(line_ccstr[i]); }
    free(line_text); free(line_conf); free(line_ccstr);
    free(line_x0); free(line_y0); free(line_x1); free(line_y1);
    free(ly0); free(ly1); free(lcy); free(col_edge); free(row_ink);
    if (desk) ocr_image_free(desk);
    if (norm) ocr_image_free(norm);
    return -1;
}
