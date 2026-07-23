/* crnn.h -- CRNN recognizer: conv feature trunk -> bidirectional LSTM -> CTC.
 *
 * A line image (H x W grayscale) is split into T vertical strips of equal
 * width (T ~ W/strip_w). Each strip is fed to the shared conv trunk
 * (convnet3) producing a D-dim feature vector; the T strips form a sequence
 * fed to a bidirectional LSTM; its (T x C) outputs are logits for CTC over C
 * classes (class 0 = CTC blank). Trained end-to-end by crnn_train.c.
 *
 * C11, no deps. The conv trunk, LSTM and CTC are each individually
 * gradient-verified (tools/rnn_test.c, tools/ctc_test.c).
 */
#ifndef WUBUOCR_CRNN_H
#define WUBUOCR_CRNN_H
#include "image.h"
#include "convnet3.h"
#include "rnn.h"
#include "ctc.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct CRNN CRNN;

/* Build a CRNN. conv_cfg describes the per-strip trunk (strips are square-ish
 * of side `strip` pixels). lstm_hid = RNN hidden size. nclass = CTC alphabet
 * size (incl. blank at 0). bidir=1 recommended. */
CRNN *crnn_create(const ConvConfig3 *conv_cfg, int strip, int lstm_hid, int nclass, int bidir, uint32_t seed);
void  crnn_free(CRNN *m);
int   crnn_time_steps(const CRNN *m, int img_w);   /* T for a given image width */
int   crnn_feat_dim(const CRNN *m);              /* D (conv feature length) */
void  crnn_set_freeze_conv(CRNN *m, int freeze); /* 1 = don't train conv trunk */
/* Set strip stride in px (<= strip width). Smaller stride = overlapping strips =
 * more time steps = robustness to non-grid-aligned (warped) glyphs. Default=strip. */
void  crnn_set_stride(CRNN *m, int stride);
int   crnn_get_stride(const CRNN *m);

/* Run inference on a line image: forward -> greedy CTC decode.
 * out: caller buffer of at least crnn_time_steps(m,width) ints. Returns length. */
int   crnn_predict(CRNN *m, const OcrImage *img, int *out);

/* Line-level recognizer: image -> UTF-8 string. `charset` maps class index k
 * (1..C-1) to charset[k-1]; blank(0) is skipped. Writes a NUL-terminated string
 * into `out` (capacity `cap`) and returns its length. This is the real product
 * API — a CRNN is a per-LINE sequence recognizer, not a per-glyph classifier,
 * so it plugs in at the block/line level, NOT the per-glyph OcrRecognizer slot. */
int  crnn_recognize(CRNN *m, const OcrImage *line, const char *charset,
                     char *out, int cap); /* ASCII-only (legacy); prefer _utf8 */
/* UTF-8-aware recognition: cp_of_class(cls,u) maps a CRNN class (0=blank) to a
 * codepoint, returning 0 to skip. Multibyte-safe. Use for any non-ASCII script. */
int  crnn_recognize_utf8(CRNN *m, const OcrImage *line,
                         uint32_t (*cp_of_class)(int cls, void *u), void *u,
                         char *out, int cap);

/* Persist / restore a trained model (architecture + all weights) to one binary
 * file. Returns 1 on success, 0 on failure. crnn_load allocates a new CRNN
 * (caller frees with crnn_free); *out set to NULL on failure. */
int   crnn_save(const CRNN *m, const char *path);
int   crnn_load(const char *path, CRNN **out);

/* Forward over a line image. Fills *logits (T*C) and returns T. Uses scratch
 * buffers internal to the model. */
int   crnn_forward_img(CRNN *m, const OcrImage *img, float *logits);

/* Forward over a caller-supplied sequence of T feature vectors (T*D floats).
 * Fills *logits (T*C). Returns T. */
int   crnn_forward_seq(CRNN *m, int T, const float *seq_feats, float *logits);

/* Full gradient step. Given a sequence (T feature vecs), target (L classes in
 * 1..C-1), performs forward + CTC backward + LSTM backward + conv backward and
 * accumulates grads. Returns the CTC loss. Caller then calls crnn_step().
 * If img!=NULL, seq_feats is ignored and the image is sliced internally. */
float crnn_train_step(CRNN *m, int T, const float *seq_feats, int L, const int *target,
                      const OcrImage *img);

/* Adam update with accumulated grads (call after one or more train_step calls).
 * lr, b1=0.9, b2=0.999, eps=1e-8. t = optimizer step counter (1-based). */
void  crnn_adam(CRNN *m, float lr, int t);

#ifdef __cplusplus
}
#endif
#endif
