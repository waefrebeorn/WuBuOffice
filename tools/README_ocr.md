# wubu OCR — lightweight C11 EMNIST Letters recognizer

A small, principled, **dependency-free** OCR stack for the EMNIST Letters
benchmark (26 classes, A–Z). Built as clean, self-contained C11 modules
with opaque structs — no monoliths, no god-headers.

## Design

```
glyph (28x28) ──▶ ZoningExtractor ──▶ feature vec (152) ──▶ MLP ──▶ 26 logits ──▶ softmax
                                   opaque               opaque (leaky ReLU)
```

- **`src/wubuocr/zoning.{h,c}`** — `ZoningExtractor`, fully opaque. Extracts
  an *interpretable* feature vector over a **fixed grid on the full 28×28
  canvas** (NOT the tight ink bounding box — stretching every glyph to fill the
  grid erases the spatial structure that distinguishes letters, which was the
  original collapse bug). Layout: `grid×grid` ink-fraction cells + aspect ratio
  + hole count (topological) + 4 quadrant ink fractions + center-of-mass (x,y).
  Feature dim = `grid*grid + 8` (e.g. grid=12 → 152).
- **`src/wubuocr/mlp.{h,c}`** — `MLP`, fully opaque. Feed-forward
  `z → h1(leakyReLU) → h2(leakyReLU) → scores → softmax`. Leaky ReLU
  (slope `MLP_LEAK=0.1`) keeps hidden units alive so the network can never
  die and freeze (the other original collapse bug). The module is **optimizer-
  agnostic**: it exposes gradient buffers + parameter-group accessors
  (`mlp_layer`), and the *caller* drives the update (WuBu Riemannian SGD,
  plain SGD, …). No optimizer code lives in the module.

## Tools

- **`tools/emnist_train.c`** — training driver. Loads EMNIST IDX, extracts
  zoning (or raw 28×28 pixels with `WUBUIX_RAW=1`), standardizes
  per-dimension (mean/std, std-guard), and trains with mini-batch SGD.
  - `WUBUIX_RAW=1` — use raw pixels (784-d) instead of zoning.
  - `WUBUIX_PLAIN=1` — plain SGD (default: WuBu Riemannian SGD optimizer
    from `../WuBuMath`, used in clipped Euclidean mode).
  - `WUBUIX_LR` (default 0.008), `WUBUIX_MOM` (default 0, momentum),
    `WUBUIX_CLIP` (gradient-clip norm), `WUBUIX_WD` (weight decay).
  - Args: `emnist_train <dir> [grid] [h1] [h2] [epochs] [train-cap] [test-cap]`.
  - Saves `<dir>/emnist_wubu_mlp_g<grid>_h<h1>_<h2>.wts`.
- **`tools/emnist_infer.c`** — dependency-free inference. Loads a `.wts`
  file, classifies the test set, prints per-class accuracy. No WuBu math needed.

## Build

```sh
bash tools/build_wubu_ocr.sh          # builds both tools against WuBuMath
# or, via CMake (the library + tools + unit tests are wired in):
cmake -S . -B build && cmake --build build
ctest --test-dir build
```

## Tests

- `tests/test_zoning.c` — zoning determinism, dim formula, distinct glyphs →
  distinct features, empty-image safety.
- `tests/test_mlp.c` — create/destroy, opaque accessors, forward determinism,
  softmax correctness, 2-class separability (proves gradient flow), save/load
  roundtrip.

## Why it was broken (and what fixed it)

1. **Label-count bug** — `load_idx` hardcoded the element size, so the 1-byte
   label files were mis-sized and the model trained on garbage labels.
2. **Bounding-box normalization** — stretching glyphs to fill the grid destroyed
   inter-letter spatial structure; features for different letters became near-
   identical. Switched to a fixed grid over the full canvas.
3. **Dead-network collapse** — plain ReLU let hidden units die and freeze
   weights. Replaced with leaky ReLU.
4. **LR divergence** — high LRs (and WuBu's momentum amplification) exploded
   the loss. Now mini-batch SGD with a stable LR + optional gradient clip.
