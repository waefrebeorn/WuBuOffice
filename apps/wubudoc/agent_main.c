/* wubudoc_agent main -- wubuOS unified document bus.
 * NDJSON protocol over stdin/stdout. Ingest ANY supported format, transform as
 * JSON, create ANY supported format. One dialect for the whole "9 yards".
 *   echo '{"cmd":"open","path":"doc.docx"}' | wubudoc_agent
 * Native C11 + POSIX. */
#include "wubudoc.h"
#include "json.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    DocSession *s = doc_session_create();
    if (!s) { fprintf(stderr, "oom\n"); return 1; }
    int rc = doc_agent_serve(s, stdin, stdout);
    doc_session_free(s);
    return rc == 0 ? 0 : 1;
}
