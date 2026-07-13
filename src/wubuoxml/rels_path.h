#ifndef WUBUOXML_RELS_PATH_H
#define WUBUOXML_RELS_PATH_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compute the .rels part path for a source part. source == "" (or NULL) maps
 * to "_rels/.rels" (the package root relationships). The result is
 * heap-allocated and owned by the caller. Shared by the OPC writer and reader
 * so both agree on the relationship graph layout. */
char *wubuoxml_rels_path_for(const char *source);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOXML_RELS_PATH_H */
