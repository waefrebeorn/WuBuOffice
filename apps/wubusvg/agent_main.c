/* wubusvg_agent main -- wubuOS NDJSON agent for vector documents.
 * Reads one JSON command per line on stdin, writes one JSON result per line on
 * stdout. The "AGI GUI" for wubusvg. Mirrors WuBuPad's agent wire format so
 * wubuOS drives both tools identically.
 *   echo '{"cmd":"ingest","text":"<svg>..."}' | wubusvg_agent
 *   wubusvg_agent --file doc.svg            # pre-ingest, then read stdin
 * Native C11 + POSIX. */
#include "agent.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    SvgAgent *a = svgagent_create();
    if (!a) { fprintf(stderr, "oom\n"); return 1; }

    /* optional pre-ingest from a file */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            char buf[512];
            snprintf(buf, sizeof buf, "{\"cmd\":\"open\",\"path\":\"%s\"}", argv[i+1]);
            char *resp = svgagent_handle(a, buf);
            if (resp) { fprintf(stderr, "%s\n", resp); free(resp); }
            i++;
        }
    }

    int rc = svgagent_serve(a, stdin, stdout);
    svgagent_free(a);
    return rc == 0 ? 0 : 1;
}
