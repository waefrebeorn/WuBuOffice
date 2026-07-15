/* wubuocr.c -- image -> structured document facade. */
#include "wubuocr.h"
#include "json.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    OcrBlock  box;
    OcrBlock *glyph;     /* glyph boxes in reading order */
    char    **gtext;     /* recognized text per glyph (may be NULL entries) */
    size_t    nglyph;
} PageBlock;

struct OcrPage {
    size_t     w, h;
    uint8_t    threshold;
    PageBlock *blk;
    size_t     nblk;
};

static void page_block_free(PageBlock *pb) {
    if (pb->gtext) {
        for (size_t i = 0; i < pb->nglyph; i++) free(pb->gtext[i]);
        free(pb->gtext);
    }
    free(pb->glyph);
}

void ocr_page_free(OcrPage *pg) {
    if (!pg) return;
    for (size_t i = 0; i < pg->nblk; i++) page_block_free(&pg->blk[i]);
    free(pg->blk);
    free(pg);
}

OcrPage *ocr_page_analyze(const OcrImage *im, const OcrLayoutParams *params,
                          OcrRecognizer rec, void *user) {
    if (!im) return NULL;
    uint8_t th = ocr_otsu_threshold(im);
    OcrBinary *bin = ocr_binarize(im, th);
    if (!bin) return NULL;

    OcrLayout *lay = ocr_layout(bin, params);
    if (!lay) { ocr_binary_free(bin); return NULL; }

    OcrPage *pg = calloc(1, sizeof *pg);
    if (!pg) { ocr_layout_free(lay); ocr_binary_free(bin); return NULL; }
    pg->w = ocr_image_width(im);
    pg->h = ocr_image_height(im);
    pg->threshold = th;

    size_t nb = ocr_layout_count(lay);
    pg->blk = nb ? calloc(nb, sizeof *pg->blk) : NULL;
    if (nb && !pg->blk) { free(pg); ocr_layout_free(lay); ocr_binary_free(bin); return NULL; }
    pg->nblk = nb;

    for (size_t i = 0; i < nb; i++) {
        const OcrBlock *b = ocr_layout_block(lay, i);
        PageBlock *pb = &pg->blk[i];
        pb->box = *b;

        OcrComponents *cc = ocr_components_in_block(bin, b, 1);
        if (cc) {
            size_t g = ocr_components_count(cc);
            if (g) {
                pb->glyph = malloc(g * sizeof *pb->glyph);
                pb->gtext = calloc(g, sizeof *pb->gtext);
                if (pb->glyph && pb->gtext) {
                    pb->nglyph = g;
                    for (size_t k = 0; k < g; k++) {
                        pb->glyph[k] = *ocr_components_box(cc, k);
                        pb->gtext[k] = rec ? rec(bin, &pb->glyph[k], user) : NULL;
                    }
                } else {
                    free(pb->glyph); pb->glyph = NULL;
                    free(pb->gtext); pb->gtext = NULL;
                }
            }
            ocr_components_free(cc);
        }
    }

    ocr_layout_free(lay);
    ocr_binary_free(bin);
    return pg;
}

OcrPage *ocr_page_from_netpbm(const uint8_t *data, size_t len,
                              OcrRecognizer rec, void *user) {
    OcrImage *im = ocr_image_from_netpbm(data, len);
    if (!im) return NULL;
    OcrPage *pg = ocr_page_analyze(im, NULL, rec, user);
    ocr_image_free(im);
    return pg;
}

size_t ocr_page_block_count(const OcrPage *pg) { return pg ? pg->nblk : 0; }

const OcrBlock *ocr_page_block(const OcrPage *pg, size_t i) {
    if (!pg || i >= pg->nblk) return NULL;
    return &pg->blk[i].box;
}

/* Concatenate a block's glyph text (skipping NULL entries). Caller frees. */
static char *block_text(const PageBlock *pb) {
    size_t total = 1;
    for (size_t k = 0; k < pb->nglyph; k++)
        if (pb->gtext[k]) total += strlen(pb->gtext[k]);
    char *s = malloc(total);
    if (!s) return NULL;
    s[0] = '\0';
    char *w = s;
    for (size_t k = 0; k < pb->nglyph; k++) {
        if (pb->gtext[k]) {
            size_t l = strlen(pb->gtext[k]);
            memcpy(w, pb->gtext[k], l);
            w += l;
        }
    }
    *w = '\0';
    return s;
}

static void put_box(JVal *o, const OcrBlock *b) {
    j_obj_put(o, "x", j_num((double)b->x0));
    j_obj_put(o, "y", j_num((double)b->y0));
    j_obj_put(o, "w", j_num((double)(b->x1 - b->x0)));
    j_obj_put(o, "h", j_num((double)(b->y1 - b->y0)));
}

char *ocr_page_to_json(const OcrPage *pg) {
    if (!pg) return NULL;
    JVal *root = j_obj();
    j_obj_put(root, "type", j_str("ocr_page"));
    j_obj_put(root, "width", j_num((double)pg->w));
    j_obj_put(root, "height", j_num((double)pg->h));
    j_obj_put(root, "threshold", j_num((double)pg->threshold));
    JVal *blocks = j_arr();
    for (size_t i = 0; i < pg->nblk; i++) {
        PageBlock *pb = &pg->blk[i];
        JVal *bo = j_obj();
        put_box(bo, &pb->box);
        j_obj_put(bo, "order", j_num((double)i));
        JVal *glyphs = j_arr();
        for (size_t k = 0; k < pb->nglyph; k++) {
            JVal *go = j_obj();
            put_box(go, &pb->glyph[k]);
            j_obj_put(go, "text", j_str(pb->gtext[k] ? pb->gtext[k] : ""));
            j_arr_push(glyphs, go);
        }
        j_obj_put(bo, "glyphs", glyphs);
        char *bt = block_text(pb);
        j_obj_put(bo, "text", j_str(bt ? bt : ""));
        free(bt);
        j_arr_push(blocks, bo);
    }
    j_obj_put(root, "blocks", blocks);
    char *out = j_emit(root);
    j_free(root);
    return out;
}

char *ocr_page_to_docmodel_json(const OcrPage *pg) {
    if (!pg) return NULL;
    JVal *root = j_obj();
    j_obj_put(root, "type", j_str("document"));
    JVal *blocks = j_arr();
    for (size_t i = 0; i < pg->nblk; i++) {
        char *bt = block_text(&pg->blk[i]);
        JVal *para = j_obj();
        j_obj_put(para, "kind", j_str("paragraph"));
        j_obj_put(para, "style", j_null());
        j_obj_put(para, "bold", j_bool(0));
        j_obj_put(para, "text", j_str(bt ? bt : ""));
        free(bt);
        j_arr_push(blocks, para);
    }
    j_obj_put(root, "blocks", blocks);
    char *out = j_emit(root);
    j_free(root);
    return out;
}
