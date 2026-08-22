/* agent.c -- NDJSON agent protocol for the wubudoc facade (see agent.h).
 *
 * One JSON object per line on stdin -> one JSON result per line on stdout;
 * the loop ends on EOF or {"cmd":"quit"}. The agent is a thin dispatcher: it
 * parses the command, calls the public wubudoc ingest/create accessors, and
 * emits a normalized result. It holds no document state itself.
 *
 * Self-contained C11: depends only on wubudoc.h + b64.h + wubujson. */
#define _POSIX_C_SOURCE 200809L
#include "agent.h"
#include "wubudoc.h"
#include "b64.h"

#include "json.h"   /* wubujson: j_* */

#include <stdlib.h>
#include <string.h>

/* Emit {"error":msg} as a malloc'd JSON string. */
static char *agent_err(const char *msg) {
    JVal *o = j_obj();
    j_obj_put(o, "error", j_str(msg));
    char *s = j_emit(o);
    j_free(o);
    return s;
}

/* A minimal {id,kind,source} summary for an open/ingest handle. */
static JVal *agent_summary(DocSession *s, long id) {
    JVal *o = j_obj();
    j_obj_put(o, "id", j_num((double)id));
    j_obj_put(o, "kind", j_str(doc_kind(s, id) ? doc_kind(s, id) : ""));
    j_obj_put(o, "source", j_str(doc_source(s, id) ? doc_source(s, id) : ""));
    return o;
}

char *doc_agent_handle(DocSession *s, const char *command_json) {
    if (!s || !command_json) return agent_err("null");
    const char *end = NULL;
    JVal *cmd = j_parse(command_json, &end);
    if (!cmd || j_type(cmd) != J_OBJ) { j_free(cmd); return agent_err("bad json"); }
    const JVal *c = j_obj_get(cmd, "cmd");
    if (!c || j_type(c) != J_STR) { j_free(cmd); return agent_err("missing cmd"); }
    const char *name = j_as_str(c);
    JVal *res = NULL;

    if (strcmp(name, "open") == 0) {
        const JVal *p = j_obj_get(cmd, "path");
        if (!p || j_type(p) != J_STR) { j_free(cmd); return agent_err("open: path required"); }
        long id = doc_open(s, j_as_str(p));
        if (id < 0) { j_free(cmd); return agent_err("open: failed (unknown type?)"); }
        res = agent_summary(s, id);
    }
    else if (strcmp(name, "ingest") == 0 || strcmp(name, "load") == 0) {
        const JVal *t = j_obj_get(cmd, "type");
        if (!t || j_type(t) != J_STR) { j_free(cmd); return agent_err("ingest: type required"); }
        const char *type = j_as_str(t);
        long id = -1;
        const JVal *bytes = j_obj_get(cmd, "bytes");
        const JVal *text = j_obj_get(cmd, "text");
        if (bytes && j_type(bytes) == J_STR) {
            size_t bl = 0; uint8_t *bd = b64_dec(j_as_str(bytes), &bl);
            id = doc_ingest_bytes(s, type, bd, bl);
            free(bd);
        } else if (text && j_type(text) == J_STR) {
            id = doc_ingest_text(s, type, j_as_str(text));
        } else { j_free(cmd); return agent_err("ingest: bytes or text required"); }
        if (id < 0) { j_free(cmd); return agent_err("ingest: failed"); }
        res = agent_summary(s, id);
    }
    else if (strcmp(name, "json") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return agent_err("json: id required"); }
        long id = (long)j_as_num(idv);
        char *m = doc_json(s, id);
        if (!m) { j_free(cmd); return agent_err("json: no such id"); }
        res = j_parse(m, NULL);
        free(m);
        if (!res) res = j_obj();
        j_obj_put(res, "id", j_num((double)id));
    }
    else if (strcmp(name, "text") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return agent_err("text: id required"); }
        long id = (long)j_as_num(idv);
        const char *tx = doc_text(s, id);
        res = j_obj();
        j_obj_put(res, "id", j_num((double)id));
        j_obj_put(res, "text", j_str(tx ? tx : ""));
    }
    else if (strcmp(name, "set") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *model = j_obj_get(cmd, "model");
        if (!idv || !model) { j_free(cmd); return agent_err("set: id,model required"); }
        long id = (long)j_as_num(idv);
        /* store an independent copy so the command's own model value can be
         * safely freed with the rest of cmd. */
        JVal *cp = j_copy(model);
        doc_set_model(s, id, (void *)cp);
        res = j_obj();
        j_obj_put(res, "ok", j_bool(1));
    }
    else if (strcmp(name, "media") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *nm = j_obj_get(cmd, "name");
        const JVal *bd = j_obj_get(cmd, "bytes");
        if (!idv || !nm || !bd || j_type(nm) != J_STR || j_type(bd) != J_STR) {
            j_free(cmd); return agent_err("media: id,name,bytes required");
        }
        size_t bl = 0; uint8_t *data = b64_dec(j_as_str(bd), &bl);
        int rc = doc_add_media(s, (long)j_as_num(idv), j_as_str(nm), data, bl);
        free(data);
        if (rc != 0) { j_free(cmd); return agent_err("media: failed"); }
        res = j_obj(); j_obj_put(res, "ok", j_bool(1));
    }
    else if (strcmp(name, "create") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *fmt = j_obj_get(cmd, "format");
        const JVal *path = j_obj_get(cmd, "path");
        if (!idv || !fmt || !path || j_type(fmt) != J_STR || j_type(path) != J_STR) {
            j_free(cmd); return agent_err("create: id,format,path required");
        }
        size_t bl = 0;
        uint8_t *blob = doc_create_bytes(s, (long)j_as_num(idv), j_as_str(fmt), &bl);
        if (!blob) { j_free(cmd); return agent_err("create: unsupported or empty model"); }
        FILE *f = fopen(j_as_str(path), "wb");
        if (!f) { free(blob); j_free(cmd); return agent_err("create: cannot write path"); }
        fwrite(blob, 1, bl, f); fclose(f);
        char *b = b64_of(blob, bl); free(blob);
        res = j_obj();
        j_obj_put(res, "ok", j_bool(1));
        j_obj_put(res, "path", j_str(j_as_str(path)));
        j_obj_put(res, "bytes", j_str(b ? b : ""));
        free(b);
    }
    else if (strcmp(name, "find") == 0) {
        /* find {id, query} -> matches with line + context window. Agents
         * need grounded search (not just boolean hit) to cite locations. */
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *qv  = j_obj_get(cmd, "query");
        if (!idv || !qv || j_type(qv) != J_STR) {
            j_free(cmd); return agent_err("find: id,query required");
        }
        long id = (long)j_as_num(idv);
        const char *tx = doc_text(s, id);
        char *tx_owned = NULL;
        if (!tx) {
            /* model-backed docs (md/docx/...): flatten the normalized JSON
             * model's "text" fields into one searchable buffer so agents get
             * the SAME grounded find on every kind, not just plain text. */
            char *mjson = doc_json(s, id);
            if (mjson) {
                size_t cap = strlen(mjson) + 1, len = 0;
                tx_owned = malloc(cap);
                const char *p = mjson;
                while ((p = strstr(p, "\"text\"")) != NULL) {
                    p = strchr(p + 6, '"');          /* open quote of value */
                    if (!p) break;
                    p++;
                    while (*p && *p != '"') {
                        if (*p == '\\' && p[1]) p++;  /* skip escapes */
                        else if (len + 1 < cap) tx_owned[len++] = *p;
                        p++;
                    }
                    if (len + 1 < cap) tx_owned[len++] = '\n';
                }
                tx_owned[len] = 0;
                free(mjson);
                if (len) tx = tx_owned;
            }
        }
        if (!tx) {
            free(tx_owned);
            res = j_obj();
            j_obj_put(res, "id", j_num((double)id));
            j_obj_put(res, "supported", j_bool(0));
        } else {
            JVal *arr = j_arr();
            const char *q = j_as_str(qv);
            const char *p = tx;
            int n = 0;
            while ((p = strstr(p, q)) != NULL && n < 200) {
                long line = 1;
                for (const char *c = tx; c < p; c++) if (*c == '\n') line++;
                size_t ctx0 = (size_t)(p - tx > 40 ? p - tx - 40 : 0);
                size_t rem = strlen(p);
                size_t clen = (rem < strlen(q) + 80) ? rem : strlen(q) + 80;
                char *ctx = malloc(clen + 1);
                memcpy(ctx, tx + ctx0, clen); ctx[clen] = 0;
                JVal *m = j_obj();
                j_obj_put(m, "line", j_num((double)line));
                j_obj_put(m, "offset", j_num((double)(p - tx)));
                j_obj_put(m, "context", j_str(ctx));
                free(ctx);
                j_arr_push(arr, m);
                n++;
                p += strlen(q);
            }
            res = j_obj();
            j_obj_put(res, "id", j_num((double)id));
            j_obj_put(res, "count", j_num((double)n));
            j_obj_put(res, "matches", arr);
            free(tx_owned);
        }
    }
    else if (strcmp(name, "structure") == 0) {
        /* structure {id} -> headings/tables/figures outline from the model.
         * The agent's table of contents: semantic anchors without GUI. */
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return agent_err("structure: id required"); }
        long id = (long)j_as_num(idv);
        char *mjson = doc_json(s, id);
        if (!mjson) { j_free(cmd); return agent_err("structure: no such id"); }
        JVal *model = j_parse(mjson, NULL);
        free(mjson);
        JVal *out = j_arr();
        if (model) {
            /* walk nodes collecting kind+text summary (depth-first) */
            /* iterative stack-free walk over the JSON object arrays */
            /* nodes live under {"nodes":[...]} or {"body":[...]} */
            JVal *nodes = j_obj_get(model, "nodes");
            if (!nodes) nodes = j_obj_get(model, "body");
            if (nodes && j_type(nodes) == J_ARR) {
                for (size_t i = 0; i < j_len(nodes); i++) {
                    JVal *nd = (JVal *)j_arr_at(nodes, i);
                    if (!nd || j_type(nd) != J_OBJ) continue;
                    const JVal *kind = j_obj_get(nd, "kind");
                    const JVal *txt  = j_obj_get(nd, "text");
                    if (!kind) continue;
                    const char *ks = j_as_str(kind);
                    int interesting = strstr(ks, "head") || strstr(ks, "table")
                                   || strstr(ks, "figure") || strstr(ks, "image")
                                   || strstr(ks, "caption");
                    if (!interesting) continue;
                    JVal *e = j_obj();
                    j_obj_put(e, "kind", j_str(ks));
                    j_obj_put(e, "index", j_num((double)i));
                    char sum[80] = "";
                    if (txt && j_type(txt) == J_STR) {
                        snprintf(sum, sizeof sum, "%.70s", j_as_str(txt));
                    }
                    j_obj_put(e, "summary", j_str(sum));
                    j_arr_push(out, e);
                }
            }
            j_free(model);
        }
        res = j_obj();
        j_obj_put(res, "id", j_num((double)id));
        j_obj_put(res, "outline", out);
    }
    else if (strcmp(name, "list") == 0) {
        JVal *arr = j_arr();
        for (size_t i = 0; i < doc_count(s); i++) {
            j_arr_push(arr, agent_summary(s, (long)i));
        }
        res = j_obj(); j_obj_put(res, "docs", arr);
    }
    else if (strcmp(name, "close") == 0) {
        res = j_obj(); j_obj_put(res, "ok", j_bool(1));
    }
    else if (strcmp(name, "quit") == 0) {
        res = j_obj(); j_obj_put(res, "ok", j_bool(1));
    }
    else { j_free(cmd); return agent_err("unknown command"); }

    char *s_out = j_emit(res);
    j_free(res);
    j_free(cmd);
    return s_out;
}

int doc_agent_serve(DocSession *s, FILE *in, FILE *out) {
    if (!s || !in || !out) return -1;
    char *line = NULL; size_t cap = 0; ssize_t len;
    int rc = 0;
    while ((len = getline(&line, &cap, in)) != -1) {
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        char *resp = doc_agent_handle(s, line);
        if (!resp) { rc = -1; break; }
        fputs(resp, out); fputc('\n', out); fflush(out);
        free(resp);
        if (strncmp(line, "{\"cmd\":\"quit\"", 12) == 0) break;
    }
    free(line);
    return rc;
}
