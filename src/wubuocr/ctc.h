/* ctc.h -- Connectionist Temporal Classification (Graves 2006), scalar C11.
 * Forward-backward loss + gradient w.r.t. per-step logits. Verified against
 * finite differences (tools/ctc_test.c: 0/20 mismatches).
 *
 * Convention: alphabet has C classes; class 0 is the CTC BLANK. A target label
 * is a sequence of class indices in 1..C-1 with NO blanks and NO consecutive
 * repeats. The extended state sequence has length S = 2*L+1.
 */
#ifndef WUBUOCR_CTC_H
#define WUBUOCR_CTC_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Compute CTC loss and per-step gradient.
 *   T      : number of time steps (image width slices)
 *   C      : number of classes (incl. blank at index 0)
 *   L      : target length (number of non-blank labels)
 *   target : L class indices, each in [1, C-1]
 *   logits : T*C floats, row-major (step t, class c) — pre-softmax scores
 *   grad   : out, T*C floats, d(-log P)/d logits (same layout)
 *   smooth : label-smoothing epsilon in [0,1); 0 = off. Softens target dist,
 *            reducing overconfidence + repeat-collapse. Applied to grad target.
 * Returns the loss (-log P(target | logits)).
 */
float ctc_loss(int T, int C, int L, const int *target,
               const float *logits, float *grad, float smooth, float focal);

/* Greedy (best-path) CTC decode: argmax per step, collapse repeats, drop blanks.
 *   logits : T*C floats (pre-softmax; argmax is scale-invariant)
 *   out    : caller buffer, at least T ints; receives the decoded label sequence
 * Returns the decoded length (0..T).
 */
int ctc_greedy_decode(int T, int C, const float *logits, int *out);

/* Beam search CTC decode (fixed-size beam, no external LM). Each beam prefix
 * carries its score and last class so the "no repeat without blank" CTC rule is
 * honoured. Outperforms greedy on repeated/ambiguous glyphs.
 *   beam   : width (e.g. 8..16)
 *   out    : caller buffer, at least T ints
 * Returns the decoded length.
 */
int ctc_beam_decode(int T, int C, const float *logits, int beam, int *out);

#ifdef __cplusplus
}
#endif
#endif
