/* docfmt.h -- alternate document serializations for the OCR docmodel JSON.
 *
 * The transcriber emits a docmodel JSON:
 *   {"blocks":[{"kind":"paragraph","text":...,"conf":N} |
 *             {"kind":"table","rows":R,"cols":C,"cells":[[...]],"conf":[[...]]}]}
 * These helpers re-serialize that JSON into the formats an OCR product needs
 * but wubuconv does not yet cover (plain text, TSV, CSV, JSONL, LaTeX, RTF,
 * hOCR, ALTO). They parse with the bundled wubujson and are dependency-free.
 *
 * Clean-room C11. Each function returns a malloc'd NUL-terminated string the
 * caller frees, or NULL on error. */
#ifndef WUBUOCR_DOCFMT_H
#define WUBUOCR_DOCFMT_H

#include <stddef.h>   /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Plain text: paragraphs separated by newlines; tables as monospaced rows. */
char *docfmt_to_text(const char *json);
/* Tab-separated values: one line per block; table cells tab-separated. */
char *docfmt_to_tsv(const char *json);
/* Comma-separated values with RFC-4180 quoting. */
char *docfmt_to_csv(const char *json);
/* JSON Lines: one JSON object per block, newline separated (streaming-friendly). */
char *docfmt_to_jsonl(const char *json);
/* LaTeX: paragraphs as \\paragraph{}, tables as tabular. */
char *docfmt_to_latex(const char *json);
/* Rich Text Format (minimally formatted, ANSI codepage). */
char *docfmt_to_rtf(const char *json);
/* hOCR: HTML with OCR-X classes (paragraph/table) per block. */
char *docfmt_to_hocr(const char *json);
/* ALTO XML (v4-ish): Layout/Page/TextBlock/TextLine/String skeleton. */
char *docfmt_to_alto(const char *json);
/* TEI (Text Encoding Initiative) XML: <TEI><text><body> with <p> per
 * paragraph and <table>/<row>/<cell> per table. Scholarly-archive format. */
char *docfmt_to_tei(const char *json);
/* Excel (OOXML .xlsx, minimal single-sheet workbook). Returns a malloc'd
 * byte buffer (NOT NUL-terminated) in *out with *out_len set; caller frees.
 * Returns 0 on success, -1 on error. Unlike the other helpers this returns
 * binary (a zip), so it uses the out/out_len contract. */
int docfmt_to_xlsx(const char *json, char **out, size_t *out_len);

#ifdef __cplusplus
}
#endif
#endif /* WUBUOCR_DOCFMT_H */
