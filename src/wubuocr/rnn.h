/* rnn.h -- minimal scalar LSTM (single layer, optional bidirectional),
 * scalar C11, no deps. Used as the sequence model inside CRNN. Exposes forward
 * and a full backprop-through-time step. Verified by tools/rnn_test.c.
 */
#ifndef WUBUOCR_RNN_H
#define WUBUOCR_RNN_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct LSTM LSTM;

/* Create a (bidirectional if bidir) LSTM: input dim `din`, hidden `hid`.
 * bidir doubles the output dim to 2*hid. */
LSTM *lstm_create(int din, int hid, int bidir, uint32_t seed);
void lstm_free(LSTM *r);

/* Forward over T steps of input `x` (T*din floats, row-major). Fills internal
 * caches. Output `y` (T * (bidir?2*hid:hid)) is written by lstm_get_output. */
void lstm_forward(LSTM *r, int T, const float *x);

/* Output buffer (length T*outdim). Caller provides; outdim = lstm_outdim(r). */
void lstm_get_output(const LSTM *r, float *y);

/* Backward: given dL/dy (T*outdim), computes and ACCUMULATES dL/dx into `dx`
 * (T*din) and updates internal weight gradients (read via lstm_grad_*). */
void lstm_backward(LSTM *r, int T, const float *dy, float *dx);

int  lstm_outdim(const LSTM *r);
int  lstm_num_params(const LSTM *r);
/* flat weight access for the optimizer (param/n_grad pairs) */
float *lstm_param(LSTM *r);
float *lstm_grad(LSTM *r);
void   lstm_zero_grad(LSTM *r);

/* NOTE: GRU is a separate self-contained module (src/wubuocr/gru.h + gru.c).
 * crnn.c includes gru.h and selects between LSTM (default) and GRU
 * (RNN_TYPE=2) via the generic seq_* wrappers. */

#ifdef __cplusplus
}
#endif
#endif
