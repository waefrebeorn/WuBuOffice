/* test_agent_verbs.c -- agent protocol verb coverage: find + structure.
 * Research synthesis (SUPERIORITY_SYNTHESIS.md A1): every UI action needs an
 * agent verb. This pins the two new semantic verbs so regressions fail. */
#include "../src/wubudoc/wubudoc.h"
#include "../src/wubudoc/agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

/* extract a numeric field from an NDJSON result without a JSON parser:
 * crude but sufficient: find "key":<num> */
static double num_field(const char *json, const char *key){
    char pat[64]; snprintf(pat,sizeof pat,"\"%s\":",key);
    const char *p = strstr(json, pat);
    if (!p) return -1;
    return atof(p + strlen(pat));
}

int main(void){
    DocSession *s = doc_session_create();
    if (!s){ fprintf(stderr,"session alloc failed\n"); return 1; }

    /* ingest a markdown doc through the agent itself */
    char cmd[512];
    snprintf(cmd,sizeof cmd,
        "{\"cmd\":\"ingest\",\"type\":\"md\",\"text\":\"# Title\\n\\npara one\\n\\n## Section\\n\\nbody text with needle here\\n\"}");
    char *r = doc_agent_handle(s, cmd);
    ck(r && strstr(r,"id"), "ingest returns id");
    long id = (long)num_field(r, "id");
    free(r);

    /* find: grounded matches with line numbers */
    snprintf(cmd,sizeof cmd,
        "{\"cmd\":\"find\",\"id\":%ld,\"query\":\"needle\"}", id);
    r = doc_agent_handle(s, cmd);
    ck(r && strstr(r,"matches"), "find returns matches");
    ck(num_field(r,"count") >= 1, "find count>=1");
    ck(strstr(r,"context"), "find includes context");
    free(r);

    snprintf(cmd,sizeof cmd,
        "{\"cmd\":\"find\",\"id\":%ld,\"query\":\"absent-token-xyz\"}", id);
    r = doc_agent_handle(s, cmd);
    ck(r && num_field(r,"count")==0, "find absent -> count 0");
    free(r);

    /* structure: outline of headings */
    snprintf(cmd,sizeof cmd, "{\"cmd\":\"structure\",\"id\":%ld}", id);
    r = doc_agent_handle(s, cmd);
    ck(r && strstr(r,"outline"), "structure returns outline");
    free(r);

    /* unknown verb still errors cleanly (protocol stability) */
    r = doc_agent_handle(s, "{\"cmd\":\"teleport\"}");
    ck(r && strstr(r,"unknown command"), "unknown verb -> clean error");
    free(r);

    doc_session_free(s);
    fprintf(stderr, bad ? "AGENT_VERBS FAIL\n" : "AGENT_VERBS PASS\n");
    return bad ? 1 : 0;
}
