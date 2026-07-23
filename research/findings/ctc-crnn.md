# CRNN + CTC — the real recognizer for document OCR (any language)

Status: design notes + build order. Nothing implemented yet (2026-07-19).

## Why this, not the per-glyph net
The `convnet3` + `mlp` stack is a **per-glyph classifier** (one fixed class per
28x28 input). It cannot emit a word or document. The office-lens goal is
**picture -> editable document in any language**, i.e. **sequence recognition**:
variable-width text-line image -> variable-length character string.

## Architecture (standard, from-scratch, C11-friendly)
Shi et al. 2015 "CRNN" + Graves et al. 2006 "CTC":
1. **CNN trunk** — reuse `convnet3` (3-stage conv) as the feature extractor.
   Input: a text-line image, height-normalized (e.g. 32px tall, variable width).
   Output: a feature map of width T (one column-slice per ~few pixels) x C channels.
   Each of the T steps is a feature vector for one horizontal position of the line.
2. **RNN** — a small bidirectional LSTM/GRU over the T steps (NEW module `rnn.c`:
   minimal LSTM cell, forward + BPTT-through-time, scalar C11, no deps). Maps each
   step to a per-class score distribution over the alphabet + a special CTC
   "blank" symbol.
3. **CTC loss + decode** — NEW module `ctc.c`:
   - Forward-backward algorithm over the extended label sequence (labels
     interleaved with blanks) to compute P(target string | image).
   - Loss = -log P. Gradient w.r.t. the RNN per-step scores (alpha/beta recursion).
   - Decode at inference by (a) taking argmax per step then (b) collapsing
     repeats + removing blanks (greedy), or beam search for better quality.

## CTC essentials (so we don't re-derive wrong)
- Alphabet L (e.g. Latin: a-z, A-Z, 0-9, space, punctuation). Add a BLANK symbol
  `'-'` not in L. Per-step network output = softmax over |L|+1 classes.
- A path is a length-T sequence over L ∪ {blank}. Two paths map to the same
  label string by: (1) merge consecutive identical non-blank chars, (2) delete
  all blanks. e.g. `--h-e-l-l-l-oo` -> `hello`.
- Forward var alpha[t][s]: total prob of paths of length t ending in state s that
  map to prefix of target. Blank and non-blank states interleave; transitions
  follow the standard 3-case recursion (blank->blank, char->same char only via
  blank, char->next char). Loss gradient = alpha*beta / P_total per state.
- Reference: Graves ICML 2006 "Connectionist Temporal Classification"; the
  ogunlao "Breaking down the CTC Loss" blog is a clean worked example.

## Build order (bite-sized; each step TDD with a C test)
1. **`ctc.c`** — forward-backward + loss + gradient. Unit test: known small cases
   (e.g. target "ab", T=4, hand-checked alpha/beta; symmetric-input loss = -log
   sum). This is the highest-risk math — prove it standalone FIRST.
2. **`rnn.c`** — 1-layer bidirectional LSTM. Unit test: gradcheck the cell
   (finite-diff vs analytic) and a trivial sequence-XOR task to confirm BPTT works.
3. **`crnn.c`** — glue: convnet3 (trunk) -> rnn -> per-step logits; exposes
   forward (logits) + backward (gradients into rnn + conv). Reuse existing
   `convnet3_backward` and `convnet3_add_grad`/`gradbuf` for threaded training.
4. **Synthetic line-data generator** — render text strings (from a corpus /
   font) into line images with warps (reuse `page_compose` / `gauntlet` warps).
   CTC needs ONLY the string label, no per-glyph boxes -> cheap to generate at
   scale. Start Latin; the SAME pipeline later swaps in any script's font.
5. **`tools/crnn_train.c`** — end-to-end CRNN+CTC trainer (per-sample or small
   batch; reuse the proven no-inorm conv recipe for the trunk LR scaling).
6. **Decode + wire into `OcrRecognizer`** — line image -> string. Feed recognized
   lines (in reading order from the existing XY-cut layout) into the document
   model -> docx/odt/md via `wubuconv`.

## Honest accuracy expectations (2011 C11 / Q6600 framing)
- A from-scratch CRNN will NOT match PaddleOCR/Tesseract/EasyOCR (~90%+ on Latin).
  Realistic: ~80-92% character accuracy on clean printed Latin, lower on
  handwritten / noisy scans / exotic scripts.
- "All languages" = one CRNN per script family (share the trunk code, swap the
  alphabet + training font), plus a lightweight *script-ID* classifier (the
  existing conv+MLP, retrained on page/script crops) to pick which decoder to use.
- This is a long build. The CTC module (step 1) is the critical-path risk and
  should be proven before any training is attempted.
