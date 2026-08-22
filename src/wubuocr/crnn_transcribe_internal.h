/* crnn_transcribe_internal.h -- shared helpers for the page-transcription
 * pipeline. Internal to src/wubuocr. C11. */
#ifndef CRNN_TRANSCRIBE_INTERNAL_H
#define CRNN_TRANSCRIBE_INTERNAL_H

#include "crnn_transcribe.h"
#include "binarize.h"
#include "lexicon.h"
#include <stdint.h>
#include <stddef.h>

#define INK_MARGIN 40

int is_ink(uint8_t g, int bg);
unsigned int precompose(unsigned int base, unsigned int mark);
const char *detect_script(const char *s);
int detect_math_line(const char *s);
int ba_append(char **buf, size_t *len, size_t *cap, const char *s, size_t n);
int ba_append_str(char **buf, size_t *len, size_t *cap, const char *s);
int ba_append_json_escaped(char **buf, size_t *len, size_t *cap, const char *s);
int row_paired_ink(const OcrImage *page, int y, int bg, int W);
OcrImage *rotate_img(const OcrImage *src, double deg, uint8_t fill);
OcrImage *deskew_page(const OcrImage *src, int bg);
int detect_ruled_grid(const OcrImage *pg, int bg, int W, int H,
                      int **rows_out, int *nrows_out,
                      int **cols_out, int *ncols_out);
int detect_figure_regions(const OcrImage *pg, int bg, int W, int H,
                          int maxb, int *bx0, int *by0, int *bx1, int *by1);

#endif /* CRNN_TRANSCRIBE_INTERNAL_H */
