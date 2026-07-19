/* stylebank.h -- 64MB multi-style conv expert bank (2011 / Q6600 framing).
 *
 * A "style" is one conv3 (CONV_MED) + MLP head (~515KB weights, ~1MB with
 * grads+caches -- cache-sized). The bank holds 64 slots split:
 *   56 BEST  : persistent champions, read-mostly, instantly swappable
 *              (load/save one 1MB block by slot index = "upgrade").
 *    8 ROLLING: hot working set, trained on gradients each round, lives in
 *              cache; the best performer periodically PROMOTES into BEST
 *              (evicting the weakest BEST) -- that is the instant upgrade.
 *
 * Inference = ensemble (average logits) over the active slots. Router is a
 * later upgrade. All plain C11, no deps, single-core scalar + the threaded
 * trainer already parallelises per-sample work.
 *
 * Memory budget: 64 slots * ~1MB = 64MB resident. Fits the target box.
 */
#ifndef WUBUOCR_STYLEBANK_H
#define WUBUOCR_STYLEBANK_H

#include <stdint.h>
#include "convnet3.h"
#include "mlp.h"

typedef enum { SLOT_BEST=0, SLOT_ROLLING=1 } SlotKind;

typedef struct StyleBank StyleBank;   /* opaque */

/* nbest + nroll must sum to the slot count (e.g. 56 + 8 = 64). */
StyleBank *stylebank_create(int nbest, int nroll, const ConvConfig3 *cfg,
                            int mlp_h1, int mlp_h2, int nclass);
void       stylebank_destroy(StyleBank *b);

int  stylebank_nslots(const StyleBank *b);
int  stylebank_nbest(const StyleBank *b);
int  stylebank_nrolling(const StyleBank *b);
/* slot index in [0, nslots); first nbest are BEST, rest are ROLLING. */
SlotKind stylebank_slot_kind(const StyleBank *b, int slot);

/* Instant upgrade: load a saved style into a slot by index (0..nslots-1).
 * For BEST slots this is pure read-mostly swap; for ROLLING it hot-loads.
 * Returns 0 on success, -1 on error. */
int stylebank_load_style(StyleBank *b, int slot,
                         const char *conv_path, const char *mlp_path);

/* Persist one slot to disk (used by promotion / export / upgrade). */
int stylebank_save_slot(const StyleBank *b, int slot,
                        const char *conv_path, const char *mlp_path);

/* Ensemble forward over ALL active slots: out_scores[K] = mean of per-slot
 * logits. Caller picks argmax. Returns 0 on success. */
int stylebank_forward(const StyleBank *b, const float *img, float *out_scores);

/* Per-slot single forward (no ensemble) -- for eval / routing. */
int stylebank_slot_forward(const StyleBank *b, int slot, const float *img,
                           float *out_scores);

/* Train one ROLLING slot on a data shard for `epochs` (reuses the conv3+mlp
 * forward/backward; single-threaded for the bank's slot loop, the per-sample
 * work is the same proven math). data_dir holds <stem>-train/test IDX.
 * Returns 0 on success. CN_* env still tunes LR/opt/aug for that slot. */
int stylebank_train_slot(StyleBank *b, int slot, const char *data_dir,
                         const char *stem, int epochs, long cap);

/* Evaluate one slot's test accuracy (for promotion decisions). */
float stylebank_slot_acc(const StyleBank *b, int slot,
                         const char *data_dir, const char *stem, int label_off);

/* Promote: find the ROLLING slot with the best held-out acc, swap it into
 * the BEST region, evicting the weakest BEST slot (by last-known acc).
 * Returns the promoted slot index, or -1 if no promotion happened. */
int stylebank_promote(StyleBank *b, const char *data_dir, const char *stem,
                      int label_off);

#endif
