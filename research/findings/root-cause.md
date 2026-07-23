# Root Cause — the session-long 10% plateau

## Symptom
Joint conv+MLP training on Fashion-MNIST pinned at ~10% (random) across EVERY
config tried over a full session: momentum on/off, clip on/off, LR high/low,
mean/sum gradient, frozen/joint, 2-stage/3-stage, SGD/Adam/WuBu-natgrad. Conv
"alive%" looked healthy (~78–81%). Everyone kept blaming the conv.

## The decisive experiments (via clean per-sample trainer train_persample.c)
1. **Linear-probe the conv features**: joint-trained conv features are 82%
   linearly separable — SAME as random conv (82.2%). => conv learns nothing in
   joint mode BUT the features are perfectly usable.
2. **Frozen conv (even RANDOM init) + MLP head = 85.7% test** in 8 epochs.
   => The MLP path and the features were fine the whole time. Fashion is
   separable even from random-conv features. NOT an architecture wall.
3. **Isolated MLP on dumped trained-conv features = 98.9% test** at per-sample
   lr 0.005. => The MLP math (mlp.c) is correct.
4. **test_convnet3 end-to-end = 100%** on a separable toy. => conv backward
   math is correct too.
5. **Joint at conv_fac=1.0 (conv LR == MLP LR) = frozen at EXACTLY 9.42%,
   INVARIANT to LR** (0.02, 0.05, 0.1 all identical). Invariance-to-LR is the
   tell: the conv is being blown out on the first few updates, producing a
   constant output => MLP always predicts one class => 9.42%.

## ROOT CAUSE
In joint training the **conv gradient is far too large** relative to the MLP.
The conv's per-sample gradient (backprop through maxpool + 3 conv stages) has
big magnitude; applying it at the same LR as the MLP destroys the conv filters
immediately. The features collapse to a constant → no learning, forever.

## THE FIX (three parts, all needed at full-data scale)
1. **conv_fac ≈ 0.0005–0.001** — conv LR = mlp_lr × conv_fac (~1000x smaller).
2. **per-layer conv gradient clipping, CN_CCLIP ≈ 0.3** — caps the conv grad
   L2 norm per layer. WITHOUT this, 60k updates/epoch drift ~3x faster than
   the 20k sanity run and the conv still explodes by ep2. WITH it: stable.
3. **per-sample SGD** (train_persample.c), z-scored features, chain-rule
   df/=zstd, staged LR decay (0.4/0.7/0.9 of schedule).

## Secondary bugs in the BATCHED trainer (emnist_train_conv3.c)
These stacked on top and guaranteed 10% independently:
- **1/cnt mean-grad with a per-sample LR** → effective step ~batch-size (256–512x)
  too small → ~2e-8/weight/step → no learning. Either drop the /cnt (per-sample)
  or scale LR by batch size (mean-grad LR ~0.5–1.0).
- **clip_n defaulted to 5.0** → shrank the MLP grad another ~60x. Now default off.
- **CN_MOM defaulted to 0.9** → momentum accumulates early conv noise, kills conv.
  Always CN_MOM=0.

## Misdiagnoses to NOT revive
"conv death (dead ReLU)", "slow-conv cure", "moving-target", "MLP-LR-explosion",
"batch-scaling alone", "depth/architecture wall". Each was a partial view. The
full picture: MLP + features are fine; the conv gradient is too large in joint
training; fix with conv_fac + conv clip + per-sample SGD.

## Fast diagnostic sequence for the next stuck-at-random net
1. Linear-probe conv features (>80% separable ⇒ optimization, not architecture).
2. Frozen-conv + MLP-only (if it learns ⇒ the conv-update path is the problem).
3. conv_fac sweep + conv grad clip (find the LR where the conv stops exploding).
4. Grad-flow print (weights drift? gradL2 size?) to catch scaling bugs.
