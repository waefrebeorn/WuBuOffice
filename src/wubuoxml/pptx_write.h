/* pptx_write.h -- H13: minimal .pptx assembly from the slide model. */
#ifndef WUBUOXML_PPTX_WRITE_H
#define WUBUOXML_PPTX_WRITE_H

/* Assemble a one-slide .pptx at `out`. bullets/nbullets optional (pass
 * NULL/0 for title-only). Returns 0 on success. */
int wubuoxml_pptx_write(const char *out, const char *title,
                        const char **bullets, int nbullets);

#endif /* WUBUOXML_PPTX_WRITE_H */
