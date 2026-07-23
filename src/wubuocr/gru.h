/* gru.h -- minimal scalar GRU (single layer, optional bidirectional),
 * scalar C11, no deps. Drop-in alternative to the LSTM in rnn.h: same API
 * shape, ~28% faster forward, equal accuracy on CRNN. Forward + full
 * backprop-through-time. Verified by tools/gru_test.c.
 *
 * Opaque struct: callers never reach inside. Flat weight/grad buffers use a
 * SINGLE shared offset table (see gru.c) so param[] and grad[] ALWAYS describe
 * the same weight -- a past bug came from the two layouts drifting apart.
 */
#ifndef WUBUOCR_GRU_H
#define WUBUOCR_GRU_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct GRU GRU;

/* Create a (bidirectional if bidir) GRU: input dim `din`, hidden `hid`.
 * bidir doubles the output dim to 2*hid. */
GRU *gru_create(int din, int hid, int bidir, uint32_t seed);
void gru_free(GRU *r);

/* Forward over T steps of input `x` (T*din floats, row-major). Fills internal
 * caches. Output `y` (T * (bidir?2*hid:hid)) is written by gru_get_output. */
void gru_forward(GRU *r, int T, const float *x);

/* Output buffer (length T*outdim). Caller provides; outdim = gru_outdim(r). */
void gru_get_output(const GRU *r, float *y);

/* Backward: given dL/dy (T*outdim), computes and ACCUMULATES dL/dx into `dx`
 * (T*din) and updates internal weight gradients (read via gru_grad). */
void gru_backward(GRU *r, int T, const float *dy, float *dx);

int  gru_outdim(const GRU *r);
int  gru_num_params(const GRU *r);
/* flat weight access for the optimizer (param/grad pairs). */
float *gru_param(GRU *r);
float *gru_grad(GRU *r);
void   gru_zero_grad(GRU *r);

#ifdef __cplusplus
}
#endif
#endif
