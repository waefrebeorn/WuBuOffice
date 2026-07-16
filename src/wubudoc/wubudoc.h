/* wubudoc.h -- unified document ingestion + creation facade for wubuOS.
 *
 * WuBuOffice/WuBuPad are the backbone. This module is the single protocol
 * surface wubuOS talks to: ingest a document (any supported format) into a
 * normalized JSON model, query/transform it, push media, and CREATE a document
 * back out in any supported format. One NDJSON dialect for the whole "9 yards".
 *
 * Supported (reusing existing clean-room modules, never re-implemented):
 *   ingest:  .txt .md .json .csv .svg .xml .html
 *            .ttf .otf .woff (sfnt)
 *            .zip .docx .xlsx .pptx (OOXML = zip+xml)
 *            .odt .ods .odp (ODF   = zip+xml)
 *            .doc .xls .ppt (legacy = CFB)
 *   create:  .json .csv .md .svg .xml .html (zip via wubuzip writer;
 *            .docx/.xlsx/.pptx/.odt/.ods/.odp via zip; .doc/.xls/.ppt via CFB)
 *
 * The normalized model is JSON (wubujson). Each loaded document is wrapped in
 * a DocHandle; the facade keeps a session of handles (id 0,1,2,...). Opaque,
 * clean C11. The AGI never touches format internals -- only the JSON model. */
#ifndef WUBUDOC_H
#define WUBUDOC_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef struct DocSession DocSession;

/* Create an empty document session. */
DocSession *doc_session_create(void);
void        doc_session_free(DocSession *s);

/* Load a document from disk into the session.
 *   path: filesystem path (extension selects the ingestor).
 * Returns a handle id (>=0) or -1 on failure. The handle owns a normalized
 * JSON model retrievable with doc_json(). */
long doc_open(DocSession *s, const char *path);

/* Ingest already-read bytes (caller supplies content + a synthetic type,
 * e.g. "json", "svg", "xml", "html", "csv", "md", "txt", "ttf", "otf", "woff",
 * "zip", "docx", "xlsx", "pptx", "odt", "ods", "odp", "doc", "xls", "ppt").
 * Returns a handle id or -1. */
long doc_ingest_bytes(DocSession *s, const char *type, const uint8_t *data, size_t len);

/* Ingest already-read bytes (caller supplies content). Kind is detected from
 * content (magic bytes) rather than a filename. Returns a handle id or -1. */
long doc_open_mem(DocSession *s, const uint8_t *data, size_t len);

/* Ingest dropped/pasted bytes: if they look like an existing file path
 * (terminals often paste a dropped file's path) open that; otherwise ingest
 * the bytes by content. Returns a handle id or -1. */
long doc_open_auto(DocSession *s, const uint8_t *data, size_t len);

/* Extract a displayable text projection for dropped/pasted bytes (malloc'd,
 * free it). For text kinds returns the raw text; for office/container kinds it
 * asks wubuconv for a Markdown projection. NULL if nothing extractable. */
char *doc_drop_text(DocSession *s, const uint8_t *data, size_t len);

/* Convenience: ingest a JSON/text literal already in memory as `type`. */
long doc_ingest_text(DocSession *s, const char *type, const char *text);

/* Accessors for a handle. All return NULL/-1 if the id is invalid. */
const char *doc_kind(const DocSession *s, long id);     /* detected kind string */
const char *doc_source(const DocSession *s, long id);    /* path/type */
char      *doc_json(const DocSession *s, long id);       /* normalized model (malloc'd, free) */
const char *doc_text(const DocSession *s, long id);      /* raw text for text kinds (or NULL) */

/* Replace a handle's normalized model (used by edits). Takes ownership of the
 * JVal* (do not free it after). */
void doc_set_model(DocSession *s, long id, void *model); /* model is struct JVal* */

/* Media: add a named attachment (e.g. an image to embed in a docx). Returns 0.
 * `data` is copied. `name` is the in-archive path (e.g. "word/media/img1.png"). */
int  doc_add_media(DocSession *s, long id, const char *name, const uint8_t *data, size_t len);

/* Serialize a handle to `out_path` in the requested `format`
 * (json|csv|md|svg|xml|html|zip|docx|xlsx|pptx|odt|ods|odp|doc|xls|ppt).
 * Returns 0 on success, -1 on unsupported/error. */
int  doc_create(DocSession *s, long id, const char *format, const char *out_path);

/* Serialize a handle to an in-memory blob of `format`. Returns malloc'd bytes
 * (caller frees) and sets *out_len, or NULL on failure. */
uint8_t *doc_create_bytes(DocSession *s, long id, const char *format, size_t *out_len);

/* NDJSON agent: process one command line (JSON), return malloc'd JSON result
 * (free it). Commands:
 *   open   {path}                       -> {id, kind, source}
 *   ingest {type, text?}                -> {id, kind}
 *   load   {type, bytes(base64)?, text?}-> {id, kind}   (bytes take precedence)
 *   json   {id}                         -> {id, model}            (normalized model)
 *   text   {id}                         -> {text}                 (raw text if any)
 *   set    {id, model(JSON value)}      -> {ok}                  (replace model)
 *   media  {id, name, bytes(base64)}    -> {ok}
 *   create {id, format, path}           -> {ok, path, bytes}
 *   list   {}                           -> {docs:[{id,kind,source}]}
 *   close  {id}                         -> {ok}
 *   quit   {}                           -> {ok}   (ends serve loop)
 *   A model value round-trips through wubujson, so the AGI edits documents as
 *   JSON and re-creates them losslessly. */
char *doc_agent_handle(DocSession *s, const char *command_json);
int   doc_agent_serve(DocSession *s, FILE *in, FILE *out);

#endif /* WUBUDOC_H */
