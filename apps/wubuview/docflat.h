/* docflat.h -- flatten a wubudoc normalized JSON model into readable plain text.
 *
 * The wubudoc facade ingests ANY supported format into one normalized JSON
 * model. For a HUMAN viewer we need that model as flowing text. docflat walks
 * the common model shapes and produces a single UTF-8/ASCII string with blank
 * lines between blocks -- pure, no I/O, no screen, so it is unit-testable from a
 * JSON literal. The interactive viewer (wubuview) renders this text with the
 * wubutui word-wrap + scroll primitives.
 *
 * Recognized shapes (best-effort, order matters):
 *   {"type":"document","blocks":[{"kind":...,"text":...}, ...]}   (doc model)
 *   {"cells":[[..]]} or {"rows":[[..]]}                           (sheet model)
 *   {"text":"..."}                                                (text wrapper)
 *   arrays / objects                                              (generic walk)
 * Anything unrecognized falls back to the compact JSON so the user still sees
 * content rather than nothing (never fabricates or hides data).
 */
#ifndef WUBUVIEW_DOCFLAT_H
#define WUBUVIEW_DOCFLAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Flatten a JSON model string to plain text. Returns malloc'd NUL-terminated
 * text (caller frees), or NULL on OOM / NULL input. Never returns NULL for a
 * syntactically valid but unrecognized model -- it returns the JSON itself. */
char *docflat_from_json(const char *model_json);

#ifdef __cplusplus
}
#endif

#endif /* WUBUVIEW_DOCFLAT_H */
