#include "wubudoc_internal.h"

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
/* Lazily build the multi-font OCR recognizer bank (used for image->document).
 * Returns the session bank, or NULL if no real font is available. */
static struct OcrFontBank *session_ocr_bank(DocSession *s) {
    if (!s->ocr_bank) s->ocr_bank = img2doc_default_bank();
    return s->ocr_bank;
}

/* Ingest already-read bytes, detecting the kind from content (not filename).
 * Returns a handle id or -1. PDFs are parsed for their text; PNGs are OCR'd
 * into a document model -- both satisfy "drag an image/PDF in, treat it as a
 * document". */
long doc_open_mem(DocSession *s, const uint8_t *data, size_t len) {
    if (!s || !data) return -1;
    const char *kind = sniff_kind(data, len);
    if (!kind) return -1;

    if (!strcmp(kind, "pdf")) {
        char *txt = pdf_extract_text(data, len);
        if (!txt) return -1;
        long id = doc_ingest_text(s, "txt", txt);  /* store extracted text */
        free(txt);
        return id;
    }
    if (!strcmp(kind, "png")) {
        struct OcrFontBank *bank = session_ocr_bank(s);
        char *doc = img2doc_recognize_png(data, len, bank);
        if (!doc) return -1;
        /* ingest the OCR'd document model as a JSON doc */
        long id = doc_ingest_text(s, "json", doc);
        free(doc);
        return id;
    }

    if (!strcmp(kind, "zip")) kind = classify_zip(data, len);
    return ingest(s, kind, "(memory)", data, len);
}
/* Decide whether dropped/pasted bytes are a file PATH (terminals often paste
 * the dropped file's path) or the file CONTENTS. If a path that exists on
 * disk, open it by path (extension-based); otherwise ingest the bytes. */
long doc_open_auto(DocSession *s, const uint8_t *data, size_t len) {
    if (!s || !data || len == 0) return -1;
    /* a path: NUL-free, contains a '/' or ends in a known extension, no
     * control chars except a single trailing newline. */
    int pathlike = 1; size_t end = len;
    while (end > 0 && (data[end-1]=='\n' || data[end-1]=='\r' || data[end-1]==' '))
        end--;
    if (end == 0) pathlike = 0;
    for (size_t i = 0; i < end; i++) {
        uint8_t c = data[i];
        if (c == 0 || c == '\t') { pathlike = 0; break; }
        if (c < 0x20) { pathlike = 0; break; }
    }
    if (pathlike && end < 4096) {
        char *p = malloc(end + 1);
        memcpy(p, data, end); p[end] = '\0';
        /* a path has a slash or a dot-extension and the file exists */
        int ispath = (strchr(p, '/') != NULL);
        struct stat stbuf;
        if (ispath && stat(p, &stbuf) == 0) {
            long id = doc_open(s, p);
            free(p);
            if (id >= 0) return id;
        }
        free(p);
    }
    return doc_open_mem(s, data, len);
}
