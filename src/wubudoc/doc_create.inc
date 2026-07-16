#include "wubudoc_internal.h"

/* ---------- creation ---------- */
/* Extract a base64 "bytes" field from a part object; returns malloc'd bytes. */
static uint8_t *part_bytes(const JVal *part, size_t *len) {
    const JVal *b = j_obj_get(part, "bytes");
    if (!b || j_type(b) != J_STR) return NULL;
    return b64_dec(j_as_str(b), len);
}

/* Build zip/cfb parts[] from the model (taking any attached media). Returns 0
 * on success, fills parts_out/names_out/n. Caller frees the arrays' contents
 * via the helper below. */
static int collect_parts(DocSession *s, long id, char ***names, uint8_t ***datas, size_t **lens, size_t *n) {
    DocHandle *d = get(s, id);
    if (!d) return -1;
    JVal *m = (JVal*)d->model;
    const JVal *parts = j_obj_get(m, "parts");    /* zip / OOXML / ODF */
    const JVal *streams = j_obj_get(m, "streams"); /* CFB legacy */
    size_t base = 0;
    if (parts && j_type(parts) == J_ARR) base += j_len(parts);
    if (streams && j_type(streams) == J_ARR) base += j_len(streams);
    *n = base + d->media_n;
    if (*n == 0) return -1;
    *names = malloc(*n * sizeof(char*));
    *datas = malloc(*n * sizeof(uint8_t*));
    *lens  = malloc(*n * sizeof(size_t));
    size_t k = 0;
    for (int which = 0; which < 2; which++) {
        const JVal *arr = which == 0 ? parts : streams;
        if (!arr || j_type(arr) != J_ARR) continue;
        for (size_t i = 0; i < j_len(arr); i++) {
            const JVal *p = j_arr_at(arr, i);
            const JVal *nm = j_obj_get(p, "name");
            if (!nm || j_type(nm) != J_STR) continue;
            size_t bl = 0; uint8_t *bd = part_bytes(p, &bl);
            (*names)[k] = strdup(j_as_str(nm));
            (*datas)[k] = bd ? bd : malloc(1);
            (*lens)[k] = bl;
            k++;
        }
    }
    for (size_t i = 0; i < d->media_n; i++) {
        (*names)[k] = strdup(d->media_name[i]);
        (*datas)[k] = malloc(d->media_len[i]);
        memcpy((*datas)[k], d->media_data[i], d->media_len[i]);
        (*lens)[k] = d->media_len[i];
        k++;
    }
    *n = k;
    return 0;
}

int doc_create(DocSession *s, long id, const char *format, const char *out_path) {
    size_t len = 0;
    uint8_t *blob = doc_create_bytes(s, id, format, &len);
    if (!blob) return -1;
    FILE *f = fopen(out_path, "wb");
    if (!f) { free(blob); return -1; }
    fwrite(blob, 1, len, f);
    fclose(f);
    free(blob);
    return 0;
}

uint8_t *doc_create_bytes(DocSession *s, long id, const char *format, size_t *out_len) {
    DocHandle *d = get(s, id);
    if (!d || !format || !out_len) return NULL;
    *out_len = 0;

    /* text formats: emit the raw text from the model -- but ONLY when the
     * handle actually holds raw text. md/csv/tsv/json/html can be ingested as
     * SEMANTIC models (stored under "model", no "text" field); those must fall
     * through to the semantic engine below, not return an empty text buffer. */
    if (!strcasecmp(format, "json") || !strcasecmp(format, "md") ||
        !strcasecmp(format, "csv") || !strcasecmp(format, "txt") ||
        !strcasecmp(format, "xml") || !strcasecmp(format, "html") ||
        !strcasecmp(format, "svg")) {
        const JVal *mtext = j_obj_get((JVal*)d->model, "text");
        const char *src = (mtext && j_type(mtext) == J_STR) ? j_as_str(mtext)
                                                            : (d->text ? d->text : NULL);
        if (src) {
            size_t L = strlen(src);
            uint8_t *o = malloc(L + 1);
            memcpy(o, src, L); o[L] = '\0';
            *out_len = L;
            return o;
        }
        /* no raw text -> semantic model; fall through to the engine */
    }

    /* zip: generic container of named parts (no semantic decoder) */
    if (!strcasecmp(format, "zip")) {
        char **names = NULL; uint8_t **datas = NULL; size_t *lens = NULL; size_t n = 0;
        if (collect_parts(s, id, &names, &datas, &lens, &n) != 0) return NULL;
        /* write to a temp ZIP via the wubuzip writer */
        char tmpl[] = "/tmp/wubudoc_XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd < 0) { free(names); free(datas); free(lens); return NULL; }
        FILE *zf = fdopen(fd, "wb");
        wubuzip_writer *zw = wubuzip_create(zf);
        for (size_t i = 0; i < n; i++) {
            wubuzip_add_deflated(zw, names[i], datas[i], (uint32_t)lens[i]);
            free(names[i]); free(datas[i]);
        }
        free(names); free(datas); free(lens);
        if (wubuzip_finalize(zw) != 0) { fclose(zf); return NULL; }
        fclose(zf);
        /* read it back */
        FILE *rf = fopen(tmpl, "rb");
        fseek(rf, 0, SEEK_END); long sz = ftell(rf); rewind(rf);
        uint8_t *o = malloc((size_t)sz + 1);
        size_t rd = fread(o, 1, (size_t)sz, rf); fclose(rf);
        remove(tmpl);
        *out_len = rd;
        return o;
    }

    /* semantic families: route the canonical model JSON through the engine.
     * This is where the "treat as media" depth lives -- docx/odt/rtf/html/
     * epub/md/doc map to dm_doc, xlsx/ods/xls to wubucell_book, pptx/odp/ppt
     * to wubushow_pres. The AGI edits the JSON model; we re-create the file. */
    if (!strcasecmp(format, "docx") || !strcasecmp(format, "odt") ||
        !strcasecmp(format, "fodt") || !strcasecmp(format, "rtf") ||
        !strcasecmp(format, "html") || !strcasecmp(format, "epub") ||
        !strcasecmp(format, "md") || !strcasecmp(format, "doc") ||
        !strcasecmp(format, "xlsx") || !strcasecmp(format, "ods") ||
        !strcasecmp(format, "fods") || !strcasecmp(format, "xls") ||
        !strcasecmp(format, "csv") || !strcasecmp(format, "tsv") ||
        !strcasecmp(format, "pptx") || !strcasecmp(format, "odp") ||
        !strcasecmp(format, "fodp") || !strcasecmp(format, "ppt")) {
        const JVal *model = j_obj_get((JVal*)d->model, "model");
        if (!model) return NULL;
        char *mj = j_emit(model);
        size_t mjlen = strlen(mj);
        uint8_t *out = NULL; size_t olen = 0;
        int rc = wubuconv_convert_mem((const uint8_t*)mj, mjlen, "json", format, &out, &olen);
        free(mj);
        if (rc != 0 || !out) return NULL;
        *out_len = olen;
        return out;
    }

    return NULL;
}
