/* image2doc.c -- END-TO-END: a page image -> an editable document via the
 * CRNN line recognizer.
 *
 *   image2doc IN.png OUT.docx        (or .md / .odt / .html / .json)
 *
 * Pipeline:
 *   PNG  -> OcrImage (grayscale)
 *   OcrImage + CRNN -> crnn_transcribe_page_json()  (docmodel JSON)
 *   docmodel JSON -> wubuconv_convert_mem("json" -> outext)  -> .docx/.md/...
 *
 * The CRNN is a per-LINE sequence model (conv trunk + BiLSTM + CTC), so this
 * is the real product path: a picture becomes an editable document, not a
 * pile of per-glyph guesses. Model loaded via LOAD=path (required).
 *
 * Usage: LOAD=/tmp/latin.crnn image2doc IN.png OUT.docx
 */
#include "png.h"          /* ocr_image_from_png */
#include "image.h"        /* OcrImage */
#include "crnn.h"         /* CRNN, crnn_load */
#include "crnn_transcribe.h"
#include "conv_map.h"     /* wubuconv_convert_mem */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Latin model was trained at STRIP=20. Default charset is A..Z; override with
 * CHARS=... to match a model trained on a broader document alphabet. */
#define LATIN_STRIP 20
#define LATIN_CHARSET "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

static uint8_t *readf(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(s ? (size_t)s : 1);
    if (fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)s; return b;
}

/* Extract the lowercase extension (without dot) from a path. */
static const char *ext_of(const char *p) {
    const char *dot = strrchr(p, '.');
    return dot ? dot + 1 : "";
}

int image2doc_main(int argc, char **argv) {
    if (argc < 3) {
        printf("usage: LOAD=<model.crnn> %s IN.png OUT.docx|OUT.md|OUT.odt|OUT.html|OUT.json\n", argv[0]);
        return 1;
    }
    const char *LOAD = getenv("LOAD");
    if (!LOAD) { printf("set LOAD=<trained .crnn>\n"); return 1; }
    const char *CHARS = getenv("CHARS");
    const char *charset = CHARS ? CHARS : LATIN_CHARSET;

    const char *in = argv[1], *out = argv[2];
    const char *outext = ext_of(out);
    if (!*outext) { printf("output needs a known extension (.docx/.md/.odt/.html/.json)\n"); return 1; }

    /* --- load image (PNG preferred; fall back to Netpbm PGM/PPM) --- */
    size_t pn = 0; uint8_t *pbuf = readf(in, &pn);
    if (!pbuf) { printf("cannot read %s\n", in); return 1; }
    int interlaced = 0;
    OcrImage *page = ocr_image_from_png(pbuf, pn, &interlaced);
    if (!page) page = ocr_image_from_netpbm(pbuf, pn);
    free(pbuf);
    if (!page) { printf("image decode failed (PNG/Netpbm): %s\n", in); return 1; }
    if (interlaced) { printf("rejecting interlaced (Adam7) PNG: %s\n", in); ocr_image_free(page); return 1; }

    /* --- load model --- */
    CRNN *m = NULL;
    if (!crnn_load(LOAD, &m) || !m) { printf("crnn_load failed: %s\n", LOAD); ocr_image_free(page); return 1; }

    /* --- transcribe page -> docmodel JSON --- */
    char *json = NULL;
    if (crnn_transcribe_page_json(m, page, LATIN_STRIP, charset, &json) != 0 || !json) {
        printf("transcription failed\n"); crnn_free(m); ocr_image_free(page); return 1;
    }
    ocr_image_free(page);
    crnn_free(m);

    /* --- JSON -> editable document via the unified converter --- */
    uint8_t *blob = NULL; size_t blen = 0;
    int rc = wubuconv_convert_mem((const uint8_t *)json, strlen(json),
                                  "json", outext, &blob, &blen);
    free(json);
    if (rc != 0 || !blob) { printf("document conversion failed (json -> %s)\n", outext); return 1; }

    FILE *o = fopen(out, "wb");
    if (!o) { printf("cannot write %s\n", out); free(blob); return 1; }
    fwrite(blob, 1, blen, o); fclose(o);
    free(blob);

    printf("wrote %s (%zu bytes) from %s via CRNN line recognizer\n", out, blen, in);
    return 0;
}

#ifndef WUBEOFFICE_EMBED
/* Standalone entry point (the wubuoffice CLI dispatches via image2doc_main). */
int main(int argc, char **argv) { return image2doc_main(argc, argv); }
#endif
