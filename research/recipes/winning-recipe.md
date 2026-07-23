# Winning Recipe (current best-known)

## Build
```
cd /home/wubu/WuBuOffice
cc -std=c11 -O2 -Isrc/wubuocr tools/train_persample.c \
   src/wubuocr/convnet3.c src/wubuocr/mlp.c -o build/train_persample -lm
```

## Run (87.4% test, Fashion-MNIST, ~25 min single core)
```
CN_TRAIN=fashion/train CN_TEST=fashion/t10k \
CN_CLASS=10 CN_LABOFF=0 \
CN_LR=0.015 CN_CONVF=0.0005 CN_CCLIP=0.3 \
CN_AUG=0 CN_NORM=1 CN_EPOCHS=25 \
./build/train_persample data
```

## What each knob does (and why)
- `CN_LR=0.015` — MLP learning rate (per-sample SGD). 0.01–0.02 is the sweet spot.
- `CN_CONVF=0.0005` — conv LR = CN_LR × this (~1000x smaller). THE fix for the
  conv-gradient explosion. Too high (>0.005) → conv destroys its own features.
- `CN_CCLIP=0.3` — per-layer conv gradient L2-norm clip. Required at 60k scale
  to stop the conv exploding over many updates/epoch. 0.3–0.5 works.
- `CN_AUG=0` — rotation augmentation in degrees. Keep MILD (0–6). From-scratch
  conv destabilizes at aug≥8.
- `CN_NORM=1` — z-score the conv features (stats from first 4000 samples) before
  the MLP. Chain-rule df/=zstd on the backward pass. Big stabilizer.
- `CN_EPOCHS=25` — LR decays at 0.4/0.7/0.9 of this (→ ×0.3, ×0.1, ×0.03).
- `CN_H1=256 CN_H2=128` — MLP hidden sizes (defaults). 512/256 also fine.

## Invariants that MUST hold (or you regress to 10%)
- Per-sample SGD (NOT the batched trainer's mean/sum gradient path).
- Momentum = 0 (no momentum knob here — per-sample plain SGD).
- Conv LR << MLP LR (conv_fac tiny) AND conv gradient clipped.
- Features z-scored with chain-rule df/=zstd.

## Verify the math still works (regression guard)
```
cc -std=c11 -O2 -Isrc/wubuocr tests/test_convnet3.c \
   src/wubuocr/convnet3.c src/wubuocr/mlp.c -o /tmp/tt3 -lm && stdbuf -oL /tmp/tt3
# expect: "conv3+MLP end-to-end on separable 2-class: acc=100.0%  PASS"
```

## Frozen-conv sanity (should hit ~85% fast, proves MLP+features healthy)
```
CN_LR=0.01 CN_CONVF=0 CN_AUG=0 CN_NORM=1 CN_EPOCHS=8 CN_SUBSET=15000 \
./build/train_persample data
```
