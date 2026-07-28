/* lang.h -- per-node language attribute (UXA-48): maps a stable node id to an
 * RFC-5646 language tag (e.g. "en", "fr"). Opaque side-table. */
#ifndef WUBULANG_H
#define WUBULANG_H
#include <stdint.h>

typedef struct LangMap LangMap;

LangMap *lang_create(void);
void     lang_destroy(LangMap *m);
int      lang_set(LangMap *m, uint64_t node_id, const char *tag);  /* 1 ok */
const char *lang_get(const LangMap *m, uint64_t node_id);          /* NULL if unset */
int      lang_count(const LangMap *m);
uint64_t lang_id_at(const LangMap *m, int i);
const char *lang_tag_at(const LangMap *m, int i);

#endif /* WUBULANG_H */
