/* fmtpaint.h -- format painter (DOC-74). Copies the FULL style prop set from
 * a source node into a held "brush", then applies it to any number of target
 * nodes (each gets a fresh style so later edits don't alias). Opaque. */
#ifndef WUBUFMTPAINT_H
#define WUBUFMTPAINT_H

typedef struct FmtPaint FmtPaint;

FmtPaint *fmtpaint_create(void);
void      fmtpaint_destroy(FmtPaint *f);

/* Pick up formatting from `src` (a wubumodel_node*). Returns the number of
 * style props copied into the brush (0 if src has no style). */
int  fmtpaint_pick(FmtPaint *f, const void *src);

/* Apply the held brush to `dst` (a wubumodel_node*): builds a fresh style
 * with the brush's props and attaches it. Returns props applied, -1 error. */
int  fmtpaint_apply(FmtPaint *f, void *dst);

/* Brush state. */
int  fmtpaint_loaded(const FmtPaint *f);       /* props held */
const char *fmtpaint_value(const FmtPaint *f, const char *name);
void fmtpaint_clear(FmtPaint *f);

#endif /* WUBUFMTPAINT_H */
