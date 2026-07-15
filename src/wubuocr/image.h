/* image.h -- WuBuOCR raster image: an 8-bit grayscale page buffer.
 *
 * The ingestion front-end for image->document digestion. Decodes the
 * dependency-free Netpbm family (PBM/PGM/PPM, ASCII "P1/P2/P3" and binary
 * "P4/P5/P6") into a single canonical representation: 8-bit grayscale, one
 * byte per pixel, row-major, 0=black .. 255=white. Colour (PPM) is flattened
 * with the Rec.601 luma weights; bitmaps (PBM) expand 1=black to 0, 0=white
 * to 255 (Netpbm PBM convention is inverted vs. our grayscale).
 *
 * Netpbm is chosen as the clean-room input format precisely because it needs
 * zero third-party decoders (no libpng/libjpeg): the whole point of a SLERM is
 * a dependency-free core. Real photos can be transcoded to PPM by any tool
 * (`pnmtopng`, ImageMagick, `ffmpeg`) outside the trust boundary.
 *
 * Opaque struct (soul.md sec.10): callers touch pixels only through the API.
 */
#ifndef WUBUOCR_IMAGE_H
#define WUBUOCR_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OcrImage OcrImage;

/* Create a blank WxH grayscale image, all pixels 255 (white). NULL on OOM or
 * zero/overflowing dimensions. */
OcrImage *ocr_image_create(size_t w, size_t h);

/* Decode a Netpbm byte stream (P1..P6). Returns NULL on malformed input.
 * The input buffer need not outlive the image. */
OcrImage *ocr_image_from_netpbm(const uint8_t *data, size_t len);

void ocr_image_free(OcrImage *im);

size_t ocr_image_width(const OcrImage *im);
size_t ocr_image_height(const OcrImage *im);

/* Grayscale value at (x,y): 0=black .. 255=white. Out-of-range reads return
 * 255 (treated as background) so callers can probe past edges safely. */
uint8_t ocr_image_get(const OcrImage *im, size_t x, size_t y);

/* Set a pixel; out-of-range writes are ignored. */
void ocr_image_set(OcrImage *im, size_t x, size_t y, uint8_t v);

/* Read-only pointer to the row-major pixel plane (w*h bytes) for fast scans. */
const uint8_t *ocr_image_pixels(const OcrImage *im);

/* Serialize as a binary PGM ("P5") blob (caller frees *out). Returns 0 on
 * success. Used for round-trip tests and debug dumps. */
int ocr_image_to_pgm(const OcrImage *im, uint8_t **out, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_IMAGE_H */
