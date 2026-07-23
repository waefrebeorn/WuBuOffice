/* crnn_transcribe.h -- page-level document transcription with a CRNN line recognizer.
 *
 * A CRNN is a per-LINE sequence model (conv trunk -> BiLSTM -> CTC), so it
 * plugs into the document pipeline at the LINE level, not the per-glyph
 * OcrRecognizer slot. Given a page OcrImage + a loaded CRNN, this module:
 *   1. segments the page into text lines via horizontal ink-projection,
 *   2. normalizes each line band to the model's `strip` height (the conv trunk
 *      slices exactly `strip`-tall windows),
 *   3. recognizes each line with crnn_recognize(),
 *   4. emits a docmodel JSON string ({"type":"doc","paragraph":[{"text":...}]})
 *      consumable by wubuconv_convert_mem() -> docx / md / odt ...
 *
 * C11, no deps beyond the wubuocr core. The module owns the JSON string it
 * returns (caller frees with free()).
 */
#ifndef WUBUOCR_CRNN_TRANSCRIBE_H
#define WUBUOCR_CRNN_TRANSCRIBE_H

#include <stddef.h>
#include <stdint.h>
#include "crnn.h"      /* CRNN */
#include "image.h"    /* OcrImage */

#ifdef __cplusplus
extern "C" {
#endif

/* Transcribe a page image into a docmodel JSON string.
 *   m      : loaded CRNN (charset is A..Z for the Latin model; pass the
 *            matching `charset` string)
 *   page   : grayscale page (0=black .. 255=white)
 *   strip  : model slice height the line crops are normalized to (must equal
 *            the value used when `m` was trained, typically 20)
 *   charset: mapping class k(1..C-1) -> charset[k-1]; blank(0) skipped
 *   out_json: set to a malloc'd NUL-terminated JSON string on success
 *             (caller frees). Set to NULL on failure.
 * Returns 0 on success, -1 on OOM/empty input. */
int crnn_transcribe_page_json(CRNN *m, const OcrImage *page,
                              int strip, const char *charset,
                              char **out_json);

#ifdef __cplusplus
}
#endif
#endif /* WUBUOCR_CRNN_TRANSCRIBE_H */
