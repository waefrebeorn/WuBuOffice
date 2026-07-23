# WuBuOCR — Conv+MLP Research Depot

Ultra-light, dependency-free C11 conv+MLP OCR/image classifier for a
4-core Q6600 Optiplex ("alternative reality where AI exists in 2011").
Scalar C11, no SIMD, no external libs (pthreads allowed). ~0.5–1M MACs/image.

This depot collects the **research, findings, recipes, and experiment log** that
took the stack from a session-long 10% random plateau to a working classifier.

## Directory map
- `README.md` .................. this file (index + quick recipe)
- `findings/root-cause.md` ..... the definitive root cause of the 10% plateau
- `findings/literature.md` ..... external references + extracted recipes
- `findings/experiment-log.md` . chronological run log (config → result)
- `recipes/winning-recipe.md` .. the current best-known training recipe
- `recipes/next-rungs.md` ...... research-backed paths toward higher accuracy

## TL;DR — current best
Per-sample SGD trainer `tools/train_persample.c` (NOT the batched one):
```
CN_LR=0.015 CN_CONVF=0.0005 CN_CCLIP=0.3 CN_AUG=0 CN_NORM=1 CN_EPOCHS=25
```
=> **87.4% test** (train 94.5%) on Fashion-MNIST, stable, no collapse.
Wide MLP (CN_H1=512 CN_H2=256) + light aug tracked ~87.5% and climbing.

## The one-paragraph story
The net sat at exactly ~10% (random) for the entire prior session. It was NOT
an architecture wall: a FROZEN conv (even random init) + MLP head already hits
85.7%. The real killer is that in JOINT training the **conv gradient explodes**
and destroys its own features on the first updates (frozen at exactly 9.42%,
invariant to LR). Fix = conv LR ~1000x smaller than the MLP (`CN_CONVF`) PLUS
per-layer conv gradient **clipping** (`CN_CCLIP`) to survive full-data scale,
using **per-sample SGD** with z-scored features. The batched trainer additionally
had 1/cnt mean-grad-with-per-sample-LR + clip=5 default bugs stacked on top.

## Accuracy ladder (Fashion-MNIST, this hardware framing)
| Tier | Test acc | What it takes |
|------|----------|---------------|
| random | 10% | (the bug) |
| frozen random conv + MLP | 85.7% | MLP head only |
| **conv+MLP joint (current)** | **87.4%** | conv_fac + conv clip + per-sample SGD |
| + wide MLP + light aug | ~88–90% | 512×256 head, aug 6–8° |
| + BatchNorm + dropout | ~92–94% | needs BN in the C conv (see next-rungs) |
| 96%+ | 96%+ | bigger conv / ensemble — beyond single Q6600 core |

## OCR document ingestion (new pivot — coordinate-aware)
The classifier above is one *recognizer* candidate. The full system is a
**coordinate-aware document ingestion** pipeline (already scaffolded in
`src/wubuocr/`): binarize -> XY-cut layout -> connected-components -> per-glyph
bounding boxes -> coordinate JSON. This session wired the two missing links and
verified it end-to-end on **64 real Windows fonts**:

- **DFT compression/analysis** (`src/wubuocr/dft.{h,c}`): 2D DFT per glyph crop,
  magnitude-sorted compression (~8x on glyphs >16px) + 6 spectral features.
- **Golden-ratio coordinate placement** (`src/wubuocr/goldplace.{h,c}`): reuses
  WuBuMath's `golden_subdivide`/`generate_phi_spiral` (byte-identical copy, no
  WuBuMath link) to place warped glyphs at golden coordinates.
- **End-to-end driver** (`tools/ocringest.c`): fonts -> golden warped page ->
  pipeline -> per-glyph coords + DFT features + golden-region tag as JSON.
- See `research/findings/ocr-ingestion.md` for the verified run + honest limits
  (geometry works; text is intentionally empty until the multi-font recognizer
  is trained; warped scatter pages fragment letters into >1 component, which is
  the stress condition, not a pipeline bug).
- **Multi-script corpus** (`tools/ocrcorpus.c`, `fonts/multiscript_active/`,
  `data/multiscript_corpus.jsonl`): **1,619 labeled glyphs across 10 scripts**
  (CJK/JP/TC/KR/Devanagari/Bengali/Tamil/Telugu/Thai/Arabic/Cyrillic/Latin).
  Full Noto TTFs converted CFF->glyf so wubufont (glyf-only) reads them. Per-script
  DFT spectral signatures are DISTINCT (cheap compression *characterizes* script).
  See `research/findings/multiscript-corpus.md`. `ocringest` mode=1 does the
  multi-script warped-page ingestion; `ocrcorpus` emits the training dataset.

## PARADIGM PIVOT (2026-07-19): per-glyph classifier -> CRNN + CTC
The 26-class conv+MLP net (and the stylebank voting ensemble) is a **per-GLYPH**
recognizer. It CANNOT output a word or a document -- it emits one fixed class per
28x28 input. The actual office-lens goal is **picture -> editable document in any
language**, which requires **sequence recognition**: a variable-width text-line
image -> a variable-length character sequence. The standard from-scratch
architecture (Graves 2006 CTC; Shi 2015 CRNN) is **CNN trunk + RNN (LSTM/GRU) +
CTC loss**. CTC is the key algorithm: it trains sequence recognition WITHOUT
per-character alignment labels (label a line image "HELLO", CTC finds where each
letter is). This fits the 2011 C11 / Q6600 framing (scalar, no deps) and reuses
`convnet3` as the feature extractor.
- The conv+MLP stack stays useful as (a) the CNN feature trunk inside CRNN, and
  (b) a *script-identification* classifier (which script before you decode).
- See `research/findings/ctc-crnn.md` for the algorithm notes + build order.
- **Two honest constraints:** (1) inorm is BROKEN (do not enable CN_INORM; see
  wubuocr-conv-pipeline skill) so train the CNN trunk WITHOUT inorm using the
  proven recipe; (2) a from-scratch 2011-C11 CRNN on a Q6600 will not match
  PaddleOCR/Tesseract accuracy (~90%+) -- realistically ~80-92% on clean printed
  Latin, less on messy scans / exotic scripts. That is the honest ceiling; the
  96% "north star" was set for the glyph net and is not the document-OCR metric.
