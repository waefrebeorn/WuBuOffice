/* wubudoc_internal.h -- PRIVATE surface for the wubudoc facade module.
 *
 * Included by the per-concern .inc fragments (doc_session, doc_detect,
 * doc_ingest, doc_access, doc_create) and by the unity aggregator
 * wubudoc.c. This header is NOT part of the public API (wubudoc.h): it
 * carries the full (opaque-to-callers) DocHandle/DocSession definitions, the
 * module include block (pulled from the clean-room sub-libraries the facade
 * reuses), and the forward declarations of the module-private `static`
 * helpers the fragments hand to one another. Include-guarded so the shared
 * symbols are defined exactly once in the single unity translation unit.
 *
 * The facade never re-implements format logic -- every fragment reuses an
 * existing clean-room module (wubujson, wubusvg, wubufont, wubuzip, wubucfb,
 * wubuxml, wubuconv, pdf_extract, img2doc) as the single source of truth.
 */
#ifndef WUBUMOC_INTERNAL_H
#define WUBUMOC_INTERNAL_H

#define _POSIX_C_SOURCE 200809L
#include "wubudoc.h"

#include "json.h"        /* wubujson */
#include "wubusvg.h"
#include "wubufont.h"
#include "woff.h"
#include "reader.h"
#include "zip.h"
#include "cfb.h"
#include "cfb_write.h"
#include "xml.h"
#include "../../apps/wubuconv/conv_map.h"   /* semantic engine (treat as media) */
#include "pdf_extract.h"  /* clean-room PDF text extraction */
#include "img2doc.h"      /* image -> OCR'd document (png + font bank) */
#include "b64.h"          /* shared base64 codec (RFC 4648) */
#include "agent.h"        /* NDJSON agent protocol (thin dispatcher) */

#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <stdio.h>
#include <sys/stat.h>

/* ---------- normalized model ----------
 * model = { kind, source, text?, parts?[], streams?[] }
 *   text kinds (txt/md/json/csv/xml/html/svg): text = raw content string
 *   container kinds (zip/docx/.../doc/...): parts[] = {name, bytes(base64)}
 *   font kinds (ttf/otf/woff): tables[] = {tag, bytes(base64)}, plus metrics
 */
typedef struct {
    char  *kind;
    char  *source;
    char  *text;       /* for text kinds, malloc'd; else NULL */
    void  *model;      /* JVal* (owned) */
    /* media attachments for creation (zip/cfb) */
    char **media_name; uint8_t **media_data; size_t *media_len; size_t media_n, media_cap;
} DocHandle;

struct DocSession {
    DocHandle *h;
    size_t n, cap;
    struct OcrFontBank *ocr_bank;   /* lazily-built multi-font recognizer */
};

/* ---- forward declarations of module-private static helpers (shared across
 * fragments, invisible outside this TU) ---- */
static void    handle_free(DocHandle *d);
static long    session_add(DocSession *s, const char *kind, const char *source);
static DocHandle *get(const DocSession *s, long id);
static JVal   *build_model(const char *kind, const uint8_t *data, size_t len, char **out_text);
static long    ingest(DocSession *s, const char *kind, const char *source,
                      const uint8_t *data, size_t len);
static char   *docmodel_flat_text(const char *json);
static struct OcrFontBank *session_ocr_bank(DocSession *s);
static int     collect_parts(DocSession *s, long id, char ***names, uint8_t ***datas,
                             size_t **lens, size_t *n);

#endif /* WUBUMOC_INTERNAL_H */
