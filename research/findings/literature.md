# Literature & External References (Fashion-MNIST small-CNN)

Retrieved 2026-07-18. External data — recipes extracted, adapted to our C11 stack.

## Accuracy landscape (what's achievable at what cost)
- Baseline single-conv(32,3x3)→maxpool→dense(100)→softmax, SGD lr=0.01 mom=0.9,
  10 epochs: **~91.2%** (machinelearningmastery, 5-fold CV). Error <10% is "easy".
- dtoertei/fashion-mnist: **93.6%** test with **0.6M params** (deeper conv + tuning).
- timothylimyl/FASHION-MNIST: "91–93% is easy with ~200k params; 94% is hard."
- Keras CNN tutorials (mjbhobe, kaggle albertbrucelee): **94%** with BatchNorm +
  Dropout + augmentation, ~17 epochs. Overfits (99–100% train, 91–92% val) without
  regularization.
- Preprints 2026 "Lightweight CNN w/ Data Aug + BatchNorm": **92.44%** test.
- MDPI 2024 "State-of-the-Art" CNN-3-128 (3 conv layers, 128 filters): **99.65%**
  — but that is NOT lightweight (heavy conv, augmentation, long training).

### Takeaway for our Q6600 framing
- ~91% = single conv + dense, trivial.
- ~92–94% = + BatchNorm + Dropout + augmentation + more filters (still lightweight-ish).
- 96%+ = materially bigger conv / ensembles → beyond a single 2011 core. Our
  path to the low-90s is BatchNorm+Dropout; toward 96 is the 56+8 stylebank
  ensemble (many small experts voting), which fits the standing architecture.

## Standard rungs (best practices, all portable to scalar C11)
1. **He initialization** for ReLU layers (var = 2/fan_in). We already init conv
   biases +0.5 to avoid dead ReLU; add He-scaled weights.
2. **'same' padding** convolutions (we already pad; keep it).
3. **BatchNorm** after each conv (before ReLU). Biggest single lever past 91%.
   Stabilizes gradients — would ALSO have prevented our conv explosion natively.
4. **Dropout** (~40%) on the dense layer → kills the 99% train / 91% val overfit.
5. **Data augmentation**: small rotation/shift. We have rotation (CN_AUG deg).
   Keep it MILD (±6–8°); too much destabilizes a from-scratch conv.
6. **LR schedule**: warmup then decay. See warmup paper below.

## Key paper — LR Warmup (arXiv 2406.09405, "Why Warmup the Learning Rate?")
Warmup's main benefit: it lets the network **tolerate larger LRs** by easing in,
avoiding the early large-gradient instability that pushes weights into bad
regions. DIRECTLY relevant to our conv-explosion: instead of the fragile
conv_fac hack, a warmup on the conv LR (ramp 0 → target over the first ~1–3
epochs) is the principled fix — the conv gets past the fragile early phase, then
can train at a higher LR. Combine with per-layer grad clip for safety.

## Gradient-explosion diagnosis (StackOverflow / Medium consensus)
- Sudden train-acc drop after climbing = update steps too big/wrong → weights
  "messed up". Lower LR 10x, or clip gradient norm. (We saw exactly this:
  78%→collapse when conv_fac/LR too high on full data.)
- Exploding grads → NaN/constant output → accuracy pinned at majority-class rate.

## Source list
- machinelearningmastery.com/how-to-develop-a-cnn-from-scratch-for-fashion-mnist-clothing-classification/
- github.com/dtoertei/fashion-mnist (93.6% @ 0.6M params)
- github.com/timothylimyl/FASHION-MNIST
- arxiv.org/html/2406.09405v1 (LR warmup mechanisms)
- mdpi.com/2227-7390/12/20/3174 (SOTA CNN-3-128, 99.65%)
- preprints.org/manuscript/202605.1915 (lightweight BN+aug, 92.44%)
- github.com/zalandoresearch/fashion-mnist (dataset home + leaderboard)
