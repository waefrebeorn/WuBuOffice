#!/usr/bin/env bash
# fetch_font_corpus.sh -- build a local "huge open-font study corpus" for the
# WuBuOCR multi-font bank.
#
# The bank gets *robust* by studying MANY real font shapes (the user's
# "study many font types" idea). This pulls a large set of well-known
# OFL/Apache open-font families (Roboto, Inter, Noto, Source, Fira, JetBrains
# Mono, League, ...) as shallow per-family clones -- fast and huge on variety.
# Point the gauntlet at the result:
#     wubugauntlet_cli --fontdir fonts/study-corpus --compose
#     wubugauntlet_cli --fontdir fonts/study-corpus --latin
#     wubugauntlet_cli --fontdir fonts/study-corpus --unicode
#     wubugauntlet_cli --fontdir fonts/study-corpus --layout lines
#     wubugauntlet_cli --fontdir fonts/study-corpus --layout grid
#
# The corpus is git-ignored (see .gitignore): it is a local study artifact,
# never committed into WuBuOffice.
#
# Usage: tools/fetch_font_corpus.sh [TARGET_DIR] [MAX_PARALLEL]
set -euo pipefail
DIR="${1:-fonts/study-corpus}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$ROOT/tools/fetch_font_families.sh" "$DIR" "${2:-8}"
