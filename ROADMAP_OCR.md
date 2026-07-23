# WuBuOffice — OCR / Office-Lens Roadmap

**Goal:** take a picture of a document in *any language* → an editable document
(docx / odt / md / html / json) inside WuBuOffice. This is the "office lens"
feature. Dependency-free C11, 2011 framing (scalar, no SIMD, no ML libs,
Q6600-class hardware).

This roadmap supersedes the old per-glyph-classifier framing. The conv+MLP net is
now a *component* (CNN feature trunk + script-ID), not the product.

## Pipeline (end to end)
```
photo → deskew/perspective-correct        (lens step — NOT YET BUILT)
      → binarize (Otsu)                   [DONE: binarize.c]
      → layout / reading-order (XY-cut)  [DONE: layout.c]
      → line & word segmentation          [PARTIAL: components.c, block_text]
      → script identification             [PLANNED: conv+MLP retrained]
      → text-line recognition (CRNN+CTC)  [PLANNED: crnn.c + ctc.c + rnn.c]
      → document model → wubuconv         [DONE: emit docx/odt/md/...]
```

## Phases

### Phase 0 — Correct the foundation (DONE 2026-07-19)
- Fixed training bugs: LR=0 in epoch-0 (runstep increment placement + warmup
  floor), freeze-flag value parsing, `use_cos` always-on.
- **Instance norm is BROKEN** (NaN in training; upstream grads ~150000× wrong).
  Disabled. Train the conv trunk WITHOUT inorm using the proven recipe.
- Conv+MLP math verified correct (gradchecks + descent probes): it learns a
  per-glyph task, but that is not the product.

### Phase 1 — CRNN + CTC (the real recognizer)  ← current priority
Build order (each TDD with a C test; see `research/findings/ctc-crnn.md`):
1. `ctc.c` — CTC forward-backward + loss + gradient. **Prove standalone first**
   (highest-risk math). Validate: `tools/ctc_test.c`.
2. `rnn.c` — minimal bidirectional LSTM (forward + BPTT). Gradcheck the cell.
3. `crnn.c` — `convnet3` trunk → `rnn` → per-step logits; forward + backward;
   reuse `convnet3_backward` + `gradbuf`/`add_grad` for threaded training.
4. Synthetic line-data generator — render strings to warped line images (reuse
   `page_compose`/`gauntlet` warps). CTC needs only the string label. Start Latin.
5. `tools/crnn_train.c` — end-to-end CRNN+CTC; reuse proven no-inorm conv LR
   scaling (conv LR ~1000× < RNN LR, per-layer clip).
6. Greedy + beam decode → string.

### Phase 2 — All-language coverage
- One CRNN per script family (share trunk code; swap alphabet + font). Latin
  first; then Cyrillic, Arabic, Devanagari, CJK, etc.
- Lightweight *script-ID* classifier (retrain conv+MLP on page/script crops)
  routes each line to the right decoder.
- Leverage existing multi-script corpus (`tools/ocrcorpus.c`,
  `fonts/multiscript_active/`, `data/multiscript_corpus.jsonl` — 1,619 glyphs
  across 12 scripts; CFF→glyf converted for wubufont).

### Phase 3 — Lens + document assembly
- Deskew / perspective correction (currently out of scope; geometry pipeline
  assumes roughly upright pages).
- Feed recognized lines (in XY-cut reading order) into the document model via
  `wubuconv` → docx/odt/md. Preserve multi-column order and table structure
  (pain points #1/#3 from `FORMATS_OCR.md`).

## Honest accuracy expectations
- From-scratch 2011-C11 CRNN on Q6600-class HW will NOT match
  PaddleOCR/Tesseract/EasyOCR (~90%+ Latin). Realistic: ~80–92% character acc on
  clean printed Latin, lower on handwritten / noisy / exotic scripts.
- "All languages" is incremental: ship Latin CRNN first, then add scripts. The
  96% "north star" was for the glyph net and is NOT the document-OCR metric.

## Non-goals
- Wrapping an existing engine (PaddleOCR/Tesseract) behind the UI — contradicts
  the from-scratch ethos; only if the user explicitly chooses (see decision below).
- VLM / transformer-based OCR (TrOCR, Nemotron-OCR) — too heavy for the 2011
  framing; the structural pipeline is the clean-room reimplementable part.

## Open decisions (user)
- Scope v1: Latin-only first (fastest demo) vs multi-script from day one.
- Build target: CRNN+CTC in our C11 engine (chosen) vs wrap existing engine.
