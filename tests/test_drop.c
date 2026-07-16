/* test_drop.c -- drag/drop + bracketed-paste ingestion.
 *
 * Covers the user's core requirement: "drag documents/PDFs/images in and
 * interpret them as documents". Verifies:
 *   1. The TUI key decoder turns ESC[200~ ... ESC[201~ into one TUI_KEY_PASTE
 *      event carrying the raw bytes (the terminal bracketed-paste / drag-drop
 *      framing), spanning multiple reads if necessary.
 *   2. wubudoc's doc_drop_text() extracts text from pasted bytes by CONTENT
 *      (magic bytes), not filename -- a .txt, a markdown blob, and a dropped
 *      file PATH (terminals often paste the path of a dropped file).
 *
 * Pure C11, no TTY. Builds against the wubudoc + wubutui libs. */

#include "input.h"
#include "wubudoc.h"
#include "png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

static int test_paste_decode(void) {
    TuiKeyState st;
    tui_key_state_init(&st);

    /* Whole paste in one chunk. */
    const char *seq = "\x1b[200~Hello World\x1b[201~";
    TuiKey k;
    size_t used = tui_key_decode_s(seq, strlen(seq), &k, &st);
    CK(k.type == TUI_KEY_PASTE, "paste event type");
    CK(used == strlen(seq), "paste consumes whole sequence");
    CK(k.paste_len == 11, "paste data length");
    CK(memcmp(k.paste_data, "Hello World", 11) == 0, "paste data content");

    /* Split across reads: open marker + content in first read, close in next. */
    TuiKeyState st2;
    tui_key_state_init(&st2);
    const char *p1 = "\x1b[200~abc";
    TuiKey k1;
    size_t u1 = tui_key_decode_s(p1, strlen(p1), &k1, &st2);
    CK(k1.type == TUI_KEY_INCOMPLETE, "split paste waits for close");
    CK(u1 == 6, "split paste consumes the open marker, buffers content");
    const char *p2 = "def\x1b[201~";
    TuiKey k2;
    size_t u2 = tui_key_decode_s(p2, strlen(p2), &k2, &st2);
    (void)u2;
    CK(k2.type == TUI_KEY_PASTE, "split paste completes");
    CK(k2.paste_len == 6, "split paste total length");
    CK(memcmp(k2.paste_data, "abcdef", 6) == 0, "split paste content");

    /* Lone ESC is still the Esc key (not swallowed as a paste prefix). */
    TuiKeyState st3; tui_key_state_init(&st3);
    TuiKey k3;
    tui_key_decode_s("\x1b", 1, &k3, &st3);
    CK(k3.type == TUI_KEY_ESC, "lone ESC stays ESC");

    tui_key_state_free(&st);
    tui_key_state_free(&st2);
    tui_key_state_free(&st3);
    printf("  paste decode OK\n");
    return 0;
}

static int test_drop_text(void) {
    DocSession *s = doc_session_create();
    CK(s != NULL, "session create");

    /* Pasted plaintext (no filename available). */
    const char *txt = "Dragged in document text.\nSecond line.\n";
    char *out = doc_drop_text(s, (const uint8_t *)txt, strlen(txt));
    CK(out && strstr(out, "Dragged in document text."), "drop plaintext");
    free(out);

    /* Pasted markdown blob, detected by content. */
    const char *md = "# Title\n\nSome *markdown* content.\n";
    out = doc_drop_text(s, (const uint8_t *)md, strlen(md));
    CK(out && strstr(out, "Title"), "drop markdown by magic bytes");
    free(out);

    /* A dropped file PATH (terminals often paste the path, not contents). */
    FILE *f = fopen("/tmp/_drop_test.txt", "w");
    CK(f != NULL, "temp file open");
    fwrite(txt, 1, strlen(txt), f);
    fclose(f);
    const char *path = "/tmp/_drop_test.txt\n";   /* trailing newline as pasted */
    out = doc_drop_text(s, (const uint8_t *)path, strlen(path));
    CK(out && strstr(out, "Dragged in document text."), "drop file path");
    free(out);
    remove("/tmp/_drop_test.txt");

    doc_session_free(s);
    printf("  drop text extraction OK\n");
    return 0;
}

/* A minimal valid PDF whose single content stream is UNCOMPRESSED, so the
 * clean-room extractor must pull text straight from the raw stream. */
static const char MINI_PDF[] =
    "%PDF-1.7\n"
    "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
    "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
    "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 200 200]>>endobj\n"
    "4 0 obj<</Length 21>>stream\n"
    "(Hello WuBuOffice) Tj\n"
    "endstream endobj\n"
    "trailer<</Root 1 0 R>>\n";

static int test_drop_pdf(void) {
    DocSession *s = doc_session_create();
    CK(s != NULL, "session create");
    /* Drop the PDF bytes (no filename) -> text extracted by magic bytes. */
    char *out = doc_drop_text(s, (const uint8_t *)MINI_PDF, sizeof MINI_PDF - 1);
    CK(out != NULL, "drop pdf returns text");
    CK(out && strstr(out, "Hello WuBuOffice"), "pdf text extracted");
    free(out);
    doc_session_free(s);
    printf("  drop PDF extraction OK\n");
    return 0;
}

/* 16x16 solid-blue PNG (RGBA), generated clean-room (zlib via wubuzip). */
static const uint8_t MINI_PNG[81] = {
    0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
    0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x10,0x08,0x06,0x00,0x00,0x00,0x1F,0xF3,0xFF,
    0x61,0x00,0x00,0x00,0x18,0x49,0x44,0x41,0x54,0x78,0xDA,0x63,0x60,0x60,0xF8,0xFF,
    0x9F,0x32,0x3C,0x6A,0xC0,0xA8,0x01,0xA3,0x06,0x0C,0x13,0x03,0x00,0x32,0xA6,0xFE,
    0x10,0x0D,0xB5,0x8F,0xE1,0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82
};

static int test_drop_png(void) {
    /* 1) Decode correctness (RGBA plane). */
    PngImage *im = png_decode(MINI_PNG, sizeof MINI_PNG);
    CK(im != NULL, "png decode succeeds");
    CK(png_width(im) == 16 && png_height(im) == 16, "png dimensions");
    const uint8_t *px = png_rgba(im);
    CK(px && px[0]==0 && px[1]==0 && px[2]==255 && px[3]==255, "png pixel(0,0)=blue");
    size_t last = (15*16+15)*4;
    CK(px[last+2]==255, "png pixel(15,15)=blue");
    png_free(im);

    /* 2) Drop the PNG bytes -> the image->OCR->document path runs end to end
     *    (does not crash; produces a document). */
    DocSession *s = doc_session_create();
    CK(s != NULL, "session create");
    long id = doc_open_mem(s, MINI_PNG, sizeof MINI_PNG);
    CK(id >= 0, "drop png ingested as a document");
    doc_session_free(s);
    printf("  drop PNG decode + ingest OK\n");
    return 0;
}

int main(void) {
    if (test_paste_decode()) return 1;
    if (test_drop_text()) return 1;
    if (test_drop_pdf()) return 1;
    if (test_drop_png()) return 1;
    printf("DROP TESTS PASSED\n");
    return 0;
}
