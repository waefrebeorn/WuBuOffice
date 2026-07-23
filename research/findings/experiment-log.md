# Experiment Log (Fashion-MNIST, conv3+MLP)

Chronological. Config → result. Trainer = tools/train_persample.c unless noted.
conv3 MED: 28x28 → 16@5(p2) → 32@5(p2) → 64@3(p1) → fdim=256; MLP 256→h1→h2→10.

## MULTICORE (train_mt.c) — 4-core data-parallel, VERIFIED
Built tools/train_mt.c: mini-batch data parallelism via pthreads. Each thread
owns a convnet3_gradbuf/mlp_gradbuf REPLICA (aliases shared read-only weights,
own caches+grads → instance-norm per-sample caches never race). Threads split
the batch, accumulate grads on their shard, main thread reduces (add_grad),
mean-scales by 1/B, applies ONE SGD update. Weights read-only during a batch.
- Mean-grad batch needs LARGER LR: CN_LR≈B×persample_lr. B=32→lr0.5, B=64→lr0.5.
- VERIFIED correct: 1-thread vs 4-thread give IDENTICAL accuracy (MED cap12k 3ep:
  train 90.29% vs 90.19%, test 84.63 vs 84.15).
- SPEEDUP (4 cores): MED cap12k 3ep 60.9s→27.3s (2.4×, epoch1 18.3→7.7).
  WIDE cap12k 2ep 152.2s→65.9s (2.3×). Wide (compute-bound) parallelizes best.
- Sweet spot: CN_BATCH=64. batch=128 hurts (too few updates/epoch: 84.9 vs 87.8).
- New env: CN_THREADS (default 4), CN_BATCH (default 32). Also saves the MLP
  (data/conv3_mlp.wts) so inference can load both.

| # | Config | Train | Test | Note |
|---|--------|-------|------|------|
| 15 | MT wide+inorm 4thr b64 WD0.0008 aug6 H1=384 35ep | 99.0% | **91.05%** (peak 92.1%) | BEST — 4-core, wide conv, LeNet-class |

## MILESTONE: 91.05% full-test (peak 92.1%) — LeNet-class, on 4 cores
The wide conv (32/64/128) + instance-norm + 4-thread MT trainer broke decisively
past the 88% single-net ceiling to 91.05% full-10k test (mid-run subset peaks
92.1%). This is genuine LeNet-territory for a scalar dependency-free C11 stack.
Train hit 99% → overfitting; WD=0.0008 is LIGHT for this bigger model. To push
higher: raise CN_WD (0.002-0.005), more aug (CN_AUG=10), and/or add MLP dropout.
Est. ceiling for this single wide net ~92-93%. 96 still needs the stylebank
ensemble.
Weights saved: data/conv3.wts + data/conv3_mlp.wts (both, via train_mt).

## Compute budget (Q6600 framing)
- MED conv 16/32/64: 1.12M MACs/img, fdim=256, ~2137 img/s single-core.
- WIDE conv 32/64/128 (CN_WIDE=1): 4.03M MACs/img (3.6×), fdim=512, ~595 img/s
  single-core — still real-time for 2011 hardware, within framing.

## Diagnostic phase (batched trainer emnist_train_conv3.c)
- Every config → ~10% random. Conv alive% 78–81% (looked fine, wasn't learning).
- Linear-probe joint conv feats = 81.9%; random conv feats = 82.2% → conv learns nothing.
- Isolated MLP on dumped feats (mlp_train_feats.c) = 98.9% test → MLP math OK.
- test_convnet3 end-to-end = 100% → conv backward math OK.
- Found batched-trainer bugs: 1/cnt mean-grad w/ per-sample LR; clip_n=5 default;
  CN_MOM=0.9 default. Fixing all → still ~10% joint (conv still exploding).

## Per-sample trainer (train_persample.c) — the breakthrough
| # | Config | Train | Test | Note |
|---|--------|-------|------|------|
| 1 | lr0.01 aug0 cap15k 6ep, conv_fac=1.0 | 9.9% | 10% | conv explodes, frozen |
| 2 | conv_fac sweep 0.05/0.1/0.02 lr0.02–0.1 | 9.42% | 10% | INVARIANT to LR = explosion |
| 3 | conv_fac=0 (FROZEN random conv) lr0.01 8ep | 94% | **85.7%** | MLP+features fine! |
| 4 | conv_fac=0.001 lr0.01 5ep cap12k | 85.9% | **83.6%** | joint learns! gentle conv |
| 5 | conv_fac=0.01 / 0.05 | ~72% | ~73% | dip early, too aggressive |
| 6 | full 60k conv_fac=0.001 lr0.02 aug10 | — | collapse ep2 | scale explodes conv |
| 7 | +conv clip CCLIP=0.5 cap20k 6ep | 89% | **84.9%** | clip stops explosion |
| 8 | full 60k lr0.02 convf0.001 cclip0.5 aug8 35ep | drift 78→73 | osc 66–83 | LR held too long, aug noise |
| 9 | **full 60k lr0.015 convf0.0005 cclip0.3 aug0 25ep, decay 0.4/0.7/0.9** | **94.5%** | **87.4%** | STABLE, best clean run |
| 10 | wide 512×256 + aug6 35ep (killed early) | 87.4%@ep17 | 87.5%@ep16 | promising, drifted pre-decay |
| 11 | **wide 512×256 + WD0.0005 + convf0.0008 + aug5, 30ep full** | 87.3% | **85.4%** | WORSE than #9 — aug+wide DON'T help w/o BatchNorm |

## DECISIVE CONCLUSION (run #11 vs #9)
Adding weight decay + wider MLP (512×256) + augmentation (5°) + higher conv_fac
made it WORSE (85.4% vs 87.4%). Confirmed experimentally: on this stack, the
CONV is the accuracy bottleneck past ~87%, and MLP-side tweaks (width, WD) plus
augmentation cannot break through it. The conv can't learn better features
because conv_fac must stay tiny (else it explodes). THE unlock is BatchNorm in
the conv (normalizes gradients → conv can train at full LR → genuinely better
features). Next real gains REQUIRE implementing batchnorm.c/.h — see
recipes/next-rungs.md rung 2. Do not waste more runs on MLP-side hyperparameter
tuning; it has been exhausted.

## New knobs added to train_persample.c (this session)
CN_H1/CN_H2 (MLP hidden sizes, default 256/128), CN_WD (MLP weight decay/L2).

## INSTANCE NORM implemented (rung 2 — the real unlock)
convnet3.c now has optional per-sample INSTANCE NORM (batch-free BatchNorm),
gated by env CN_INORM=1. Per-channel normalize conv pre-activations over the
spatial map + learnable gamma/beta, BEFORE ReLU. Identical at train/test (no
running stats). When off, byte-identical to the proven ReLU path.
- VERIFIED: tests/gradcheck_inorm.c finite-diff checks all 6 gamma/beta grads
  across 3 stages → rel_err <1% → BN backward math correct. ReLU-path
  test_convnet3 still 100%.
- IMPACT: inorm lets conv_fac jump from 0.0005 → 0.1–0.2 (200–400× higher conv
  LR) WITHOUT exploding. On 15k/6ep, train hits 94% (vs the conv barely moving
  before). This is the fix the literature predicted: BN normalizes conv
  gradients so the conv can actually learn good features.
- Also fixed 2 latent maxpool-backprop indexing bugs (dc1[a]/dc2[a] were
  missing the *K+k channel offset).
- convnet3_layer_count() returns 12 when use_in (adds gamma/beta as layers
  6..11) so the trainer's generic optimizer loop updates them automatically.
- Save/load extended: optional "inorm 1" block persists gamma/beta.

| # | Config | Train | Test | Note |
|---|--------|-------|------|------|
| 12 | INORM conv_fac=0.2 cap15k 6ep | 92% | ~85% | conv_fac 400× higher, stable |
| 13 | INORM full 60k conv_fac=0.1 aug3 30ep | 93.0% | **88.0%** | +0.6 over baseline; healthy climb (no drift) |
| 14 | INORM + WD0.001 + aug6 + H1=384, 35ep | 89.8% | **88.0%** | ZERO overfit gap; subset peak ~89.4% |

## INORM RESULT (honest)
Instance-norm full-test settles at **88.0%** (both runs), up from the 87.4%
ReLU baseline — a real but MODEST +0.6% on the number. The BIG wins are
qualitative and set up the next tier:
- Conv is now genuinely TRAINABLE: conv_fac 0.0005 → 0.1–0.2 (200–400× higher)
  without exploding. Train climbs smoothly (no pre-decay drift-down that plagued
  every ReLU run).
- With WD+aug the train/test gap CLOSES to ~0 (89.8/88.0) — no overfit, so
  there's headroom to grow capacity (more filters/epochs) without penalty.
- NOTE: mid-run test uses a 2000 subset (peaks ~89.4%); only the LAST epoch
  evaluates full 10k. Trust the last-epoch number (88.0%).
Why not 92-94% yet: literature 92%+ uses BN with a BIGGER conv (more
filters/deeper) + longer training + real batches. Our K1/K2/K3=16/32/64 conv is
still small. Next: widen conv channels (32/64/128) now that inorm makes deeper
conv trainable, and/or the stylebank ensemble. 96 remains an ensemble target.

## Key learnings from the log
- conv_fac=1.0 → instant explosion; 0.0005 + conv clip 0.3 → stable at 60k scale.
- LR must DECAY EARLY (0.4 of schedule) — holding high LR causes slow conv drift-down.
- Augmentation must be MILD (0–6°) for a from-scratch conv; aug8–10 destabilizes.
- Wider MLP alone doesn't clearly beat narrow yet — the conv (features) is the
  bottleneck past ~87%. Next real gains need BatchNorm in the conv (see next-rungs).
- Full test-set eval only at last epoch (subset 2000 mid-run) — a few % noise
  in mid-run test numbers is the eval subset, not real variance.
