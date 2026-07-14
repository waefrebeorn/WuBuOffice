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

#ifdef __cplusplus
extern "C" {
#endif

/* Convert <in_path> to <out_path>, choosing formats by extension.
 * Returns 0 on success, non-zero on unsupported combo / I/O error. */
int wubuconv_convert(const char *in_path, const char *out_path);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCONV_CONV_MAP_H */
