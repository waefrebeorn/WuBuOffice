#include "wubuqr.h"
#include <stdlib.h>
#include <string.h>

/* The QR codec lives in wubuocr; expose it under the wubuqr module name. */
#include "../wubuocr/qr.h"

char *wubuqr_render_ascii(const char *text, int *out_size){
    if (!text) return NULL;
    unsigned char *mat = NULL;
    int size = 0;
    int v = qr_encode(text, &mat, &size);
    if (v < 0) return NULL;
    /* rows: '#'*size + '\n'; final NUL */
    size_t rowlen = (size_t)size + 1;
    size_t total = rowlen * (size_t)size + 1;
    char *out = (char*)malloc(total);
    if (!out){ free(mat); return NULL; }
    for (int r = 0; r < size; r++){
        for (int c = 0; c < size; c++)
            out[(size_t)r*rowlen + c] = mat[(size_t)r*size + c] ? '#' : '.';
        out[(size_t)r*rowlen + size] = '\n';
    }
    out[total - 1] = '\0';
    free(mat);
    if (out_size) *out_size = size;
    return out;
}
