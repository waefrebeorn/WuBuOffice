/* a11y.h -- dependency-free C11 accessibility auditor (wubua11y).
 *
 * Checks a document / EPUB for accessibility gaps per WCAG / EPUB a11y basics:
 *   - document is non-empty (has body text)
 *   - a title is present (a Title/heading-1 paragraph, or OPF <dc:title>)
 *   - a document language is declared (OPF <dc:language>)
 *   - a navigation document exists (EPUB nav.xhtml)
 *   - every image has alt text (XHTML <img alt="...">)
 *   - heading hierarchy does not skip levels (1 -> 2 -> 3, not 1 -> 3)
 *
 * Two entry points: a11y_check_doc() audits a wubumodel_doc directly;
 * a11y_check_epub_parts() audits already-extracted OPF + nav + chapter XHTML
 * text (so no ZIP/inflate dependency is needed to read deflated members).
 * Each produces a list of issue strings the caller frees. Self-contained. */
#ifndef WUBUOFFICE_A11Y_H
#define WUBUOFFICE_A11Y_H

#include <stddef.h>
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A report: count + issue strings (caller frees with a11y_report_free). */
typedef struct {
    char **items;
    int    count;
    int    cap;
} a11y_report;

void a11y_report_free(a11y_report *r);
/* Print a report to stdout (for tooling). */
void a11y_report_print(const a11y_report *r);

/* Audit a wubumodel_doc for a11y issues. `expect_title`=1 requires a
 * Title/heading paragraph; `expect_lang`=1 requires lang (model has no lang
 * field, so this currently flags "document language not declared" -- wire
 * your language in at the EPUB/OPF layer). */
int a11y_check_doc(const wubumodel_doc *doc, int expect_title,
                   int expect_lang, a11y_report *out);

/* Audit extracted EPUB parts (NUL-terminated text). Any pointer may be NULL
 * to skip that check. `chapter_xhtml` is a single concatenated chapter body
 * (or one chapter); pass NULL to skip image/heading checks. */
int a11y_check_epub_parts(const char *opf_text, const char *nav_text,
                            const char *chapter_xhtml, a11y_report *out);

/* DOC-44: WCAG contrast utilities for the theme engine. */
/* Relative contrast ratio (1.0..21.0) between two sRGB colors. */
double wubua11y_contrast_ratio(int r1,int g1,int b1, int r2,int g2,int b2);
/* Returns 1 if every built-in chrome palette meets WCAG AA (>=4.5:1), else 0.
 * Emits a report entry per failing palette when out != NULL. */
int wubua11y_palette_aa(a11y_report *out);

#ifdef __cplusplus
}
#endif
#endif /* WUBUOFFICE_A11Y_H */
