/* doccmd.h -- opaque document-editing command module for the Document view.
 *
 * Owns the model-mutation commands (insert chart/draw/math/link/list/table/
 * image/breaks/header/footer/comment/track-change/field/script, export EPUB,
 * save DOCX/ODT, run a11y check). Each command takes the wubumodel_doc* (and a
 * few view-provided bits) and returns what the view needs to own.
 *
 * The view keeps only its overlay raster slots + status string; the command
 * logic lives here. C11, opaque where practical, no god-header: include only
 * this file (+ wubumodel.h + wubusvg/rast.h for types).
 */
#ifndef WUBUOS_DOCCMD_H
#define WUBUOS_DOCCMD_H

#include <stddef.h>
#include "wubumodel/model.h"   /* wubumodel_doc, node kinds */
#include "a11y.h"              /* a11y_report, a11y_check_doc */
#include "rtf.h"               /* RtfRun, rtf_write (src/wuburtf) */

typedef void (*svg_text_fn)(const char *s, int x, int y, int size,
                            unsigned char r, unsigned char g, unsigned char b,
                            unsigned char *fb, int w, int h);

/* --- object inserts (chart/draw/math) ---
 * Build the sample object, rasterize to a freshly-malloc'd RGBA buffer
 * (caller frees with free()). Returns the buffer or NULL on failure.
 * `text_fn` is the rasterizer's text callback (pass the shell's font draw). */
unsigned char *doccmd_insert_chart(svg_text_fn text_fn, int *w, int *h);
unsigned char *doccmd_insert_draw (svg_text_fn text_fn, int *w, int *h);
unsigned char *doccmd_insert_math (svg_text_fn text_fn, int *w, int *h);

/* --- structural inserts (mutate doc; return 1 if TOC needs rebuild) --- */
int doccmd_insert_link(wubumodel_doc *doc);
int doccmd_insert_list(wubumodel_doc *doc);
int doccmd_insert_table(wubumodel_doc *doc);
int doccmd_insert_image(wubumodel_doc *doc);
int doccmd_insert_image_alt(wubumodel_doc *doc, const char *alt);   /* UXA-47 */
int doccmd_insert_qr(wubumodel_doc *doc, const char *text);        /* EXP-89 */
int doccmd_insert_link_url(wubumodel_doc *doc, const char *url);    /* DOC-66 */
int doccmd_insert_pagebreak(wubumodel_doc *doc);
int doccmd_insert_sectionbreak(wubumodel_doc *doc);
int doccmd_insert_header(wubumodel_doc *doc);
int doccmd_insert_footer(wubumodel_doc *doc);
int doccmd_insert_comment(wubumodel_doc *doc);
int doccmd_insert_trackchange(wubumodel_doc *doc);
int doccmd_insert_field(wubumodel_doc *doc);

/* Insert a wubuscript-computed field; `expr` evaluated with `lines` (paragraph
 * count) available. Returns 1 if TOC needs rebuild. */
int doccmd_insert_script_field(wubumodel_doc *doc, const char *expr);

/* Export the doc to EPUB at the given path; returns a malloc'd status string
 * (caller frees) describing success/failure, or NULL on OOM. */
char *doccmd_export_epub(wubumodel_doc *doc, const char *out);

/* Layout-based exporters (INT-3.5). Each builds a wubulayout_doc from `doc`,
 * invokes the matching wubuexp_* function, and returns a malloc'd status
 * string (caller frees). Output paths are fixed:
 *   PDF       -> /tmp/wubuos_export.pdf
 *   HTML      -> /tmp/wubuos_export.html
 *   Markdown  -> /tmp/wubuos_export.md
 *   LaTeX     -> /tmp/wubuos_export.tex
 *   RTF       -> /tmp/wubuos_export.rtf
 * The exports reflect the model's STRUCTURE (paragraphs + text), not its
 * visual styling -- that is a deliberate cap that keeps the doccmd API
 * minimal. */
char *doccmd_export_pdf      (wubumodel_doc *doc);
char *doccmd_export_pdf_direct(wubumodel_doc *doc);
char *doccmd_export_html     (wubumodel_doc *doc);
char *doccmd_export_markdown (wubumodel_doc *doc);
char *doccmd_export_latex    (wubumodel_doc *doc);
char *doccmd_export_rtf      (wubumodel_doc *doc);

/* doccmd_export_rtf_runs -- direct RTF write via src/wuburtf (run-based,
 * no layout pass). Used by callers that already have styled runs. */
char *doccmd_export_rtf_runs(const RtfRun *runs, int n);

/* doccmd_redact_doc -- redact ranges of the model's plain-text dump
 * (src/wuburedact). `ranges` is a flat array of n_ranges*2 size_t byte
 * offsets; result written to /tmp/wubuos_redacted.txt. */
char *doccmd_redact_doc(wubumodel_doc *doc, const size_t *ranges, int n_ranges);

/* doccmd_col_demo -- exercise src/wubucol (comment-thread store). */
char *doccmd_col_demo(void);

/* doccmd_cite_demo -- exercise src/wubucite (DOC-68 citation store). */
char *doccmd_cite_demo(void);

/* doccmd_caption_demo -- exercise src/wubucaption (DOC-70). */
char *doccmd_caption_demo(void);

/* doccmd_heading_enforce -- exercise src/wubuheading (UXA-49/50) on `doc`. */
char *doccmd_heading_enforce(wubumodel_doc *doc);

/* doccmd_eqnum_scan -- exercise src/wubueqnum (DOC-69) on `doc`. */
char *doccmd_eqnum_scan(wubumodel_doc *doc);

/* doccmd_vars_expand -- exercise src/wubuvars (DOC-73). `tpl` may be NULL. */
char *doccmd_vars_expand(const char *tpl);

/* doccmd_hash_sha256 -- one-shot SHA-256 hex of `text`. */
char *doccmd_hash_sha256(const char *text);

/* doccmd_doc_sign -- HMAC-SHA256 sign+verify a fixed message (EXP-90). */
char *doccmd_doc_sign(void);

/* doccmd_crdt_demo -- exercise src/wubucrdt (LWW-merge). */
char *doccmd_crdt_demo(void);

/* doccmd_csv_parse -- exercise src/wubucsv (EXP-86). */
char *doccmd_csv_parse(void);

/* doccmd_focus_demo -- exercise src/wubufocus (UXA-51). */
char *doccmd_focus_demo(void);

/* doccmd_watermark_demo -- exercise src/wubuwatermark (DOC-71). */
char *doccmd_watermark_demo(void);

/* doccmd_dyslexia_demo -- exercise src/wubudyslexia (UXA-52). */
char *doccmd_dyslexia_demo(void);

/* doccmd_fmtpaint_demo -- exercise src/wubufmtpaint (DOC-74) on `doc`. */
char *doccmd_fmtpaint_demo(wubumodel_doc *doc);

/* doccmd_sandbox_demo -- exercise src/wubusandbox (SCR-100). */
char *doccmd_sandbox_demo(void);

/* doccmd_form_demo -- exercise src/wubuform (DOC-72). */
char *doccmd_form_demo(void);

/* doccmd_history_demo -- exercise src/wubuhistory (DOC-75). */
char *doccmd_history_demo(void);

/* doccmd_lang_demo -- exercise src/wubulang (DOC-76). */
char *doccmd_lang_demo(void);

/* doccmd_nesttab_demo -- exercise src/wubunesttab (DOC-77) on `doc`. */
char *doccmd_nesttab_demo(wubumodel_doc *doc);

/* doccmd_pdfextract_demo -- exercise src/wubupdfextract (EXP-91). */
char *doccmd_pdfextract_demo(void);

/* doccmd_pdfform_demo -- exercise src/wubupdfform (EXP-92). */
char *doccmd_pdfform_demo(void);

/* doccmd_scope_demo -- exercise src/wubuscope (DOC-78). */
char *doccmd_scope_demo(void);

/* doccmd_sync_demo -- exercise src/wubusync (DOC-79). */
char *doccmd_sync_demo(void);

/* doccmd_xps_demo -- exercise src/wubuxps (EXP-93). */
char *doccmd_xps_demo(void);

/* doccmd_aislot_demo -- exercise src/wubuaislot (SCR-99). */
char *doccmd_aislot_demo(void);

/* ---- Spreadsheet & document analysis modules (2026-08-11 wave) ---- */
char *doccmd_sort_demo(void);
char *doccmd_filter_demo(void);
char *doccmd_subtotal_demo(void);
char *doccmd_goalseek_demo(void);
char *doccmd_solver_demo(void);
char *doccmd_pivot_demo(void);
char *doccmd_scenario_demo(void);
char *doccmd_freeze_demo(void);
char *doccmd_hyperlink_demo(void);
char *doccmd_thesaurus_demo(void);
char *doccmd_grammar_demo(void);
char *doccmd_index_demo(void);
char *doccmd_mailmerge_demo(void);
char *doccmd_diff_demo(void);
char *doccmd_masterdoc_demo(void);
char *doccmd_dropcap_demo(void);
char *doccmd_ruler_demo(void);
char *doccmd_gridline_demo(void);
char *doccmd_icon_demo(void);
char *doccmd_gallery_demo(void);
char *doccmd_sidebar_demo(void);
char *doccmd_transition_demo(void);
char *doccmd_animation_demo(void);
char *doccmd_masterslide_demo(void);
char *doccmd_connector_demo(void);
char *doccmd_encrypt_demo(void);
char *doccmd_mailexport_demo(void);
char *doccmd_notebookbar_demo(void);
char *doccmd_qr_demo(void);
char *doccmd_smartart_demo(void);
char *doccmd_basic_demo(void);
char *doccmd_3d_demo(void);

/* Save the doc back to DOCX/ODT (round-trip). `path` is the current file (may
 * be NULL) used to pick the output name/format. Returns a malloc'd status
 * string (caller frees) or NULL on OOM. */
char *doccmd_save(wubumodel_doc *doc, const char *path);

/* Run an a11y check on the doc, filling `out` (caller owns/frees via
 * a11y_report_free). Returns 0 normally. */
void doccmd_a11y_check(wubumodel_doc *doc, a11y_report *out);

#endif /* WUBUOS_DOCCMD_H */
