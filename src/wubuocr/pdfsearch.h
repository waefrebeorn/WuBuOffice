/* pdfsearch.h -- searchable PDF: page image + invisible text layer.
 *
 * Takes the decoded (grayscale) page image plus the docmodel JSON
 * (which now carries per-block "bbox" and per-cell "cellbox"), and
 * writes a PDF where the original image is the page background and an
 * invisible text layer (PDF text-showing mode 3) is laid over it at
 * the recognized coordinates. Selecting the PDF text copies the OCR
 * result; searching hits it. Clean-room C11 (no libpdf / no zlib):
 * the image is embeded UNCOMPRESSED (always-decodes, no Flate risk),
 * the text uses the built-in Helvetica/WinAnsiEncoding (ASCII charset). */
#ifndef WUBUOCR_PDFSEARCH_H
#define WUBUOCR_PDFSEARCH_H
#include "image.h"   /* OcrImage */
#include <stdio.h>   /* FILE */

#ifdef __cplusplus
extern "C" {
#endif

/* Write a searchable PDF of `img` with the invisible OCR text from
 * `docmodel_json` to FILE `out`. Returns 0 on success, -1 on error. */
int wubuocr_write_searchable_pdf(const OcrImage *img,
                                  const char *docmodel_json,
                                  FILE *out);

#ifdef __cplusplus
}
#endif
#endif
