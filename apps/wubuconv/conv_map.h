/* conv_map.h -- unified format conversion across the full WuBuOffice matrix.
 *
 * Three canonical models are the interchange pivot:
 *   TEXT  : dm_doc        (word/text/presentation-body content)
 *   SHEET : wubucell_book
 *   SHOW  : wubushow_pres
 *
 * Every format implements READ->model and WRITE<-model, so any input format
 * can reach any output format. This file declares the single entry point that
 * the wubuoffice CLI dispatches to. */

#ifndef WUBUCONV_CONV_MAP_H
#define WUBUCONV_CONV_MAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convert <in_path> to <out_path>, choosing formats by extension.
 * Returns 0 on success, non-zero on unsupported combo / I/O error. */
int wubuconv_convert(const char *in_path, const char *out_path);

/* In-memory conversion (no filesystem round-trip). inext and outext are the
 * (synthetic) format tags. On success sets out/out_len to a malloc'd blob the
 * caller frees; returns 0, or non-zero on unsupported combo or parse error.
 * When inext is "json" the bytes are a model-JSON document (parsed back into
 * the canonical model), enabling AGI edit-then-create without temp files. */
int wubuconv_convert_mem(const uint8_t *data, size_t len,
                         const char *inext, const char *outext,
                         uint8_t **out, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCONV_CONV_MAP_H */
