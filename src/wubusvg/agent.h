/* agent.h -- wubuOS-facing protocol layer for wubusvg.
 *
 * wubusvg is the vector-document ingestion + creation engine. The Agent is the
 * machine interface (the "AGI GUI"): wubuOS sends one JSON command per line
 * (NDJSON) on stdin, and wubusvg emits one JSON result per line on stdout.
 * No pixels; pure protocol. The human GUI layers on later and reuses this same
 * core. Opaque. Clean C11. Wire format mirrors WuBuPad's agent.c so both tools
 * speak the same wubuOS dialect.
 *
 * Recognized commands (each is a JSON object {"cmd":"<name>", ...}):
 *   ingest  {text}                       -> {ok, glyphs?}  (parse SVG text)
 *   open    {path}                       -> {ok, root, glyphs?}  (read+parse file)
 *   find    {path}                       -> {found, name}
 *   find_all{path}                       -> {count}
 *   set     {path, key, val}             -> {ok}
 *   remove  {path}                       -> {ok, removed}
 *   count   {tag}                        -> {count}
 *   regurgitate {}                       -> {svg}  (re-emit well-formed SVG)
 *   quit    {}                           -> terminates the serve loop
 * Missing field or unknown command -> {"error":"..."}. */
#ifndef WUBUSVG_AGENT_H
#define WUBUSVG_AGENT_H

#include <stdio.h>

typedef struct SvgAgent SvgAgent;

SvgAgent *svgagent_create(void);
void      svgagent_free(SvgAgent *a);

/* Process one NDJSON command (NUL-terminated). Returns a malloc'd JSON result
 * string (caller frees) or NULL on fatal error. */
char *svgagent_handle(SvgAgent *a, const char *command_json);

/* Line-buffered serve loop over `in` writing NDJSON to `out`. Stops on EOF or
 * {"cmd":"quit"}. Returns 0 on clean EOF, -1 on a protocol error. */
int svgagent_serve(SvgAgent *a, FILE *in, FILE *out);

#endif /* WUBUSVG_AGENT_H */
