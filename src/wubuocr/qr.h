/* qr.h -- self-contained QR-code codec (encoder + decoder), clean C11, no deps.
 * Byte-mode QR, ECC level M, versions 1..7. See qr.c. */
#ifndef WUBUOCR_QR_H
#define WUBUOCR_QR_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Encode `text` into a QR module matrix. On success returns the QR version
 * (1..7), sets *out to a malloc'd row-major size*size byte array (0/1), and
 * *out_size to the module dimension. Caller frees *out. Returns -1 if text is
 * too long for versions 1..7. */
int qr_encode(const char *text, unsigned char **out, int *out_size);

/* Decode a QR module matrix (row-major size*size, 0/1) into NUL-terminated text.
 * Returns 0 on success (byte-mode QR), -1 otherwise. */
int qr_decode_matrix(const unsigned char *matrix, int size, char *out, int outcap);

/* Detect and decode QR codes in a grayscale page. `pix` is row-major W*H (0=black
 * ink .. 255=white), `bg` the background level. Up to `maxn` decoded texts are
 * written to the out-params (caller provides text[] arrays of capacity txtcap).
 * Returns the number of QR codes found/decoded (<= maxn). Coordinates of each
 * QR's bounding box are written to x0/y0/x1/y1 (may be NULL). */
int qr_detect_blocks(const unsigned char *pix, int W, int H, int bg,
                     int maxn, char text[][256], int txtcap,
                     int *x0, int *y0, int *x1, int *y1);

#ifdef __cplusplus
}
#endif
#endif
