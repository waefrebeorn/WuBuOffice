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

#endif /* WUBUOXML_PPTX_WRITE_H */
