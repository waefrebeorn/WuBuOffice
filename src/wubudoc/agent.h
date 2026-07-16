/* agent.h -- NDJSON agent protocol for the wubudoc facade (see agent.c).
 *
 * The wubuOS bus talks to documents over ONE NDJSON dialect. This module
 * implements the line protocol (doc_agent_handle / doc_agent_serve); the
 * actual ingest/create work is delegated to the public wubudoc API, so the
 * agent holds no document state of its own. Mirrors wubusvg's agent split. */
#ifndef WUBUDOC_AGENT_H
#define WUBUDOC_AGENT_H

#include <stdio.h>

typedef struct DocSession DocSession;

/* Process one NDJSON command line; returns a malloc'd JSON result (free it).
 * See wubudoc.h for the command set. */
char *doc_agent_handle(DocSession *s, const char *command_json);

/* Read NDJSON commands from `in`, write results to `out`, until EOF or a
 * quit command. Returns 0 on clean EOF, -1 on protocol/handle error. */
int   doc_agent_serve(DocSession *s, FILE *in, FILE *out);

#endif /* WUBUDOC_AGENT_H */
