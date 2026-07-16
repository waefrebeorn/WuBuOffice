#!/usr/bin/env bash
# fetch_emnist.sh -- download the EMNIST Letters benchmark into data/emnist/.
# Lightweight, reliable, text-only (A-Z, 26 classes, 28x28 grayscale) OCR dataset
# used to TRAIN + EVALUATE the WuBuOCR zoning recognizer. ~62 MB total.
#
# Source: Hugging Face mirror (Royc30ne/emnist-letters), which serves the
# canonical NIST EMNIST Letters IDX files. Files are git-ignored.
#
# Usage: tools/fetch_emnist.sh [TARGET_DIR]
set -euo pipefail
TARGET="${1:-data/emnist}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/$TARGET"
mkdir -p "$DEST"

BASE="https://huggingface.co/datasets/Royc30ne/emnist-letters/resolve/main"
FILES=( emnist-letters-train-images-idx3-ubyte.gz
        emnist-letters-train-labels-idx1-ubyte.gz
        emnist-letters-test-images-idx3-ubyte.gz
        emnist-letters-test-labels-idx1-ubyte.gz )

for f in "${FILES[@]}"; do
  if [ -f "$DEST/${f%.gz}" ]; then echo "  already have ${f%.gz}"; continue; fi
  echo "  get $f ..."
  curl -sL --max-time 180 -o "$DEST/$f" "$BASE/$f"
  gunzip -f "$DEST/$f"
done

echo "== EMNIST Letters ready in $DEST =="
echo "== Train (evaluate): tools/emnist_eval data/emnist <GRID> =="
