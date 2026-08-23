#include <stddef.h>
/* pptx_write.h -- H13/H17: .pptx assembly from the slide model.
 * H17 adds multi-slide support (sldIdLst with N entries, one part per
 * slide, per-slide rels). */
#ifndef WUBUOXML_PPTX_WRITE_H
#define WUBUOXML_PPTX_WRITE_H

/* Assemble a one-slide .pptx at `out`. bullets/nbullets optional (pass
 * NULL/0 for title-only). Returns 0 on success. */
int wubuoxml_pptx_write(const char *out, const char *title,
                        const char **bullets, int nbullets);

/* One slide's content (multi-slide variant). */
typedef struct {
    const char *title;
    const char **bullets;
    int nbullets;
} PptxSlide;

/* Assemble a multi-slide .pptx. nslides >= 1. Returns 0 on success. */
int wubuoxml_pptx_write_multi(const char *out, const PptxSlide *slides,
                              int nslides);

/* One slide read back from a deck. */
typedef struct {
    char title[96];
    char bullets[12][96];
    int nbullets;
} PptxSlideData;

/* Read ALL slides of `path` into slides[] (capacity maxslides).
 * Returns the slide count, or -1 on error. */
int wubuoxml_pptx_read_multi(const char *path, PptxSlideData *slides,
                             int maxslides);

/* Read the first slide of `path` back: title + up to maxb bullets.
 * Returns 0 on success. */
int wubuoxml_pptx_read(const char *path, char *title, size_t tcap,
                       char bullets[][96], int maxb, int *nbullets);

#endif /* WUBUOXML_PPTX_WRITE_H */
