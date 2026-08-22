/* crnn_transcribe_pre.c -- page preprocessing + text helpers for the
 * transcription pipeline: NFC latin precompose, script/math detection,
 * growable byte buffer with JSON escaping, ink masks, rotation/deskew,
 * ruled-grid and figure-region detection. Split from crnn_transcribe.c. */
#include "crnn_transcribe_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int is_ink(uint8_t g, int bg) { return g > bg ? (g - bg) > INK_MARGIN : (bg - g) > INK_MARGIN; }

/* precompose(base, mark): return the Unicode codepoint of base LETTER followed
 * by combining mark `mark` where a precomposed form exists, else 0. base must
 * be an ASCII letter; the returned codepoint matches base's case. Covers the
 * common Latin letters/marks. */
unsigned int precompose(unsigned int base, unsigned int m) {
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
const char *detect_script(const char *s) {
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
int detect_math_line(const char *s) {
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
int ba_append(char **buf, size_t *len, size_t *cap, const char *s, size_t n) {
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

int ba_append_str(char **buf, size_t *len, size_t *cap, const char *s) {
    return ba_append(buf, len, cap, s, strlen(s));
}

/* Append a JSON-escaped copy of `s`. */
int ba_append_json_escaped(char **buf, size_t *len, size_t *cap, const char *s) {
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
int row_paired_ink(const OcrImage *page, int y, int bg, int W) {
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
OcrImage *rotate_img(const OcrImage *src, double deg, uint8_t fill) {
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
OcrImage *deskew_page(const OcrImage *src, int bg) {
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
int detect_ruled_grid(const OcrImage *pg, int bg, int W, int H,
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
int detect_figure_regions(const OcrImage *pg, int bg, int W, int H,
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

