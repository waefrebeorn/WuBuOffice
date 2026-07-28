/* caption.h -- figure/table captions (DOC-70): maps a stable node id to a
 * caption string. Opaque side-table. */
#ifndef WUBUCAPTION_H
#define WUBUCAPTION_H
#include <stdint.h>

typedef struct CaptionMap CaptionMap;

CaptionMap *caption_create(void);
void        caption_destroy(CaptionMap *m);
int         caption_set(CaptionMap *m, uint64_t node_id, const char *text); /* 1 ok */
const char *caption_get(const CaptionMap *m, uint64_t node_id);             /* NULL */
int         caption_count(const CaptionMap *m);
uint64_t    caption_id_at(const CaptionMap *m, int i);
const char *caption_text_at(const CaptionMap *m, int i);

#endif /* WUBUCAPTION_H */
