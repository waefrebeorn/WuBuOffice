# Next Rungs — research-backed paths to higher accuracy

Current: 87.4% test. Each rung below is ordered by expected gain / effort, with
the literature backing and how it maps to our scalar C11 stack + 64MB 56+8 bank.

## Rung 1 — LR warmup on the conv (→ ~88–89%, cheap, principled)
Backing: arXiv 2406.09405. Warmup lets the net tolerate a larger conv LR by
easing past the fragile early phase, instead of permanently crippling the conv
with a tiny conv_fac. Implementation: ramp conv effective LR 0 → target over the
first ~1.5 epochs, then hold, then decay. Lets us RAISE conv_fac (e.g. 0.005)
after warmup so the conv actually learns better features, not just survives.
Keep the per-layer conv grad clip as the safety net.

## Rung 2 — BatchNorm after each conv stage (→ ~92–94%, biggest lever)
Backing: every 92%+ Fashion recipe (Keras tutorials, Preprints 2026 92.44%,
dtoertei 93.6%). BN normalizes conv pre-activations → stable gradients → would
have NATIVELY prevented our conv explosion (no conv_fac hack needed) AND lets the
conv train at full LR → genuinely better features. This is THE step past 91%.
C11 implementation (scalar, cache-friendly):
- Per-channel running mean/var (EMA during train, frozen at test).
- y = gamma * (x - mean)/sqrt(var+eps) + beta, learnable gamma/beta per channel.
- Backward: standard BN grad (dx, dgamma, dbeta). ~1 new module batchnorm.c/.h
  with an opaque struct, one instance per conv stage (16, 32, 64 channels).
- Cost: tiny (3 stages × channels params) — fits the 1MB/style budget easily.
NOTE: BN wants a batch. Options: (a) per-sample "instance"/running-stat BN
(simplest, works ok on 28x28), or (b) small mini-batches in train_persample.

## Rung 3 — Dropout on the MLP dense layer (→ +0.5–1%, kills overfit)
Backing: mjbhobe/kaggle (99% train vs 91% val without it → 94% with dropout+BN).
We already see 94.5% train / 87.4% test = overfitting. Add ~40% dropout between
MLP hidden layers (train-time mask, scale at test). ~10 lines in mlp.c forward.

## Rung 4 — He init + mild augmentation tuned (→ +0.5–1%)
He-scaled conv/MLP weights (var=2/fan_in) instead of current init. Add small
random shift (±2px) alongside the ±4–6° rotation. Both cheap, both standard.

## Rung 5 — 56+8 Stylebank ENSEMBLE (→ low-90s, fits standing architecture)
Backing: ensembles routinely add 1–3% over a single small net; matches the
user's own 64MB 56-BEST + 8-ROLLING expert-bank spec (~1MB/expert, Q6600 cache).
Train N diverse conv+MLP experts (different seeds/augmentation/subsets), store
each as a 1MB block, vote (avg softmax) at inference. Each expert is already
~515KB. This is the intended route to the low-90s WITHOUT a bigger single conv,
staying inside the 2011/Q6600 compute framing (experts run/serve one at a time,
or vote cheaply since each is <1M MACs).

## The 96% reality check
96%+ on Fashion-MNIST needs either (a) a materially larger single conv (more
channels/layers, heavy aug, long training — breaks the single-Q6600-core
framing), or (b) a strong ensemble of the above rungs. Realistic in-framing
ceiling for ONE lightweight expert is ~93–94% (BN+dropout+aug). 96 is an
ensemble/stylebank target, not a single-net target at this compute budget.

## Recommended execution order
1. BatchNorm (rung 2) — unlocks the most and removes the conv_fac fragility.
2. Dropout (rung 3) — closes the overfit gap.
3. He init + aug tune (rung 4).
4. Warmup (rung 1) — lets conv LR rise safely with BN in place.
5. Stylebank ensemble (rung 5) — stack experts toward the low-90s / 96 target.
