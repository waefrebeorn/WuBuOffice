# Open-font study corpus

The WuBuOCR multi-font bank gets *robust* by studying **many** real font
shapes (the user's "study many font types" idea). The more fonts it votes
across, the more resilient recognition is to style variation. This directory
holds the tooling to assemble a **huge open-font repository** for that study.

## Fetch

```sh
tools/fetch_font_corpus.sh            # clones into fonts/study-corpus/
tools/fetch_font_corpus.sh <dir> [N]  # custom target / parallel clones
```

It clones a large set of well-known **OFL/Apache open-font families**
(Roboto, Inter, Noto, Source, Fira, JetBrains Mono, League, Ubuntu, Lora,
Merriweather, Montserrat, …) as **shallow per-family clones** — fast to pull
and huge on variety. (The monolithic `google/fonts` tree is also an option
but too large to fetch quickly, so the curated-family approach is the default
in `tools/fetch_font_families.sh`.) The corpus is git-ignored; it is a local
study artifact, never committed.

> OCR here is language-agnostic: each glyph is a shape mapped to a UTF-8
> codepoint *token* plus coordinates. No translation / language model lives in
> this layer (that is a separate neural-AI problem). Arabic and other
> connected / right-to-left scripts are intentionally omitted — they are not
> token-shaped (one glyph = one token) and belong to a different engine.
>
> The bank caps at `OCR_FONTBANK_MAX` (16) contributing fonts per build; the
> corpus simply gives the study far more *candidate* variety to vote across.
