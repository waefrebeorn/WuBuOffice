/* conv_map.c -- unified format conversion. See conv_map.h.
 * Clean-room C11. Bridges every supported format through three models:
 *   TEXT  : dm_doc        (docx / md / rtf / html / odt)
 *   SHEET : wubucell_book (xlsx / csv / tsv / ods)
 *   SHOW  : wubushow_pres (pptx / odp)
 * CSV/JSON are SHEET/TEXT/SHOW dumps that can target any model.
 *
 * Reader functions build the model; writer functions emit a format. The
 * converter picks a reader by the input extension and a writer by the output
 * extension and, when families differ, bridges through the natural mapping
 * (sheet -> doc as one table; show -> doc as title + bullet body). */

#include "conv_map.h"
#include "conv_bridge.h"
#include "model_from_json.h"

#include "json.h"   /* wubujson (src/wubujson) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>   /* write, close, mkstemp */

/* family classification (shared by wubuconv_convert and the in-memory path) */
enum { NONE, TEXT, SHEET, SHOW };

#include "../wubuedit/docmodel.h"
#include "../wubucell/cell.h"
#include "../wubucell/cell_read.h"
#include "../wubucell/cell_csv.h"
#include "../wubushow/show.h"
#include "../wubushow/show_read.h"
#include "../wubuword/word.h"
#include "../wubuword/assemble.h"
#include "../wubudoc/doc_md.h"
#include "../wubudoc/doc_rtf.h"
#include "../wubudoc/model_json.h"
#include "../wubudoc/epub.h"
#include "../wubuodf/odf.h"
#include "../wubulegacy/legacy.h"
#include "../wubupdf/pdf.h"
#include "../wuburead/readers.h"
#include "../wubuoxml/reader.h"
#include "../wubuxml/parser.h"

static char *extof(const char *path) {
    const char *dot = strrchr(path, '.');
    return strdup(dot ? dot + 1 : "");
}
static int ext_is(const char *ext, const char *w) { return strcasecmp(ext, w) == 0; }

static uint8_t *slurp(const char *p, size_t *o) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(s ? (size_t)s : 1);
    if (fread(d, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(d); return NULL; }
    fclose(f); *o = (size_t)s; return d;
}

/* forward declaration for the docx writer (reuses wubuword). md/html serialize
 * via wubudoc_write_md / wubudoc_write_html (apps/wubudoc) -- single source,
 * no second copy of that logic here. */
static int dm_doc_write_docx(const dm_doc *d, const char *out);

/* ---------- TEXT model (dm_doc) ---------- */

/* read a .md into a dm_doc via the proven md->docx->model pivot */
static int read_md_text(const char *in, dm_doc *out) {
    wubuword_doc *wd = NULL;
    if (wubudoc_read_md(in, &wd) != 0) return -1;
    size_t len = 0; char *xml = wubuword_render(wd, &len);
    wubuword_free(wd);
    int rc = wubuedit_docmodel_parse((const uint8_t *)xml, len, out);
    free(xml);
    return rc;
}

static int read_text(const char *in, const char *ext, dm_doc *out) {
    if (ext_is(ext, "docx")) {
        size_t sz = 0; uint8_t *d = slurp(in, &sz); if (!d) return -1;
        wubuoxml_package pkg; if (wubuoxml_read(d, sz, &pkg) != 0) { free(d); return -1; }
        const wubuoxml_part *dp = wubuoxml_part_find(&pkg, "word/document.xml");
        int rc = dp ? wubuedit_docmodel_parse(dp->bytes, dp->len, out) : -1;
        wubuoxml_free(&pkg); free(d); return rc;
    }
    if (ext_is(ext, "md"))  return read_md_text(in, out);
    if (ext_is(ext, "odt")) return wubuodf_read_odt(in, out);
    if (ext_is(ext, "fodt")) return wubuodf_read_fodt(in, out);
    if (ext_is(ext, "doc"))  return wubulegacy_read_doc(in, out);
    if (ext_is(ext, "rtf"))  return wuburead_rtf(in, out);
    if (ext_is(ext, "html")) return wuburead_html(in, out);
    if (ext_is(ext, "epub")) return wuburead_epub(in, out);
    return -1;
}

/* ---------- SHEET model (wubucell_book) ---------- */

static int read_sheet(const char *in, const char *ext, wubucell_book **out) {
    if (ext_is(ext, "xlsx")) return wubucell_read(in, out);
    if (ext_is(ext, "csv") || ext_is(ext, "tsv")) {
        char delim = ext_is(ext, "tsv") ? '\t' : ',';
        return wubucell_read_csv(in, delim, out);
    }
    if (ext_is(ext, "ods"))  return wubuodf_read_ods(in, out);
    if (ext_is(ext, "fods")) return wubuodf_read_fods(in, out);
    if (ext_is(ext, "xls"))  return wubulegacy_read_xls(in, out);
    return -1;
}

/* ---------- SHOW model (wubushow_pres) ---------- */

static int read_show(const char *in, const char *ext, wubushow_pres **out) {
    if (ext_is(ext, "pptx")) return wubushow_read(in, out);
    if (ext_is(ext, "odp"))  return wubuodf_read_odp(in, out);
    if (ext_is(ext, "fodp")) return wubuodf_read_fodp(in, out);
    if (ext_is(ext, "ppt"))  return wubulegacy_read_ppt(in, out);
    return -1;
}

/* ============================================================
 *  WRITE side: model -> out_path
 * ============================================================ */

/* TEXT writers */
static int write_text(const char *out, const char *ext, dm_doc *d) {
    if (ext_is(ext, "docx")) return dm_doc_write_docx(d, out);
    if (ext_is(ext, "md"))   return wubudoc_write_md(d, out);
    if (ext_is(ext, "html")) return wubudoc_write_html(d, out);
    if (ext_is(ext, "rtf"))  return wubudoc_write_rtf(d, out);
    if (ext_is(ext, "odt"))  return wubuodf_write_odt(d, out);
    if (ext_is(ext, "fodt")) return wubuodf_write_fodt(d, out);
    if (ext_is(ext, "pdf"))  return wubupdf_write(d, out);
    if (ext_is(ext, "epub")) return wubudoc_write_epub(d, out);
    if (ext_is(ext, "doc"))  return wubulegacy_write_doc(d, out);
    if (ext_is(ext, "json")) return wubudoc_write_doc_json(d, out);
    return -1;
}

/* SHEET writers */
static int write_sheet(const char *out, const char *ext, wubucell_book *b) {
    if (ext_is(ext, "xlsx")) return wubucell_assemble(b, out);
    if (ext_is(ext, "csv") || ext_is(ext, "tsv")) {
        char delim = ext_is(ext, "tsv") ? '\t' : ',';
        return wubucell_write_csv(b, 1, delim, out);
    }
    if (ext_is(ext, "ods"))  return wubuodf_write_ods(b, out);
    if (ext_is(ext, "fods")) return wubuodf_write_fods(b, out);
    if (ext_is(ext, "xls"))  return wubulegacy_write_xls(b, out);
    if (ext_is(ext, "json")) return wubudoc_write_book_json(b, out);
    return -1;
}

/* SHOW writers */
static int write_show(const char *out, const char *ext, wubushow_pres *p) {
    if (ext_is(ext, "pptx")) return wubushow_assemble(p, out);
    if (ext_is(ext, "odp"))  return wubuodf_write_odp(p, out);
    if (ext_is(ext, "fodp")) return wubuodf_write_fodp(p, out);
    if (ext_is(ext, "ppt"))  return wubulegacy_write_ppt(p, out);
    if (ext_is(ext, "json")) return wubudoc_write_pres_json(p, out);
    return -1;
}

/* ---------- bridges between families ---------- */

/* Cross-family transforms (sheet<->text, show<->text) live in conv_bridge.c,
 * their own cohesive module, so this file stays a thin read/dispatch/write
 * layer. */

/* ============================================================
 *  dm_doc -> docx renderer (reuses wubuword)
 * ============================================================ */

static int dm_doc_write_docx(const dm_doc *d, const char *out) {
    wubuword_doc *wd = wubuword_create();
    for (size_t i = 0; i < d->n; i++) {
        dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            wubuword_para(wd, b->para.style, b->para.bold, b->para.text);
        } else {
            wubuword_table_begin(wd);
            for (size_t r = 0; r < b->table.rows; r++) {
                wubuword_row(wd);
                for (size_t c = 0; c < b->table.cols; c++) {
                    dm_para *cc = b->table.cells[r * b->table.cols + c];
                    wubuword_cell(wd, cc ? cc->bold : 0, cc ? cc->text : "");
                }
            }
            wubuword_table_end(wd);
        }
    }
    size_t len = 0; char *xml = wubuword_render(wd, &len);
    int rc = wubuword_assemble(out, xml, len);
    free(xml); wubuword_free(wd);
    return rc;
}

/* md/html serialize via wubudoc_write_md / wubudoc_write_html (apps/wubudoc) --
 * single source of truth, no second copy of that logic in this file. */

/* ============================================================
 *  Top-level convert
 * ============================================================ */

int wubuconv_convert(const char *in_path, const char *out_path) {
    char *inext = extof(in_path);
    char *outext = extof(out_path);

    /* classify families */
    int infam = NONE, outfam = NONE;
    if (ext_is(inext, "docx") || ext_is(inext, "md") || ext_is(inext, "rtf") ||
        ext_is(inext, "html") || ext_is(inext, "epub") || ext_is(inext, "odt") ||
        ext_is(inext, "fodt") || ext_is(inext, "doc")) infam = TEXT;
    else if (ext_is(inext, "xlsx") || ext_is(inext, "csv") || ext_is(inext, "tsv") ||
             ext_is(inext, "ods") || ext_is(inext, "fods") || ext_is(inext, "xls")) infam = SHEET;
    else if (ext_is(inext, "pptx") || ext_is(inext, "odp") || ext_is(inext, "fodp") ||
             ext_is(inext, "ppt")) infam = SHOW;

    if (ext_is(outext, "docx") || ext_is(outext, "md") || ext_is(outext, "html") ||
        ext_is(outext, "rtf") || ext_is(outext, "odt") || ext_is(outext, "fodt") ||
        ext_is(outext, "pdf") || ext_is(outext, "epub") || ext_is(outext, "doc") ||
        ext_is(outext, "json")) {
        /* json is emitted in the input family's model (doc/book/pres) */
        if (ext_is(outext, "json")) outfam = infam;
        else outfam = TEXT;
    } else if (ext_is(outext, "xlsx") || ext_is(outext, "csv") || ext_is(outext, "tsv") ||
               ext_is(outext, "ods") || ext_is(outext, "fods") || ext_is(outext, "xls")) outfam = SHEET;
    else if (ext_is(outext, "pptx") || ext_is(outext, "odp") || ext_is(outext, "fodp") ||
             ext_is(outext, "ppt")) outfam = SHOW;
    else outfam = NONE;

    int rc = -1;

    if (infam == TEXT && outfam == TEXT) {
        dm_doc d; memset(&d, 0, sizeof d);
        if (read_text(in_path, inext, &d) == 0) {
            if (ext_is(outext, "json")) rc = wubudoc_write_doc_json(&d, out_path);
            else rc = write_text(out_path, outext, &d);
        }
        wubuedit_docmodel_free(&d);
    }
    else if (infam == SHEET && outfam == SHEET) {
        wubucell_book *b = NULL;
        if (read_sheet(in_path, inext, &b) == 0) {
            if (ext_is(outext, "json")) rc = wubudoc_write_book_json(b, out_path);
            else rc = write_sheet(out_path, outext, b);
        }
        wubucell_free(b);
    }
    else if (infam == SHOW && outfam == SHOW) {
        wubushow_pres *p = NULL;
        if (read_show(in_path, inext, &p) == 0) {
            if (ext_is(outext, "json")) rc = wubudoc_write_pres_json(p, out_path);
            else rc = write_show(out_path, outext, p);
        }
        wubushow_free(p);
    }
    /* cross-family bridges */
    else if (infam == SHEET && outfam == TEXT) {
        wubucell_book *b = NULL;
        if (read_sheet(in_path, inext, &b) == 0) {
            dm_doc d; wubuconv_sheet_to_text(b, &d);
            if (ext_is(outext, "json")) rc = wubudoc_write_doc_json(&d, out_path);
            else rc = write_text(out_path, outext, &d);
            wubuedit_docmodel_free(&d);
        }
        wubucell_free(b);
    }
    else if (infam == SHOW && outfam == TEXT) {
        wubushow_pres *p = NULL;
        if (read_show(in_path, inext, &p) == 0) {
            dm_doc d; wubuconv_show_to_text(p, &d);
            if (ext_is(outext, "json")) rc = wubudoc_write_doc_json(&d, out_path);
            else rc = write_text(out_path, outext, &d);
            wubuedit_docmodel_free(&d);
        }
        wubushow_free(p);
    }
    else if (infam == TEXT && outfam == SHEET) {
        dm_doc d; memset(&d, 0, sizeof d);
        if (read_text(in_path, inext, &d) == 0) {
            wubucell_book *b = wubucell_create();
            wubuconv_text_to_sheet(&d, b);
            rc = write_sheet(out_path, outext, b);
            wubucell_free(b);
        }
        wubuedit_docmodel_free(&d);
    }
    else if (infam == TEXT && outfam == SHOW) {
        dm_doc d; memset(&d, 0, sizeof d);
        if (read_text(in_path, inext, &d) == 0) {
            wubushow_pres *p = wubushow_create();
            wubuconv_text_to_show(&d, p);
            if (ext_is(outext, "json")) rc = wubudoc_write_pres_json(p, out_path);
            else rc = write_show(out_path, outext, p);
            wubushow_free(p);
        }
        wubuedit_docmodel_free(&d);
    }
    else if (infam == SHEET && outfam == SHOW) {
        wubucell_book *b = NULL;
        if (read_sheet(in_path, inext, &b) == 0) {
            dm_doc d; wubuconv_sheet_to_text(b, &d);
            wubushow_pres *p = wubushow_create();
            wubuconv_text_to_show(&d, p);
            if (ext_is(outext, "json")) rc = wubudoc_write_pres_json(p, out_path);
            else rc = write_show(out_path, outext, p);
            wubushow_free(p);
            wubuedit_docmodel_free(&d);
        }
        wubucell_free(b);
    }
    else if (infam == SHOW && outfam == SHEET) {
        wubushow_pres *p = NULL;
        if (read_show(in_path, inext, &p) == 0) {
            dm_doc d; wubuconv_show_to_text(p, &d);
            wubucell_book *b = wubucell_create();
            wubuconv_text_to_sheet(&d, b);
            if (ext_is(outext, "json")) rc = wubudoc_write_book_json(b, out_path);
            else rc = write_sheet(out_path, outext, b);
            wubucell_free(b);
            wubuedit_docmodel_free(&d);
        }
        wubushow_free(p);
    }
    else {
        fprintf(stderr, "wubuconv: unsupported conversion %s -> %s\n", inext, outext);
    }

    free(inext); free(outext);
    return rc;
}

/* ---------- in-memory conversion (pathless bytes) ----------
 * Used by the wubudoc agent so the normalized model can be edited and
 * re-created without round-tripping through the filesystem. inext and outext
 * are the synthetic format tags. Returns 0 on success and sets out and
 * out_len to a malloc'd blob (caller frees); non-zero on failure. */
static int classify(const char *ext, int *fam_out, int as_output) {
    int fam = NONE;
    if (ext_is(ext, "docx") || ext_is(ext, "md") || ext_is(ext, "rtf") ||
        ext_is(ext, "html") || ext_is(ext, "epub") || ext_is(ext, "odt") ||
        ext_is(ext, "fodt") || ext_is(ext, "doc") ||
        (as_output == 0 && ext_is(ext, "json"))) fam = TEXT;
    else if (ext_is(ext, "xlsx") || ext_is(ext, "csv") || ext_is(ext, "tsv") ||
             ext_is(ext, "ods") || ext_is(ext, "fods") || ext_is(ext, "xls") ||
             (as_output == 0 && ext_is(ext, "json"))) fam = SHEET;
    else if (ext_is(ext, "pptx") || ext_is(ext, "odp") || ext_is(ext, "fodp") ||
             ext_is(ext, "ppt") ||
             (as_output == 0 && ext_is(ext, "json"))) fam = SHOW;
    *fam_out = fam;
    return fam != NONE ? 0 : -1;
}

/* Build a model from in-memory bytes of a known family. For "json" the bytes
 * ARE the model-JSON (parsed by model_from_json). For other inputs the bytes
 * are the raw file (parsed by the per-format readers, which take a path, so we
 * spill to a temp file — the readers are path-based). */
static int model_from_bytes(const uint8_t *data, size_t len, const char *inext,
                            dm_doc **outd, wubucell_book **outb, wubushow_pres **outp) {
    if (ext_is(inext, "json")) {
        char *js = malloc(len + 1); if (!js) return -1;
        memcpy(js, data, len); js[len] = '\0';
        /* peek type to decide which parser */
        const char *end = NULL;
        JVal *r = j_parse(js, &end);
        const JVal *t = r ? j_obj_get(r, "type") : NULL;
        const char *ts = (t && j_type(t) == J_STR) ? j_as_str(t) : "";
        if (strcmp(ts, "workbook") == 0) { *outb = wubuconv_book_from_json(js); }
        else if (strcmp(ts, "presentation") == 0) { *outp = wubuconv_pres_from_json(js); }
        else { *outd = wubuconv_doc_from_json(js); }
        j_free(r); free(js);
        return (*outd || *outb || *outp) ? 0 : -1;
    }
    /* non-json: spill to a temp file, use the path-based readers */
    char tmpl[] = "/tmp/wubuconv_XXXXXX";
    int fd = mkstemp(tmpl); if (fd < 0) return -1;
    if (write(fd, data, len) != (ssize_t)len) { close(fd); return -1; }
    close(fd);
    int rc = -1;
    if (ext_is(inext, "docx") || ext_is(inext, "md") || ext_is(inext, "rtf") ||
        ext_is(inext, "html") || ext_is(inext, "epub") || ext_is(inext, "odt") ||
        ext_is(inext, "fodt") || ext_is(inext, "doc")) {
        dm_doc d; memset(&d, 0, sizeof d);
        if (read_text(tmpl, inext, &d) == 0) {
            *outd = malloc(sizeof(dm_doc));
            if (*outd) { **outd = d; rc = 0; }
        }
    } else if (ext_is(inext, "xlsx") || ext_is(inext, "csv") || ext_is(inext, "tsv") ||
               ext_is(inext, "ods") || ext_is(inext, "fods") || ext_is(inext, "xls")) {
        wubucell_book *b = NULL;
        if (read_sheet(tmpl, inext, &b) == 0) { *outb = b; rc = 0; }
    } else if (ext_is(inext, "pptx") || ext_is(inext, "odp") || ext_is(inext, "fodp") ||
               ext_is(inext, "ppt")) {
        wubushow_pres *p = NULL;
        if (read_show(tmpl, inext, &p) == 0) { *outp = p; rc = 0; }
    }
    remove(tmpl);
    return rc;
}

int wubuconv_convert_mem(const uint8_t *data, size_t len,
                         const char *inext, const char *outext,
                         uint8_t **out, size_t *out_len) {
    int infam = NONE, outfam = NONE;
    if (classify(inext, &infam, 0) != 0) return -1;
    if (ext_is(outext, "json")) outfam = infam;   /* json = model serialization */
    else if (classify(outext, &outfam, 1) != 0) return -1;

    dm_doc *d = NULL; wubucell_book *b = NULL; wubushow_pres *p = NULL;
    if (model_from_bytes(data, len, inext, &d, &b, &p) != 0) return -1;

    /* pick which model we actually have */
    if (!d && !b && !p) return -1;
    /* For json input the family is ambiguous by extension (classify() picks the
     * first branch, TEXT); the real family is whichever model model_from_bytes
     * actually built by peeking the "type" field. Trust that. */
    if (ext_is(inext, "json"))
        infam = d ? TEXT : (b ? SHEET : SHOW);
    if (infam == TEXT && !d) return -1;
    if (infam == SHEET && !b) return -1;
    if (infam == SHOW && !p) return -1;

    /* write to a temp file then read back (writers are path-based) */
    int rc = -1;
    char tmpl[] = "/tmp/wubuconv_o_XXXXXX";
    int fd = mkstemp(tmpl); if (fd < 0) { goto cleanup; }
    close(fd);

    if (outfam == TEXT) {
        rc = (ext_is(outext, "json")) ? wubudoc_write_doc_json(d, tmpl)
                                      : write_text(tmpl, outext, d);
    } else if (outfam == SHEET) {
        rc = (ext_is(outext, "json")) ? wubudoc_write_book_json(b, tmpl)
                                      : write_sheet(tmpl, outext, b);
    } else if (outfam == SHOW) {
        rc = (ext_is(outext, "json")) ? wubudoc_write_pres_json(p, tmpl)
                                       : write_show(tmpl, outext, p);
    }
    if (rc == 0) {
        FILE *f = fopen(tmpl, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
            uint8_t *blob = malloc((size_t)sz + 1);
            size_t rd = fread(blob, 1, (size_t)sz, f); fclose(f);
            *out = blob; *out_len = rd; rc = 0;
        } else rc = -1;
    }
    remove(tmpl);
cleanup:
    if (d) { wubuedit_docmodel_free(d); free(d); }
    if (b) wubucell_free(b);
    if (p) wubushow_free(p);
    return rc;
}
