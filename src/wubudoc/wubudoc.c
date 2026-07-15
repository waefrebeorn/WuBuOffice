/* wubudoc.c -- unified document ingestion + creation facade for wubuOS.
 * Self-contained C11. Reuses (never re-implements):
 *   wubujson  - JSON model + NDJSON
 *   wubusvg   - SVG ingest/regurgitate
 *   wubufont  - sfnt/TTF/OTF + woff
 *   wubuzip   - ZIP reader + writer
 *   wubucfb   - CFB reader + writer
 *   wubuxml   - XML reader/writer
 * Opaque. The AGI only ever sees the normalized JSON model. */
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

#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <stdio.h>

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
};

DocSession *doc_session_create(void) {
    DocSession *s = malloc(sizeof *s);
    if (!s) return NULL;
    s->h = NULL; s->n = s->cap = 0;
    return s;
}

static void handle_free(DocHandle *d) {
    if (!d) return;
    free(d->kind); free(d->source); free(d->text);
    j_free((JVal *)d->model);
    for (size_t i = 0; i < d->media_n; i++) { free(d->media_name[i]); free(d->media_data[i]); }
    free(d->media_name); free(d->media_data); free(d->media_len);
    memset(d, 0, sizeof *d);
}

void doc_session_free(DocSession *s) {
    if (!s) return;
    for (size_t i = 0; i < s->n; i++) handle_free(&s->h[i]);
    free(s->h);
    free(s);
}

/* ---------- base64 (RFC 4648) for binary parts in JSON ---------- */
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void b64_encode(const uint8_t *in, size_t n, char *out) {
    size_t i = 0, o = 0;
    while (i + 3 <= n) {
        uint32_t v = (in[i]<<16)|(in[i+1]<<8)|in[i+2];
        out[o++] = B64[(v>>18)&63]; out[o++] = B64[(v>>12)&63];
        out[o++] = B64[(v>>6)&63];  out[o++] = B64[v&63];
        i += 3;
    }
    size_t rem = n - i;
    if (rem == 1) {
        uint32_t v = in[i] << 16;
        out[o++] = B64[(v>>18)&63]; out[o++] = B64[(v>>12)&63];
        out[o++] = '='; out[o++] = '=';
    } else if (rem == 2) {
        uint32_t v = (in[i]<<16)|(in[i+1]<<8);
        out[o++] = B64[(v>>18)&63]; out[o++] = B64[(v>>12)&63];
        out[o++] = B64[(v>>6)&63]; out[o++] = '=';
    }
    out[o] = '\0';
}
static char *b64_of(const uint8_t *data, size_t len) {
    char *o = malloc((len + 2) / 3 * 4 + 1);
    if (!o) return NULL;
    b64_encode(data, len, o);
    return o;
}
static int b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static uint8_t *b64_dec(const char *s, size_t *out_len) {
    size_t n = strlen(s); size_t o = 0;
    uint8_t *buf = malloc(n / 4 * 3 + 3);
    if (!buf) return NULL;
    size_t i = 0;
    while (i + 4 <= n) {
        int a = b64_val(s[i]), b = b64_val(s[i+1]), c = b64_val(s[i+2]), d = b64_val(s[i+3]);
        if (a < 0 || b < 0) break;
        uint32_t v = (a<<18)|(b<<12)|((c<0?0:c)<<6)|(d<0?0:d);
        buf[o++] = (v>>16)&255;
        if (c >= 0) buf[o++] = (v>>8)&255;
        if (d >= 0) buf[o++] = v&255;
        i += 4;
    }
    *out_len = o;
    return buf;
}

/* ---------- handle registration ---------- */
static long session_add(DocSession *s, const char *kind, const char *source) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 4;
        s->h = realloc(s->h, s->cap * sizeof *s->h);
        if (!s->h) return -1;
    }
    DocHandle *d = &s->h[s->n];
    memset(d, 0, sizeof *d);
    d->kind = strdup(kind ? kind : "?");
    d->source = source ? strdup(source) : strdup("");
    long id = (long)(s->n);
    s->n++;
    return id;
}
static DocHandle *get(const DocSession *s, long id) {
    if (id < 0 || (size_t)id >= s->n) return NULL;
    return &s->h[id];
}

/* ---------- kind detection from a path/type ---------- */
static const char *kind_of_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;
    const char *e = dot + 1;
    if (!strcasecmp(e, "txt")) return "txt";
    if (!strcasecmp(e, "md"))  return "md";
    if (!strcasecmp(e, "json")) return "json";
    if (!strcasecmp(e, "csv")) return "csv";
    if (!strcasecmp(e, "tsv")) return "tsv";
    if (!strcasecmp(e, "svg")) return "svg";
    if (!strcasecmp(e, "xml")) return "xml";
    if (!strcasecmp(e, "html") || !strcasecmp(e, "htm")) return "html";
    if (!strcasecmp(e, "rtf")) return "rtf";
    if (!strcasecmp(e, "epub")) return "epub";
    if (!strcasecmp(e, "ttf") || !strcasecmp(e, "otf")) return "font";
    if (!strcasecmp(e, "woff")) return "woff";
    if (!strcasecmp(e, "zip")) return "zip";
    if (!strcasecmp(e, "docx")) return "docx";
    if (!strcasecmp(e, "xlsx")) return "xlsx";
    if (!strcasecmp(e, "pptx")) return "pptx";
    if (!strcasecmp(e, "odt"))  return "odt";
    if (!strcasecmp(e, "ods"))  return "ods";
    if (!strcasecmp(e, "odp"))  return "odp";
    if (!strcasecmp(e, "fodt")) return "fodt";
    if (!strcasecmp(e, "fods")) return "fods";
    if (!strcasecmp(e, "fodp")) return "fodp";
    if (!strcasecmp(e, "doc")) return "doc";
    if (!strcasecmp(e, "xls")) return "xls";
    if (!strcasecmp(e, "ppt")) return "ppt";
    return NULL;
}

/* ---------- container ingestors (zip) ---------- */
static JVal *ingest_zip(const uint8_t *data, size_t len) {
    wubuzip_archive z;
    if (wubuzip_open(data, len, &z) != 0) return NULL;
    JVal *arr = j_arr();
    for (size_t i = 0; i < wubuzip_count(&z); i++) {
        const char *nm = wubuzip_name(&z, i);
        uint8_t *bytes = NULL; size_t bl = 0;
        wubuzip_extract(&z, i, &bytes, &bl);
        char *b = b64_of(bytes, bl); free(bytes);
        JVal *p = j_obj();
        j_obj_put(p, "name", j_str(nm));
        j_obj_put(p, "bytes", j_str(b ? b : ""));
        free(b);
        j_arr_push(arr, p);
    }
    wubuzip_close(&z);
    return arr;
}

static JVal *ingest_font(const uint8_t *data, size_t len) {
    Font *f = font_open_owned(data, len, 1);
    if (!f) return NULL;
    JVal *o = j_obj();
    uint16_t upm = font_units_per_em(f);
    uint16_t gc  = font_glyph_count(f);
    char *fam = font_name(f, 1);
    j_obj_put(o, "unitsPerEm", j_num((double)upm));
    j_obj_put(o, "glyphCount", j_num((double)gc));
    if (fam) j_obj_put(o, "family", j_str(fam));
    free(fam);
    JVal *tbl = j_arr();
    size_t tc = font_table_count(f);
    for (size_t i = 0; i < tc; i++) {
        size_t off, ln;
        if (font_table_range(f, i, &off, &ln)) {
            uint32_t tag = font_table_tag(f, i);
            char tagbuf[5] = { (char)(tag>>24),(char)(tag>>16),(char)(tag>>8),(char)tag, 0 };
            JVal *t = j_obj();
            j_obj_put(t, "tag", j_str(tagbuf));
            j_obj_put(t, "length", j_num((double)ln));
            j_arr_push(tbl, t);
        }
    }
    j_obj_put(o, "tables", tbl);
    font_free(f);
    return o;
}

static JVal *ingest_woff(const uint8_t *data, size_t len) {
    Font *f = woff_open(data, len);
    if (!f) return NULL;
    JVal *o = j_obj();
    j_obj_put(o, "unitsPerEm", j_num((double)font_units_per_em(f)));
    j_obj_put(o, "glyphCount", j_num((double)font_glyph_count(f)));
    JVal *tbl = j_arr();
    size_t tc = font_table_count(f);
    for (size_t i = 0; i < tc; i++) {
        size_t off, ln;
        if (font_table_range(f, i, &off, &ln)) {
            uint32_t tag = font_table_tag(f, i);
            char tagbuf[5] = { (char)(tag>>24),(char)(tag>>16),(char)(tag>>8),(char)tag, 0 };
            JVal *t = j_obj();
            j_obj_put(t, "tag", j_str(tagbuf));
            j_obj_put(t, "length", j_num((double)ln));
            j_arr_push(tbl, t);
        }
    }
    j_obj_put(o, "tables", tbl);
    font_free(f);
    return o;
}

/* ---------- the master ingest ---------- */
/* Builds a normalized model (JVal*) for `kind` from bytes. Returns owned JVal*
 * or NULL. For text kinds also sets *out_text. */
static JVal *build_model(const char *kind, const uint8_t *data, size_t len,
                         char **out_text) {
    JVal *m = j_obj();
    if (!strcasecmp(kind, "txt") ||
        !strcasecmp(kind, "xml") || !strcasecmp(kind, "html") ||
        !strcasecmp(kind, "svg")) {
        char *txt = malloc(len + 1);
        memcpy(txt, data, len); txt[len] = '\0';
        if (out_text) *out_text = txt; else free(txt);
        j_obj_put(m, "text", j_str(txt));
        if (!out_text) { /* still owned by handle */ }
    }
    else if (!strcasecmp(kind, "csv") || !strcasecmp(kind, "tsv") ||
             !strcasecmp(kind, "json") ||
             !strcasecmp(kind, "docx") || !strcasecmp(kind, "md") ||
             !strcasecmp(kind, "rtf") || !strcasecmp(kind, "html") ||
             !strcasecmp(kind, "epub") || !strcasecmp(kind, "odt") ||
             !strcasecmp(kind, "fodt") || !strcasecmp(kind, "doc") ||
             !strcasecmp(kind, "xlsx") || !strcasecmp(kind, "ods") ||
             !strcasecmp(kind, "fods") || !strcasecmp(kind, "xls") ||
             !strcasecmp(kind, "pptx") || !strcasecmp(kind, "odp") ||
             !strcasecmp(kind, "fodp") || !strcasecmp(kind, "ppt")) {
        /* Treat the file as the MEDIA it is, not the bytes it contains
         * (WuBuContainer's "no binary waterfalls" rule). Route through the
         * semantic engine -> a canonical model serialized as JSON. */
        uint8_t *mj = NULL; size_t mjlen = 0;
        if (wubuconv_convert_mem(data, len, kind, "json", &mj, &mjlen) == 0) {
            char *js = malloc(mjlen + 1); memcpy(js, mj, mjlen); js[mjlen] = '\0';
            free(mj);
            JVal *model = j_parse(js, NULL);
            if (model) j_obj_put(m, "model", model);
            /* also keep a human text projection */
            const JVal *blocks = j_obj_get(model, "blocks");
            const JVal *sheets = j_obj_get(model, "sheets");
            const JVal *slides = j_obj_get(model, "slides");
            if (blocks || sheets || slides) {
                /* no separate text; the model IS the content */
            }
            free(js);
        }
        /* fall back: if the semantic engine didn't handle it, leave model null
         * and the create path will refuse (keeps the gate honest). */
    }
    else if (!strcasecmp(kind, "zip")) {
        JVal *parts = ingest_zip(data, len);
        if (!parts) { j_free(m); return NULL; }
        j_obj_put(m, "parts", parts);
    }
    else if (!strcasecmp(kind, "font")) {
        JVal *f = ingest_font(data, len);
        if (!f) { j_free(m); return NULL; }
        j_obj_put(m, "font", f);
    }
    else if (!strcasecmp(kind, "woff")) {
        JVal *f = ingest_woff(data, len);
        if (!f) { j_free(m); return NULL; }
        j_obj_put(m, "font", f);
    }
    else { j_free(m); return NULL; }
    return m;
}

static long ingest(DocSession *s, const char *kind, const char *source,
                   const uint8_t *data, size_t len) {
    char *text = NULL;
    JVal *m = build_model(kind, data, len, &text);
    if (!m) { free(text); return -1; }
    long id = session_add(s, kind, source);
    if (id < 0) { j_free(m); free(text); return -1; }
    DocHandle *d = get(s, id);
    d->model = m;
    d->text = text;   /* takes ownership */
    return id;
}

/* ---------- public ingest API ---------- */
long doc_ingest_bytes(DocSession *s, const char *type, const uint8_t *data, size_t len) {
    if (!s || !type || !data) return -1;
    return ingest(s, type, type, data, len);
}
long doc_ingest_text(DocSession *s, const char *type, const char *text) {
    if (!s || !type || !text) return -1;
    return ingest(s, type, type, (const uint8_t *)text, strlen(text));
}
long doc_open(DocSession *s, const char *path) {
    if (!s || !path) return -1;
    const char *kind = kind_of_ext(path);
    if (!kind) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f); rewind(f);
    if (n < 0) { fclose(f); return -1; }
    uint8_t *buf = malloc((size_t)n + 1);
    size_t rd = fread(buf, 1, (size_t)n, f); fclose(f);
    long id = ingest(s, kind, path, buf, rd);
    free(buf);
    return id;
}

/* ---------- accessors ---------- */
const char *doc_kind(const DocSession *s, long id) {
    DocHandle *d = get(s, id); return d ? d->kind : NULL;
}
const char *doc_source(const DocSession *s, long id) {
    DocHandle *d = get(s, id); return d ? d->source : NULL;
}
char *doc_json(const DocSession *s, long id) {
    DocHandle *d = get(s, id);
    if (!d) return NULL;
    return j_emit((JVal *)d->model);
}
const char *doc_text(const DocSession *s, long id) {
    DocHandle *d = get(s, id); return d ? d->text : NULL;
}
void doc_set_model(DocSession *s, long id, void *model) {
    DocHandle *d = get(s, id);
    if (!d) { j_free((JVal*)model); return; }
    j_free((JVal*)d->model);
    d->model = model;
}

int doc_add_media(DocSession *s, long id, const char *name, const uint8_t *data, size_t len) {
    DocHandle *d = get(s, id);
    if (!d || !name || !data) return -1;
    if (d->media_n == d->media_cap) {
        d->media_cap = d->media_cap ? d->media_cap * 2 : 4;
        d->media_name = realloc(d->media_name, d->media_cap * sizeof *d->media_name);
        d->media_data = realloc(d->media_data, d->media_cap * sizeof *d->media_data);
        d->media_len  = realloc(d->media_len,  d->media_cap * sizeof *d->media_len);
    }
    d->media_name[d->media_n] = strdup(name);
    d->media_data[d->media_n] = malloc(len);
    memcpy(d->media_data[d->media_n], data, len);
    d->media_len[d->media_n] = len;
    d->media_n++;
    return 0;
}

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

/* ---------- NDJSON agent ---------- */
static char *err(const char *msg) {
    JVal *o = j_obj(); j_obj_put(o, "error", j_str(msg));
    char *s = j_emit(o); j_free(o); return s;
}
static JVal *handle_summary(DocSession *s, long id) {
    DocHandle *d = get(s, id);
    JVal *o = j_obj();
    j_obj_put(o, "id", j_num((double)id));
    j_obj_put(o, "kind", j_str(d->kind));
    j_obj_put(o, "source", j_str(d->source));
    return o;
}

char *doc_agent_handle(DocSession *s, const char *command_json) {
    if (!s || !command_json) return err("null");
    const char *end = NULL;
    JVal *cmd = j_parse(command_json, &end);
    if (!cmd || j_type(cmd) != J_OBJ) { j_free(cmd); return err("bad json"); }
    const JVal *c = j_obj_get(cmd, "cmd");
    if (!c || j_type(c) != J_STR) { j_free(cmd); return err("missing cmd"); }
    const char *name = j_as_str(c);
    JVal *res = NULL;

    if (strcmp(name, "open") == 0) {
        const JVal *p = j_obj_get(cmd, "path");
        if (!p || j_type(p) != J_STR) { j_free(cmd); return err("open: path required"); }
        long id = doc_open(s, j_as_str(p));
        if (id < 0) { j_free(cmd); return err("open: failed (unknown type?)"); }
        res = handle_summary(s, id);
    }
    else if (strcmp(name, "ingest") == 0 || strcmp(name, "load") == 0) {
        const JVal *t = j_obj_get(cmd, "type");
        if (!t || j_type(t) != J_STR) { j_free(cmd); return err("ingest: type required"); }
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
        } else { j_free(cmd); return err("ingest: bytes or text required"); }
        if (id < 0) { j_free(cmd); return err("ingest: failed"); }
        res = handle_summary(s, id);
    }
    else if (strcmp(name, "json") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return err("json: id required"); }
        DocHandle *d = get(s, (long)j_as_num(idv));
        if (!d) { j_free(cmd); return err("json: no such id"); }
        res = j_obj();
        j_obj_put(res, "id", j_num(j_as_num(idv)));
        j_obj_put(res, "model", j_copy((JVal*)d->model));
    }
    else if (strcmp(name, "text") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return err("text: id required"); }
        DocHandle *d = get(s, (long)j_as_num(idv));
        if (!d) { j_free(cmd); return err("text: no such id"); }
        res = j_obj();
        j_obj_put(res, "id", j_num(j_as_num(idv)));
        j_obj_put(res, "text", j_str(d->text ? d->text : ""));
    }
    else if (strcmp(name, "set") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *model = j_obj_get(cmd, "model");
        if (!idv || !model) { j_free(cmd); return err("set: id,model required"); }
        DocHandle *d = get(s, (long)j_as_num(idv));
        if (!d) { j_free(cmd); return err("set: no such id"); }
        /* store an independent copy so the command's own model value can be
         * safely freed with the rest of cmd. */
        JVal *cp = j_copy(model);
        doc_set_model(s, (long)j_as_num(idv), (void*)cp);
        res = j_obj();
        j_obj_put(res, "ok", j_bool(1));
    }
    else if (strcmp(name, "media") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *nm = j_obj_get(cmd, "name");
        const JVal *bd = j_obj_get(cmd, "bytes");
        if (!idv || !nm || !bd || j_type(nm) != J_STR || j_type(bd) != J_STR) {
            j_free(cmd); return err("media: id,name,bytes required");
        }
        size_t bl = 0; uint8_t *data = b64_dec(j_as_str(bd), &bl);
        int rc = doc_add_media(s, (long)j_as_num(idv), j_as_str(nm), data, bl);
        free(data);
        if (rc != 0) { j_free(cmd); return err("media: failed"); }
        res = j_obj(); j_obj_put(res, "ok", j_bool(1));
    }
    else if (strcmp(name, "create") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *fmt = j_obj_get(cmd, "format");
        const JVal *path = j_obj_get(cmd, "path");
        if (!idv || !fmt || !path || j_type(fmt) != J_STR || j_type(path) != J_STR) {
            j_free(cmd); return err("create: id,format,path required");
        }
        size_t bl = 0;
        uint8_t *blob = doc_create_bytes(s, (long)j_as_num(idv), j_as_str(fmt), &bl);
        if (!blob) { j_free(cmd); return err("create: unsupported or empty model"); }
        FILE *f = fopen(j_as_str(path), "wb");
        if (!f) { free(blob); j_free(cmd); return err("create: cannot write path"); }
        fwrite(blob, 1, bl, f); fclose(f);
        char *b = b64_of(blob, bl); free(blob);
        res = j_obj();
        j_obj_put(res, "ok", j_bool(1));
        j_obj_put(res, "path", j_str(j_as_str(path)));
        j_obj_put(res, "bytes", j_str(b ? b : ""));
        free(b);
    }
    else if (strcmp(name, "list") == 0) {
        JVal *arr = j_arr();
        for (size_t i = 0; i < s->n; i++) {
            JVal *o = j_obj();
            j_obj_put(o, "id", j_num((double)i));
            j_obj_put(o, "kind", j_str(s->h[i].kind));
            j_obj_put(o, "source", j_str(s->h[i].source));
            j_arr_push(arr, o);
        }
        res = j_obj(); j_obj_put(res, "docs", arr);
    }
    else if (strcmp(name, "close") == 0) {
        /* sessions are append-only for simplicity; close is a no-op success */
        res = j_obj(); j_obj_put(res, "ok", j_bool(1));
    }
    else if (strcmp(name, "quit") == 0) {
        res = j_obj(); j_obj_put(res, "ok", j_bool(1));
    }
    else { j_free(cmd); return err("unknown command"); }

    /* free cmd after building res (model was copied, so no dangling ref) */
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
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
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
